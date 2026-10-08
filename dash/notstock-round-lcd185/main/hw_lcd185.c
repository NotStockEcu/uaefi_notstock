/* Board bring-up for the Waveshare ESP32-S3-Touch-LCD-1.85, see
 * board_lcd185.h: the ST77916 LCD over QSPI, LVGL, the backlight, the
 * CST816 touch and the beep through the PCM5101 and the speaker.
 * The same API as the 2.1" LCD's hw.c (../notstock-round/main/hw.h).
 *
 * The UI is the 2.1"'s, laid out for 480 x 480. Rather than a second
 * layout, LVGL renders into a 480 x 480 frame in PSRAM and every refresh
 * goes out scaled to the panel's 360 x 360: each 4 x 4 block of pixels
 * becomes 3 x 3, area-weighted, so text and arcs stay smooth. RGB565 goes
 * out high byte first, swapped on the way.
 */
#include "hw.h"
#include "board_lcd185.h"
#include "boot_fb.h"
#include "esp_lcd_st77916.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "hw";

#define FB_W           RND_W      /* the UI's frame: 480 */
#define LVGL_BUF_LINES 40
#define LVGL_TICK_MS   2
#define STRIP_SRC      32         /* frame rows per strip to the panel */
#define STRIP_ROWS     (STRIP_SRC * 3 / 4)

_Static_assert(FB_W * 3 / 4 == LCD_H_RES, "the panel is 3/4 of the UI");

/* ------------------------------------------------------------------ I2C */
/* Board V2: everything on GPIO10/11. V1: the touch on GPIO1/3. */
#define BUS_MAIN I2C_NUM_0
#define BUS_V1   I2C_NUM_1

static SemaphoreHandle_t s_i2c;
static i2c_port_t s_tp_bus = BUS_MAIN;

static void i2c_bus(i2c_port_t port, int sda, int scl)
{
    i2c_config_t c = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(port, &c));
    ESP_ERROR_CHECK(i2c_driver_install(port, c.mode, 0, 0, 0));
}

static esp_err_t i2c_write(i2c_port_t port, uint8_t addr, const uint8_t *d,
                           size_t n)
{
    xSemaphoreTake(s_i2c, portMAX_DELAY);
    esp_err_t e = i2c_master_write_to_device(port, addr, d, n,
                                             pdMS_TO_TICKS(50));
    xSemaphoreGive(s_i2c);
    return e;
}

static esp_err_t i2c_read(i2c_port_t port, uint8_t addr, uint8_t reg,
                          uint8_t *d, size_t n)
{
    xSemaphoreTake(s_i2c, portMAX_DELAY);
    esp_err_t e = i2c_master_write_read_device(port, addr, &reg, 1, d, n,
                                               pdMS_TO_TICKS(50));
    xSemaphoreGive(s_i2c);
    return e;
}

