/* Board bring-up for the Waveshare ESP32-S3-Touch-AMOLED-1.75, see
 * board_amoled.h: the CO5300 AMOLED over QSPI, LVGL, brightness, the
 * CST9217 touch, and the beep through the ES8311 codec and the speaker.
 * The same API as the 2.1" LCD's hw.c (../notstock-round/main/hw.h).
 *
 * The panel has no frame buffer the ESP32 can reach: LVGL renders strips
 * that go out over QSPI, two at a time in flight. RGB565 travels high byte
 * first, so each strip is byte-swapped on the way. The panel takes whole
 * pixel pairs: areas start on even and end on odd coordinates.
 */
#include "hw.h"
#if defined(BOARD_A132)
#include "board_a132.h"        /* the 1.32, ../notstock-a4 */
#else
#include "board_amoled.h"
#endif
#include "boot_fb.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "hw";

#define LVGL_BUF_LINES 40         /* even, see above */
#define LVGL_TICK_MS   2
#define PUSH_LINES     40         /* boot frames go out in strips this high */

static SemaphoreHandle_t s_i2c;   /* touch, codec and IMU share the bus */

/* ------------------------------------------------------------------ I2C */
static void i2c_init(void)
{
    i2c_config_t c = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &c));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, c.mode, 0, 0, 0));
    s_i2c = xSemaphoreCreateMutex();
}

static esp_err_t i2c_write(uint8_t addr, const uint8_t *d, size_t n)
{
    xSemaphoreTake(s_i2c, portMAX_DELAY);
    esp_err_t e = i2c_master_write_to_device(I2C_NUM_0, addr, d, n,
                                             pdMS_TO_TICKS(50));
    xSemaphoreGive(s_i2c);
    return e;
}

static esp_err_t i2c_write_read(uint8_t addr, const uint8_t *w, size_t wn,
                                uint8_t *r, size_t rn)
{
    xSemaphoreTake(s_i2c, portMAX_DELAY);
    esp_err_t e = i2c_master_write_read_device(I2C_NUM_0, addr, w, wn, r, rn,
                                               pdMS_TO_TICKS(50));
    xSemaphoreGive(s_i2c);
    return e;
}

/* for motion_amoled.c: the same bus, the same lock */
esp_err_t hw_i2c_write(uint8_t addr, const uint8_t *d, size_t n)
{
    return i2c_write(addr, d, n);
}

esp_err_t hw_i2c_write_read(uint8_t addr, const uint8_t *w, size_t wn,
                            uint8_t *r, size_t rn)
{
    return i2c_write_read(addr, w, wn, r, rn);
}

/* Logs what answers: tells a wiring problem from a driver problem. On this
 * board: 0x18 ES8311, 0x20 TCA9554, 0x34 AXP2101, 0x40 ES7210, 0x51 RTC,
 * 0x5A touch, 0x6B IMU. */
static void i2c_scan(void)
{
    char line[128];
    int len = snprintf(line, sizeof line, "i2c devices:");
    for (int a = 0x08; a < 0x78; a++) {
        i2c_cmd_handle_t c = i2c_cmd_link_create();
        i2c_master_start(c);
        i2c_master_write_byte(c, (uint8_t)((a << 1) | I2C_MASTER_WRITE), true);
        i2c_master_stop(c);
        xSemaphoreTake(s_i2c, portMAX_DELAY);
        esp_err_t e = i2c_master_cmd_begin(I2C_NUM_0, c, pdMS_TO_TICKS(20));
        xSemaphoreGive(s_i2c);
        i2c_cmd_link_delete(c);
        if (e == ESP_OK && len < (int)sizeof line - 8) {
            len += snprintf(line + len, sizeof line - len, " 0x%02X", a);
        }
    }
    ESP_LOGI(TAG, "%s", line);
}

/* ---------------------------------------------------------------- panel */
/* QSPI framing of the CO5300: a 32-bit header, opcode and the command in
 * the middle; 0x02 sends a command with its parameters, 0x32 pixels */
#define OP_CMD   0x02
#define OP_PIXEL 0x32

static esp_lcd_panel_io_handle_t s_io;
static SemaphoreHandle_t s_push_done;     /* a boot strip went out */
static lv_disp_drv_t *volatile s_flushing; /* LVGL waits for this flush */

static void lcd_cmd(uint8_t cmd, const uint8_t *p, size_t n)
{
    esp_lcd_panel_io_tx_param(s_io, (OP_CMD << 24) | (cmd << 8), p, n);
}

/* inclusive; the panel's 466 visible columns start at LCD_X_GAP */
static void lcd_window(int x1, int y1, int x2, int y2)
{
    x1 += LCD_X_GAP;
    x2 += LCD_X_GAP;
    const uint8_t c[4] = { x1 >> 8, x1 & 0xFF, x2 >> 8, x2 & 0xFF };
    const uint8_t r[4] = { y1 >> 8, y1 & 0xFF, y2 >> 8, y2 & 0xFF };
    /* a command waits for the pixels still going out, so the strip before
     * is done when this returns */
    lcd_cmd(0x2A, c, 4);
    lcd_cmd(0x2B, r, 4);
}

static void lcd_pixels(const void *px, size_t bytes)
{
    esp_lcd_panel_io_tx_color(s_io, (OP_PIXEL << 24) | (0x2C << 8), px, bytes);
}

static bool color_done(esp_lcd_panel_io_handle_t io,
                       esp_lcd_panel_io_event_data_t *ed, void *ctx)
{
    (void)io;
    (void)ed;
    (void)ctx;
    lv_disp_drv_t *d = s_flushing;
    if (d) {
        s_flushing = NULL;
        lv_disp_flush_ready(d);
        return false;
    }
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_push_done, &woken);
    return woken == pdTRUE;
}

static inline void swap_copy(uint16_t *d, const uint16_t *s, size_t n)
{
    for (size_t i = 0; i < n; i++) d[i] = __builtin_bswap16(s[i]);
}

/* Waveshare's set-up for this panel (their BSP); brightness starts at 0,
 * the boot logo brings it up */
static void panel_init(void)
{
    gpio_config_t rst = {
        .pin_bit_mask = 1ULL << PIN_LCD_RST,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst);
    gpio_set_level(PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    const spi_bus_config_t bus = {
        .sclk_io_num = PIN_LCD_CLK,
        .data0_io_num = PIN_LCD_D0,
        .data1_io_num = PIN_LCD_D1,
        .data2_io_num = PIN_LCD_D2,
        .data3_io_num = PIN_LCD_D3,
        .max_transfer_sz = LCD_H_RES * PUSH_LINES * 2 + 64,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    const esp_lcd_panel_io_spi_config_t io = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = -1,
        .spi_mode = 0,
        .pclk_hz = LCD_PCLK_HZ,
        .trans_queue_depth = 4,
        .on_color_trans_done = color_done,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags = { .quad_mode = 1 },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                             &io, &s_io));
    s_push_done = xSemaphoreCreateCounting(64, 0);

    static const struct {
        uint8_t cmd, n, d[4];
        uint16_t ms;
    } INIT[] = {
        { 0xFE, 1, { 0x20 }, 0 },
        { 0x19, 1, { 0x10 }, 0 },
        { 0x1C, 1, { 0xA0 }, 0 },
        { 0xFE, 1, { 0x00 }, 0 },
        { 0xC4, 1, { 0x80 }, 0 },
        { 0x3A, 1, { 0x55 }, 0 },           /* RGB565 */
        { 0x35, 1, { 0x00 }, 0 },
        { 0x53, 1, { 0x20 }, 0 },           /* brightness by 0x51 */
        { 0x51, 1, { 0x00 }, 0 },
        { 0x63, 1, { 0xFF }, 0 },
        { 0x2A, 4, { 0x00, LCD_X_GAP, (LCD_X_GAP + LCD_H_RES - 1) >> 8,
                     (LCD_X_GAP + LCD_H_RES - 1) & 0xFF }, 0 },
        { 0x2B, 4, { 0x00, 0x00, (LCD_V_RES - 1) >> 8,
                     (LCD_V_RES - 1) & 0xFF }, 0 },
        { 0x11, 0, { 0 }, 300 },            /* out of sleep */
        { 0x29, 0, { 0 }, 20 },             /* on */
    };
    for (size_t i = 0; i < sizeof INIT / sizeof INIT[0]; i++) {
        lcd_cmd(INIT[i].cmd, INIT[i].n ? INIT[i].d : NULL, INIT[i].n);
        if (INIT[i].ms) vTaskDelay(pdMS_TO_TICKS(INIT[i].ms));
    }
    ESP_LOGI(TAG, "CO5300 up");
}