static bool i2c_there(i2c_port_t port, uint8_t addr)
{
    i2c_cmd_handle_t c = i2c_cmd_link_create();
    i2c_master_start(c);
    i2c_master_write_byte(c, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    i2c_master_stop(c);
    xSemaphoreTake(s_i2c, portMAX_DELAY);
    esp_err_t e = i2c_master_cmd_begin(port, c, pdMS_TO_TICKS(20));
    xSemaphoreGive(s_i2c);
    i2c_cmd_link_delete(c);
    return e == ESP_OK;
}

/* Logs what answers: tells a wiring problem from a driver problem. On this
 * board: 0x15 touch, 0x20 TCA9554, 0x51 RTC, 0x6B IMU. */
static void i2c_scan(i2c_port_t port)
{
    char line[128];
    int len = snprintf(line, sizeof line, "i2c%d devices:", (int)port);
    for (int a = 0x08; a < 0x78; a++) {
        if (i2c_there(port, (uint8_t)a) && len < (int)sizeof line - 8) {
            len += snprintf(line + len, sizeof line - len, " 0x%02X", a);
        }
    }
    ESP_LOGI(TAG, "%s", line);
}

/* --------------------------------------------------------------- TCA9554 */
static uint8_t s_exio;

static void exio_write(void)
{
    const uint8_t out[2] = { 0x01, s_exio };
    i2c_write(BUS_MAIN, TCA9554_ADDR, out, 2);
}

void exio_set(uint8_t mask, bool on)
{
    if (on) s_exio |= mask;
    else    s_exio &= (uint8_t)~mask;
    exio_write();
}

/* both resets low, then the expander's two pins as outputs, the rest left
 * as inputs (SD card and the like) */
static void tca_init(void)
{
    s_exio = 0;
    exio_write();
    const uint8_t cfg[2] = { 0x03, (uint8_t)~(EXIO_TP_RST | EXIO_LCD_RST) };
    if (i2c_write(BUS_MAIN, TCA9554_ADDR, cfg, 2) != ESP_OK) {
        ESP_LOGW(TAG, "TCA9554 not answering: no panel or touch reset");
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    exio_set(EXIO_TP_RST | EXIO_LCD_RST, true);
    vTaskDelay(pdMS_TO_TICKS(60));
}

/* ------------------------------------------------------------ backlight */
static uint8_t s_level = 100;     /* what the settings ask for */
static bool s_booting = true;     /* the boot logo has the backlight */

static void bl_duty(uint32_t duty)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void backlight_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 20000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    ledc_channel_config_t c = {
        .gpio_num = PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,                         /* dark until there is a picture */
    };
    ESP_ERROR_CHECK(ledc_channel_config(&c));
}

void hw_backlight(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_level = percent;
    if (!s_booting) bl_duty((uint32_t)percent * 1023 / 100);
}

/* ---------------------------------------------------------------- panel */
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_sent;          /* a strip went out */
static uint16_t *s_strip[2];              /* DMA-capable, panel byte order */

static bool color_done(esp_lcd_panel_io_handle_t io,
                       esp_lcd_panel_io_event_data_t *ed, void *ctx)
{
    (void)io;
    (void)ed;
    (void)ctx;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_sent, &woken);
    return woken == pdTRUE;
}

extern const st77916_lcd_init_cmd_t st77916_init_new[];
extern const size_t st77916_init_new_n;

static void panel_init(void)
{
    const spi_bus_config_t bus = {
        .sclk_io_num = PIN_LCD_CLK,
        .data0_io_num = PIN_LCD_D0,
        .data1_io_num = PIN_LCD_D1,
        .data2_io_num = PIN_LCD_D2,
        .data3_io_num = PIN_LCD_D3,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = LCD_H_RES * STRIP_ROWS * 2 + 64,
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_spi_config_t io = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = -1,
        .spi_mode = 0,
        .pclk_hz = 3 * 1000 * 1000,       /* slow, to read the ID */
        .trans_queue_depth = 4,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags = { .quad_mode = 1 },
    };
    esp_lcd_panel_io_handle_t h;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                             &io, &h));
    /* two panel revisions with different set-ups: the ID tells them apart,
     * as in Waveshare's demo */
    uint8_t id[4] = { 0 };
    esp_lcd_panel_io_rx_param(h, (0x0B << 24) | (0x04 << 8), id, sizeof id);
    ESP_ERROR_CHECK(esp_lcd_panel_io_del(h));
    ESP_LOGI(TAG, "ST77916 ID %02X %02X %02X %02X", id[0], id[1], id[2], id[3]);

    io.pclk_hz = LCD_PCLK_HZ;
    io.on_color_trans_done = color_done;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                             &io, &h));
    static st77916_vendor_config_t vendor = {
        .flags = { .use_qspi_interface = 1 },
    };
    if (id[0] == 0x00 && id[1] == 0x02 && id[2] == 0x7F && id[3] == 0x7F) {
        vendor.init_cmds = st77916_init_new;
        vendor.init_cmds_size = (uint16_t)st77916_init_new_n;
    }
    const esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = -1,             /* EXIO2, done in tca_init */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(h, &dev, &s_panel));
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_disp_on_off(s_panel, true);

    s_sent = xSemaphoreCreateCounting(64, 0);
    for (int i = 0; i < 2; i++) {
        s_strip[i] = heap_caps_malloc(LCD_H_RES * STRIP_ROWS * 2,
                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        assert(s_strip[i]);
    }
}

/* ---------------------------------------------------------- 480 -> 360 */
/* RGB565 spread to 0x07E0F81F leaves room above each field: 16 weighted
 * pixels add up without one field running into the next. */
static inline uint32_t spread(uint16_t c)
{
    return ((uint32_t)c | ((uint32_t)c << 16)) & 0x07E0F81Fu;
}

static inline uint16_t pack16(uint32_t x)
{
    x = ((x + 0x01004008u) >> 4) & 0x07E0F81Fu;      /* /16, rounded */
    uint16_t c = (uint16_t)(x | (x >> 16));
    return (uint16_t)((c << 8) | (c >> 8));          /* panel byte order */
}

/* frame rows y0..y0+4n-1, columns x0..x0+4m-1 (multiples of 4) into 3n
 * rows of 3m pixels. Weights per 4 -> 3: (3,1) (2,2) (1,3). */
static void scale_block_rows(const uint16_t *fb, int x0, int y0, int m,
                             int n, uint16_t *out)
{
    int ow = 3 * m;
    for (int by = 0; by < n; by++) {
        const uint16_t *r[4];
        for (int i = 0; i < 4; i++) r[i] = fb + (y0 + 4 * by + i) * FB_W + x0;
        uint16_t *o0 = out + (3 * by) * ow, *o1 = o0 + ow, *o2 = o1 + ow;
        for (int bx = 0; bx < m; bx++) {
            uint32_t h[4][3];
            for (int i = 0; i < 4; i++) {
                const uint16_t *p = r[i] + 4 * bx;
                uint32_t s0 = spread(p[0]), s1 = spread(p[1]);
                uint32_t s2 = spread(p[2]), s3 = spread(p[3]);
                h[i][0] = 3 * s0 + s1;
                h[i][1] = 2 * (s1 + s2);
                h[i][2] = s2 + 3 * s3;
            }
            for (int j = 0; j < 3; j++) {
                o0[3 * bx + j] = pack16(3 * h[0][j] + h[1][j]);
                o1[3 * bx + j] = pack16(2 * (h[1][j] + h[2][j]));
                o2[3 * bx + j] = pack16(h[2][j] + 3 * h[3][j]);
            }
        }
    }
}

/* the frame's x1..x2, y1..y2 (inclusive) to the panel, scaled */
static void present(const uint16_t *fb, int x1, int y1, int x2, int y2)
{
    x1 &= ~3;
    y1 &= ~3;
    x2 |= 3;
    y2 |= 3;
    int m = (x2 - x1 + 1) / 4, ox = x1 * 3 / 4, ow = 3 * m;
    while (xSemaphoreTake(s_sent, 0) == pdTRUE) {}
    int sent = 0, k = 0;
    for (int y = y1; y <= y2; y += STRIP_SRC, k ^= 1) {
        int n = (y2 + 1 - y) / 4;
        if (n > STRIP_SRC / 4) n = STRIP_SRC / 4;
        /* the strip in this buffer two steps ago is out: the address
         * commands of the last step waited for it */
        scale_block_rows(fb, x1, y, m, n, s_strip[k]);
        int oy = y * 3 / 4;
        esp_lcd_panel_draw_bitmap(s_panel, ox, oy, ox + ow, oy + 3 * n,
                                  s_strip[k]);
        sent++;
    }
    while (sent--) xSemaphoreTake(s_sent, portMAX_DELAY);
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
    if (!i2c_there(BUS_MAIN, CST816_ADDR)) {
        /* board V1: the touch has pins of its own */
        i2c_bus(BUS_V1, PIN_TP_SDA_V1, PIN_TP_SCL_V1);
        if (i2c_there(BUS_V1, CST816_ADDR)) s_tp_bus = BUS_V1;
    }
    s_touch_ok = i2c_there(s_tp_bus, CST816_ADDR);
    if (s_touch_ok) {
        const uint8_t awake[2] = { 0xFE, 0x01 };     /* no auto sleep */
        i2c_write(s_tp_bus, CST816_ADDR, awake, 2);
    }
    ESP_LOGI(TAG, "CST816 %s%s", s_touch_ok ? "ready" : "not answering",
             s_touch_ok && s_tp_bus == BUS_V1 ? " (board V1 pins)" : "");
}