/* ----------------------------------------------------------- brightness */
static uint8_t s_level = 255;     /* what the settings ask for */
static bool s_booting = true;     /* the boot logo has the brightness */

static void bright_set(uint8_t v)
{
    lcd_cmd(0x51, &v, 1);
}

void hw_backlight(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_level = (uint8_t)(percent * 255 / 100);
    if (!s_booting) bright_set(s_level);
}

/* ----------------------------------------------------------------- touch */
static bool s_touch_ok;

static void touch_init(void)
{
    gpio_config_t in = {
        .pin_bit_mask = 1ULL << PIN_TP_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&in);
    gpio_config_t rst = {
        .pin_bit_mask = 1ULL << PIN_TP_RST,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst);
    gpio_set_level(PIN_TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_TP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(80));
#if TOUCH_CST8XX
    /* chip id 0xA7; no auto sleep (0xFE), or it stops answering */
    const uint8_t id_reg = 0xA7;
    uint8_t id = 0;
    s_touch_ok = i2c_write_read(CST8XX_ADDR, &id_reg, 1, &id, 1) == ESP_OK;
    if (s_touch_ok) {
        const uint8_t nosleep[2] = { 0xFE, 0x01 };
        i2c_write(CST8XX_ADDR, nosleep, 2);
    }
    ESP_LOGI(TAG, "CST8xx %s, id 0x%02X", s_touch_ok ? "ready" : "not answering", id);
#else
    const uint8_t cmd[2] = { 0xD0, 0x00 };
    s_touch_ok = i2c_write(CST9217_ADDR, cmd, 2) == ESP_OK;
    ESP_LOGI(TAG, "CST9217 %s", s_touch_ok ? "ready" : "not answering");
#endif
}

#if TOUCH_CST8XX
/* CST816 / CST820: from 0x02 the finger count, then x and y in 12 bits */
static bool touch_point(lv_coord_t *px, lv_coord_t *py)
{
    const uint8_t reg = 0x02;
    uint8_t b[5];
    if (i2c_write_read(CST8XX_ADDR, &reg, 1, b, sizeof b) != ESP_OK) return false;
    if ((b[0] & 0x0F) == 0) return false;
    *px = (lv_coord_t)(((b[1] & 0x0F) << 8) | b[2]);
    *py = (lv_coord_t)(((b[3] & 0x0F) << 8) | b[4]);
    return true;
}
#else
/* The report (as in Waveshare's CST92xx driver): command 0xD000, 15 bytes
 * back, then 0xD000 0xAB to acknowledge. Byte 6 is 0xAB when the report is
 * good, byte 5 the number of fingers; the first finger: event in the low
 * nibble of byte 0 (6: down), x and y in 12 bits over bytes 1..3. */
static bool touch_point(lv_coord_t *px, lv_coord_t *py)
{
    static const uint8_t CMD[2] = { 0xD0, 0x00 };
    static const uint8_t ACK[3] = { 0xD0, 0x00, 0xAB };
    uint8_t b[15];
    if (i2c_write_read(CST9217_ADDR, CMD, 2, b, sizeof b) != ESP_OK) return false;
    i2c_write(CST9217_ADDR, ACK, 3);
    int n = b[5] & 0x7F;
    if (!(b[6] == 0xAB && n >= 1 && n <= 2 && (b[0] & 0x0F) == 0x06)) return false;
    *px = (lv_coord_t)((b[1] << 4) | (b[3] >> 4));
    *py = (lv_coord_t)((b[2] << 4) | (b[3] & 0x0F));
    return true;
}
#endif

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    static lv_coord_t lx, ly;
    lv_coord_t x, y;
    data->state = LV_INDEV_STATE_RELEASED;
    if (touch_point(&x, &y)) {
#if TOUCH_SWAP_XY
        lv_coord_t t = x; x = y; y = t;
#endif
#if TOUCH_MIRROR_X
        x = LCD_H_RES - 1 - x;
#endif
#if TOUCH_MIRROR_Y
        y = LCD_V_RES - 1 - y;
#endif
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= LCD_H_RES) x = LCD_H_RES - 1;
        if (y >= LCD_V_RES) y = LCD_V_RES - 1;
        lx = x;
        ly = y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    data->point.x = lx;
    data->point.y = ly;
}

/* ----------------------------------------------------------------- beep */
/* No buzzer here: a tone through the ES8311 and the amplifier into the
 * speaker. 16 kHz, 16 bit, the codec as I2S slave with MCLK = 256 x fs.
 * n beeps of BEEP_ON_MS with BEEP_OFF_MS between, from a task of its own:
 * hw_beep never blocks the UI. */
#define BEEP_RATE    16000
#define BEEP_HZ      2400
#define BEEP_ON_MS   90
#define BEEP_OFF_MS  110
#define BEEP_AMP     22000        /* of 32767 */
#define BEEP_VOLUME  0xD8         /* ES8311 DAC volume, 0xBF = 0 dB */
#define BEEP_RAMP    48           /* samples of fade in and out: no clicks */

static i2s_chan_handle_t s_tx;
static TaskHandle_t s_beep_task;

static esp_err_t es_write(uint8_t reg, uint8_t v)
{
    const uint8_t d[2] = { reg, v };
    return i2c_write(ES8311_ADDR, d, 2);
}

/* Espressif's es8311 driver (Waveshare's example), cut down to playback:
 * reset, clocks for MCLK 4.096 MHz / 16 kHz, slave I2S 16 bit, DAC on */
static bool es8311_init(void)
{
    if (es_write(0x00, 0x1F) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(20));
    static const uint8_t SEQ[][2] = {
        { 0x00, 0x00 }, { 0x00, 0x80 },             /* power on */
        { 0x01, 0x3F },                             /* clocks from MCLK */
        { 0x02, 0x00 }, { 0x03, 0x10 }, { 0x04, 0x10 }, { 0x05, 0x00 },
        { 0x06, 0x03 }, { 0x07, 0x00 }, { 0x08, 0xFF },
        { 0x09, 0x0C }, { 0x0A, 0x0C },             /* I2S, 16 bit */
        { 0x0D, 0x01 }, { 0x0E, 0x02 }, { 0x12, 0x00 }, { 0x13, 0x10 },
        { 0x1C, 0x6A }, { 0x37, 0x08 },
        { 0x32, BEEP_VOLUME }, { 0x31, 0x00 },      /* volume, unmuted */
    };
    for (size_t i = 0; i < sizeof SEQ / sizeof SEQ[0]; i++) {
        if (es_write(SEQ[i][0], SEQ[i][1]) != ESP_OK) return false;
    }
    return true;
}

static volatile int s_vol = 70;   /* % of BEEP_AMP, SETTINGS -> VOLUME */

void hw_volume(uint8_t percent)
{
    s_vol = percent < 10 ? 10 : percent > 100 ? 100 : percent;
}