/* reg 0x02: fingers, then x and y in 12 bits; panel pixels, so scaled up
 * to the UI's 480 */
static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    static lv_coord_t lx, ly;
    uint8_t b[5];
    data->state = LV_INDEV_STATE_RELEASED;
    if (i2c_read(s_tp_bus, CST816_ADDR, 0x02, b, sizeof b) == ESP_OK &&
        (b[0] & 0x0F) > 0) {
        int x = ((b[1] & 0x0F) << 8) | b[2];
        int y = ((b[3] & 0x0F) << 8) | b[4];
#if TOUCH_SWAP_XY
        int t = x; x = y; y = t;
#endif
#if TOUCH_MIRROR_X
        x = LCD_H_RES - 1 - x;
#endif
#if TOUCH_MIRROR_Y
        y = LCD_V_RES - 1 - y;
#endif
        x = x * 4 / 3;
        y = y * 4 / 3;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= FB_W) x = FB_W - 1;
        if (y >= FB_W) y = FB_W - 1;
        lx = (lv_coord_t)x;
        ly = (lv_coord_t)y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    data->point.x = lx;
    data->point.y = ly;
}

/* ----------------------------------------------------------------- beep */
/* No buzzer: a tone through the PCM5101 into the amplifier and speaker.
 * 16 kHz, 16 bit; the DAC makes its own clock from BCK. n beeps of
 * BEEP_ON_MS with BEEP_OFF_MS between, from a task of its own. */
#define BEEP_RATE    16000
#define BEEP_HZ      2400
#define BEEP_ON_MS   90
#define BEEP_OFF_MS  110
#define BEEP_AMP     16000        /* of 32767: loudness */
#define BEEP_RAMP    48           /* samples of fade in and out: no clicks */

static i2s_chan_handle_t s_tx;
static TaskHandle_t s_beep_task;

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
                int k = done + i, env = BEEP_AMP;
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

static void beep_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t n = 0;
        xTaskNotifyWait(0, UINT32_MAX, &n, portMAX_DELAY);
        for (uint32_t i = 0; i < n && i < 10; i++) {
            play(BEEP_ON_MS, true);
            play(BEEP_OFF_MS, false);
        }
    }
}

void hw_beep(int n)
{
    if (n <= 0 || !s_beep_task) return;
    /* while it beeps, a new request is dropped, as on the buzzer */
    xTaskNotify(s_beep_task, (uint32_t)n, eSetValueWithoutOverwrite);
}

static void audio_init(void)
{
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,
                                                      I2S_ROLE_MASTER);
    cc.auto_clear = true;                         /* silence when idle */
    i2s_std_config_t sc = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BEEP_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_I2S_BCK,
            .ws = PIN_I2S_WS,
            .dout = PIN_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
        },
    };
    if (i2s_new_channel(&cc, &s_tx, NULL) != ESP_OK ||
        i2s_channel_init_std_mode(s_tx, &sc) != ESP_OK ||
        i2s_channel_enable(s_tx) != ESP_OK) {
        ESP_LOGE(TAG, "I2S failed, no beep");
        return;
    }
    xTaskCreatePinnedToCore(beep_task, "beep", 3072, NULL, 3, &s_beep_task, 0);
    ESP_LOGI(TAG, "PCM5101 ready, beep on the speaker");
}

/* ------------------------------------------------------------------ LVGL */
static uint16_t *s_fb;            /* the UI's 480 x 480 frame, PSRAM */
static bool s_hold;               /* boot: render, but do not show */
static lv_area_t s_dirty;
static bool s_have_dirty;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px)
{
    int w = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; y++) {
        memcpy(s_fb + y * FB_W + a->x1, px, w * 2);
        px += w;
    }
    if (!s_have_dirty) {
        s_dirty = *a;
        s_have_dirty = true;
    } else {
        if (a->x1 < s_dirty.x1) s_dirty.x1 = a->x1;
        if (a->y1 < s_dirty.y1) s_dirty.y1 = a->y1;
        if (a->x2 > s_dirty.x2) s_dirty.x2 = a->x2;
        if (a->y2 > s_dirty.y2) s_dirty.y2 = a->y2;
    }
    if (lv_disp_flush_is_last(drv)) {
        if (!s_hold) {
            present(s_fb, s_dirty.x1, s_dirty.y1, s_dirty.x2, s_dirty.y2);
        }
        s_have_dirty = false;
    }
    lv_disp_flush_ready(drv);
}