/* ms of tone (on) or silence, as stereo frames */
static void play(int ms, bool on)
{
    static int16_t buf[2 * 160];                  /* 10 ms */
    static uint32_t phase;
    const uint32_t step = (uint32_t)((uint64_t)BEEP_HZ * 65536 / BEEP_RATE);
    int total = BEEP_RATE * ms / 1000;
    for (int done = 0; done < total;) {
        int n = total - done < 160 ? total - done : 160;
        for (int i = 0; i < n; i++) {
            int16_t v = 0;
            if (on) {
                /* the tone at the PCM sounds' loudness: a sine at full
                 * BEEP_AMP is some 12 dB above them */
                int k = done + i, env = BEEP_AMP * 30 / 100 * s_vol / 100;
                if (k < BEEP_RAMP) env = env * k / BEEP_RAMP;
                if (total - k < BEEP_RAMP) env = env * (total - k) / BEEP_RAMP;
                v = (int16_t)(env * sinf(phase * (2.0f * (float)M_PI / 65536)));
                phase = (phase + step) & 0xFFFF;
            }
            buf[2 * i] = buf[2 * i + 1] = v;
        }
        size_t w;
        i2s_channel_write(s_tx, buf, n * 4, &w, pdMS_TO_TICKS(200));
        done += n;
    }
}

/* PCM from hw_play: 16 kHz mono, to both channels, scaled to the beep's
 * loudness */
#define PLAY_PCM 0x10000          /* notify value: play s_pcm, not beeps */
static const int16_t *s_pcm;
static size_t s_pcm_n;

static void play_pcm(void)
{
    static int16_t buf[2 * 160];
    for (size_t done = 0; done < s_pcm_n;) {
        size_t n = s_pcm_n - done < 160 ? s_pcm_n - done : 160;
        for (size_t i = 0; i < n; i++) {
            int16_t v = (int16_t)((int32_t)s_pcm[done + i] * BEEP_AMP / 29000
                                  * s_vol / 100);
            buf[2 * i] = buf[2 * i + 1] = v;
        }
        size_t w;
        i2s_channel_write(s_tx, buf, n * 4, &w, pdMS_TO_TICKS(200));
        done += n;
    }
}

static void beep_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t n = 0;
        xTaskNotifyWait(0, UINT32_MAX, &n, portMAX_DELAY);
        if (n == PLAY_PCM) {
            gpio_set_level(PIN_PA, 1);
            play(20, false);                      /* amplifier settles */
            play_pcm();
            play(100, false);                     /* the DMA runs out */
            gpio_set_level(PIN_PA, 0);
            continue;
        }
        gpio_set_level(PIN_PA, 1);
        play(20, false);                          /* amplifier settles */
        for (uint32_t i = 0; i < n && i < 10; i++) {
            play(BEEP_ON_MS, true);
            play(BEEP_OFF_MS, false);
        }
        play(100, false);                         /* the DMA runs out */
        gpio_set_level(PIN_PA, 0);
    }
}

bool hw_play(const int16_t *pcm, size_t n)
{
    if (!pcm || !n || !s_beep_task) return false;
    s_pcm = pcm;
    s_pcm_n = n;
    /* while something plays, a new request is dropped */
    xTaskNotify(s_beep_task, PLAY_PCM, eSetValueWithoutOverwrite);
    return true;
}

void hw_beep(int n)
{
    if (n <= 0 || !s_beep_task) return;
    /* while it beeps, a new request is dropped, as on the buzzer */
    xTaskNotify(s_beep_task, (uint32_t)n, eSetValueWithoutOverwrite);
}

static void audio_init(void)
{
    gpio_config_t pa = {
        .pin_bit_mask = 1ULL << PIN_PA,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&pa);
    gpio_set_level(PIN_PA, 0);

    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,
                                                      I2S_ROLE_MASTER);
    cc.auto_clear = true;                         /* silence when idle */
    i2s_std_config_t sc = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BEEP_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = PIN_I2S_MCLK,
            .bclk = PIN_I2S_BCLK,
            .ws = PIN_I2S_WS,
            .dout = PIN_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
        },
    };
    sc.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    if (i2s_new_channel(&cc, &s_tx, NULL) != ESP_OK ||
        i2s_channel_init_std_mode(s_tx, &sc) != ESP_OK ||
        i2s_channel_enable(s_tx) != ESP_OK) {
        ESP_LOGE(TAG, "I2S failed, no beep");
        return;
    }
    /* the codec wants MCLK running before it is set up */
    if (!es8311_init()) {
        ESP_LOGE(TAG, "ES8311 not answering at 0x%02X, no beep", ES8311_ADDR);
        return;
    }
    xTaskCreatePinnedToCore(beep_task, "beep", 3072, NULL, 3, &s_beep_task, 0);
    ESP_LOGI(TAG, "ES8311 ready, beep on the speaker");
}

/* ------------------------------------------------------------------ LVGL */
/* While set, LVGL renders into this buffer instead of the panel: the boot
 * gets a finished gauge frame to cross-fade into. */
static uint16_t *s_shadow;

static void rounder_cb(lv_disp_drv_t *drv, lv_area_t *a)
{
    (void)drv;
    a->x1 &= ~1;
    a->y1 &= ~1;
    a->x2 |= 1;
    a->y2 |= 1;
}

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px)
{
    int w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
    if (s_shadow) {
        for (int y = a->y1; y <= a->y2; y++) {
            memcpy(s_shadow + y * LCD_H_RES + a->x1, px, w * 2);
            px += w;
        }
        lv_disp_flush_ready(drv);
        return;
    }
    uint16_t *p = (uint16_t *)px;
    swap_copy(p, p, (size_t)w * h);           /* in place: it is ours now */
    lcd_window(a->x1, a->y1, a->x2, a->y2);
    s_flushing = drv;
    lcd_pixels(p, (size_t)w * h * 2);
}

static void tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_MS);
}

static void lvgl_init(void)
{
    lv_init();
    static lv_disp_draw_buf_t buf;
    static lv_disp_drv_t drv;
    size_t px = LCD_H_RES * LVGL_BUF_LINES;
    lv_color_t *b1 = heap_caps_malloc(px * sizeof(lv_color_t),
                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    lv_color_t *b2 = heap_caps_malloc(px * sizeof(lv_color_t),
                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(b1 && b2);
    lv_disp_draw_buf_init(&buf, b1, b2, px);
    lv_disp_drv_init(&drv);
    drv.hor_res = LCD_H_RES;
    drv.ver_res = LCD_V_RES;
    drv.flush_cb = flush_cb;
    drv.rounder_cb = rounder_cb;
    drv.draw_buf = &buf;
    lv_disp_drv_register(&drv);

    const esp_timer_create_args_t t = { .callback = tick_cb, .name = "lv_tick" };
    esp_timer_handle_t h;
    ESP_ERROR_CHECK(esp_timer_create(&t, &h));
    ESP_ERROR_CHECK(esp_timer_start_periodic(h, LVGL_TICK_MS * 1000));

    if (s_touch_ok) {
        static lv_indev_drv_t in;
        lv_indev_drv_init(&in);
        in.type = LV_INDEV_TYPE_POINTER;
        in.read_cb = touch_read;
        lv_indev_drv_register(&in);
    }
}

/* ------------------------------------------------------------------ boot */
/* The logo comes up out of black on the panel's own brightness (smooth on
 * an AMOLED, nothing to redraw), holds while LVGL renders the gauges into a
 * shadow buffer, and cross-fades into them frame by frame (boot_fb.c). */
static uint16_t *s_strip[2];

static int s_push_lines = PUSH_LINES;

static void push_frame(const uint16_t *fb)
{
    while (xSemaphoreTake(s_push_done, 0) == pdTRUE) {}
    int sent = 0;
    for (int y = 0, k = 0; y < LCD_V_RES; y += s_push_lines, k ^= 1) {
        int h = LCD_V_RES - y < s_push_lines ? LCD_V_RES - y : s_push_lines;
        /* the strip in this buffer two steps ago is out: the window
         * commands of the last step waited for it */
        swap_copy(s_strip[k], fb + y * LCD_H_RES, (size_t)LCD_H_RES * h);
        lcd_window(0, y, LCD_H_RES - 1, y + h - 1);
        lcd_pixels(s_strip[k], (size_t)LCD_H_RES * h * 2);
        sent++;
    }
    while (sent--) xSemaphoreTake(s_push_done, portMAX_DELAY);
}

static void boot_end(void)
{
    s_booting = false;
    bright_set(s_level);
}

void hw_boot(const lv_img_dsc_t *logo)
{
    size_t frame = (size_t)LCD_H_RES * LCD_V_RES * 2;
    uint16_t *fb = heap_caps_calloc(1, frame, MALLOC_CAP_SPIRAM);
    uint16_t *shadow = heap_caps_malloc(frame, MALLOC_CAP_SPIRAM);
    /* the strips need internal DMA memory, which LVGL, the audio and the
     * tasks share: as high as there is room for, down to 5 lines */
    for (s_push_lines = PUSH_LINES; s_push_lines >= 5; s_push_lines /= 2) {
        for (int i = 0; i < 2; i++) {
            s_strip[i] = heap_caps_malloc(LCD_H_RES * s_push_lines * 2,
                                          MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        }
        if (s_strip[0] && s_strip[1]) break;
        for (int i = 0; i < 2; i++) {
            heap_caps_free(s_strip[i]);
            s_strip[i] = NULL;
        }
    }
    ESP_LOGI(TAG, "boot logo: strips of %d lines, internal DMA %u B free",
             s_strip[0] ? s_push_lines : 0,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    if (!fb || !shadow || !s_strip[0] || !s_strip[1]) {
        ESP_LOGW(TAG, "no memory, no boot logo (fb %p shadow %p)", fb, shadow);
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(NULL);
        boot_end();
        goto out;
    }

    /* the logo, dark, then brightness up along the 2.1"'s curve */
    boot_fb_logo(fb, logo, 32);
    push_frame(fb);
    int64_t t0 = esp_timer_get_time();
    for (int last = -1;;) {
        uint32_t ms = (uint32_t)((esp_timer_get_time() - t0) / 1000);
        int lv = boot_fb_in_level(ms);
        if (lv != last) {
            bright_set((uint8_t)(s_level * lv / 32));
            last = lv;
        }
        if (lv >= 32) break;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    s_booting = false;

    /* hold: LVGL runs, into the shadow, so the gauge sweeps up on live
     * values behind the logo */
    s_shadow = shadow;
    lv_obj_invalidate(lv_scr_act());
    int64_t hold_end = t0 + (int64_t)(RND_BOOT_IN_MS + RND_BOOT_HOLD_MS) * 1000;
    while (esp_timer_get_time() < hold_end) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);

    int64_t t1 = esp_timer_get_time();
    for (int last = -1;;) {
        uint32_t ms = (uint32_t)((esp_timer_get_time() - t1) / 1000);
        int lv = boot_fb_x_level(ms);
        if (lv != last) {
            boot_fb_cross(fb, logo, shadow, lv);
            push_frame(fb);
            last = lv;
        }
        if (lv >= 32) break;
        vTaskDelay(1);
    }
    /* the panel now shows the last LVGL frame: LVGL goes back to it */
    s_shadow = NULL;
    boot_end();
out:
    heap_caps_free(fb);
    heap_caps_free(shadow);
    for (int i = 0; i < 2; i++) {
        heap_caps_free(s_strip[i]);
        s_strip[i] = NULL;
    }
}

/* ------------------------------------------------------------------ init */
void hw_init(void)
{
#ifdef PIN_BAT_EN
    /* the 1.32's battery switch: held on, or a battery-only start dies */
    gpio_config_t pw = { .pin_bit_mask = 1ULL << PIN_BAT_EN, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&pw);
    gpio_set_level(PIN_BAT_EN, 1);
#endif
#ifdef PIN_CODEC_EN
    /* the 1.32 switches the codec's supply */
    gpio_config_t ce = { .pin_bit_mask = 1ULL << PIN_CODEC_EN, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&ce);
    gpio_set_level(PIN_CODEC_EN, 1);
#endif
    i2c_init();
    i2c_scan();
    panel_init();
    touch_init();
    audio_init();
    lvgl_init();
    motion_start();
}