static void tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_MS);
}

static void lvgl_init(void)
{
    s_fb = heap_caps_calloc(1, FB_W * FB_W * 2, MALLOC_CAP_SPIRAM);
    assert(s_fb);
    lv_init();
    static lv_disp_draw_buf_t buf;
    static lv_disp_drv_t drv;
    size_t px = FB_W * LVGL_BUF_LINES;
    lv_color_t *b1 = heap_caps_malloc(px * sizeof(lv_color_t),
                                      MALLOC_CAP_INTERNAL);
    assert(b1);
    lv_disp_draw_buf_init(&buf, b1, NULL, px);
    lv_disp_drv_init(&drv);
    drv.hor_res = FB_W;
    drv.ver_res = FB_W;
    drv.flush_cb = flush_cb;
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
/* The logo comes up out of black on the backlight (smooth, nothing to
 * redraw), holds while LVGL renders the gauges into its frame unseen, and
 * cross-fades into them frame by frame (boot_fb.c), all at 480 and scaled
 * on the way out like everything else. */
static void boot_end(void)
{
    s_booting = false;
    s_hold = false;
    bl_duty((uint32_t)s_level * 1023 / 100);
}

void hw_boot(const lv_img_dsc_t *logo)
{
    uint16_t *out = heap_caps_calloc(1, FB_W * FB_W * 2, MALLOC_CAP_SPIRAM);
    if (!out) {
        ESP_LOGW(TAG, "no memory, no boot logo");
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(NULL);
        boot_end();
        return;
    }
    boot_fb_logo(out, logo, 32);
    present(out, 0, 0, FB_W - 1, FB_W - 1);
    uint32_t full = (uint32_t)s_level * 1023 / 100;
    int64_t t0 = esp_timer_get_time();
    for (int last = -1;;) {
        uint32_t ms = (uint32_t)((esp_timer_get_time() - t0) / 1000);
        int lv = boot_fb_in_level(ms);
        if (lv != last) {
            bl_duty(full * (uint32_t)(lv * lv) / 1024);   /* lit, not linear */
            last = lv;
        }
        if (lv >= 32) break;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    s_booting = false;

    /* hold: LVGL runs into its frame, unseen, so the gauge sweeps up on
     * live values behind the logo */
    s_hold = true;
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
            boot_fb_cross(out, logo, s_fb, lv);
            present(out, 0, 0, FB_W - 1, FB_W - 1);
            last = lv;
        }
        if (lv >= 32) break;
        vTaskDelay(1);
    }
    /* the panel now shows LVGL's frame: LVGL goes on from there */
    boot_end();
    heap_caps_free(out);
}

/* ------------------------------------------------------------------ init */
void hw_init(void)
{
    s_i2c = xSemaphoreCreateMutex();
    i2c_bus(BUS_MAIN, PIN_I2C_SDA, PIN_I2C_SCL);
    i2c_scan(BUS_MAIN);
    backlight_init();
    tca_init();
    panel_init();
    touch_init();
    audio_init();
    lvgl_init();
    motion_start();
}

/* ------------------------------------------------- I2C for the G-meter */
esp_err_t hw_i2c_write(uint8_t addr, const uint8_t *d, size_t n)
{
    return i2c_write(BUS_MAIN, addr, d, n);
}

esp_err_t hw_i2c_write_read(uint8_t addr, const uint8_t *w, size_t wn,
                            uint8_t *r, size_t rn)
{
    xSemaphoreTake(s_i2c, portMAX_DELAY);
    esp_err_t e = i2c_master_write_read_device(BUS_MAIN, addr, w, wn, r, rn,
                                               pdMS_TO_TICKS(50));
    xSemaphoreGive(s_i2c);
    return e;
}
