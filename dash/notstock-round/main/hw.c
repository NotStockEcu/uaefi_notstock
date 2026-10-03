/* Board bring-up for the Waveshare ESP32-S3-Touch-LCD-2.1, see board_round.h:
 * I2C and the TCA9554 expander, the ST7701 set-up over bit-banged 3-wire SPI,
 * the RGB panel, LVGL, backlight PWM, the buzzer and the CST820 touch.
 */
#include "hw.h"
#include "board_round.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "hw";

#define LVGL_BUF_LINES 48
#define LVGL_TICK_MS   2

static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_i2c;          /* touch, expander and buzzer share it */
static uint8_t s_exio;

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

static esp_err_t i2c_read(uint8_t addr, uint8_t reg, uint8_t *d, size_t n)
{
    xSemaphoreTake(s_i2c, portMAX_DELAY);
    esp_err_t e = i2c_master_write_read_device(I2C_NUM_0, addr, &reg, 1, d, n,
                                               pdMS_TO_TICKS(50));
    xSemaphoreGive(s_i2c);
    return e;
}

/* Logs what answers: tells a wiring problem from a driver problem. */
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

/* --------------------------------------------------------------- TCA9554 */
#define TCA_OUT 0x01
#define TCA_CFG 0x03

void exio_set(uint8_t mask, bool on)
{
    if (on) s_exio |= mask;
    else    s_exio &= (uint8_t)~mask;
    const uint8_t d[2] = { TCA_OUT, s_exio };
    i2c_write(TCA9554_ADDR, d, 2);
}

static void tca_init(void)
{
    s_exio = EXIO_LCD_RST | EXIO_TP_RST | EXIO_LCD_CS | EXIO_SD_CS;
    const uint8_t out[2] = { TCA_OUT, s_exio };
    const uint8_t cfg[2] = { TCA_CFG, 0x00 };          /* all outputs */
    if (i2c_write(TCA9554_ADDR, out, 2) != ESP_OK ||
        i2c_write(TCA9554_ADDR, cfg, 2) != ESP_OK) {
        ESP_LOGE(TAG, "TCA9554 not answering at 0x%02X", TCA9554_ADDR);
    }
}

/* ---------------------------------------------------------------- ST7701 */
/* 3-wire SPI, 9 bits a word: D/C first (0 command, 1 data), MSB first,
 * latched on the rising clock edge. CS stays low for the whole set-up. */
static void st_word(bool data, uint8_t v)
{
    uint16_t w = (uint16_t)((data ? 0x100 : 0) | v);
    for (int i = 8; i >= 0; i--) {
        gpio_set_level(PIN_ST_SDA, (w >> i) & 1);
        esp_rom_delay_us(1);
        gpio_set_level(PIN_ST_SCL, 1);
        esp_rom_delay_us(1);
        gpio_set_level(PIN_ST_SCL, 0);
    }
}

/* command, argument count, arguments; a count of 0x80 | n means n arguments
 * and then a delay of the next byte in ms */
#define DLY 0x80
static const uint8_t ST7701_INIT[] = {
    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x10,
    0xC0, 2, 0x3B, 0x00,
    0xC1, 2, 0x0B, 0x02,
    0xC2, 2, 0x07, 0x02,
    0xCC, 1, 0x10,
    0xCD, 1, 0x08,
    0xB0, 16, 0x00, 0x11, 0x16, 0x0E, 0x11, 0x06, 0x05, 0x09,
              0x08, 0x21, 0x06, 0x13, 0x10, 0x29, 0x31, 0x18,
    0xB1, 16, 0x00, 0x11, 0x16, 0x0E, 0x11, 0x07, 0x05, 0x09,
              0x09, 0x21, 0x05, 0x13, 0x11, 0x2A, 0x31, 0x18,
    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x11,
    0xB0, 1, 0x6D,
    0xB1, 1, 0x37,
    0xB2, 1, 0x81,
    0xB3, 1, 0x80,
    0xB5, 1, 0x43,
    0xB7, 1, 0x85,
    0xB8, 1, 0x20,
    0xC1, 1, 0x78,
    0xC2, 1, 0x78,
    0xD0, 1, 0x88,
    0xE0, 3, 0x00, 0x00, 0x02,
    0xE1, 11, 0x03, 0xA0, 0x00, 0x00, 0x04, 0xA0, 0x00, 0x00,
              0x00, 0x20, 0x20,
    0xE2, 13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00,
    0xE3, 4, 0x00, 0x00, 0x11, 0x00,
    0xE4, 2, 0x22, 0x00,
    0xE5, 16, 0x05, 0xEC, 0xA0, 0xA0, 0x07, 0xEE, 0xA0, 0xA0,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xE6, 4, 0x00, 0x00, 0x11, 0x00,
    0xE7, 2, 0x22, 0x00,
    0xE8, 16, 0x06, 0xED, 0xA0, 0xA0, 0x08, 0xEF, 0xA0, 0xA0,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xEB, 7, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00,
    0xED, 16, 0xFF, 0xFF, 0xFF, 0xBA, 0x0A, 0xBF, 0x45, 0xFF,
              0xFF, 0x54, 0xFB, 0xA0, 0xAB, 0xFF, 0xFF, 0xFF,
    0xEF, 6, 0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F,
    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
    0xEF, 1, 0x08,
    0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x00,
    0x36, 1, 0x00,                       /* memory access: as is */
    0x3A, 1, 0x66,                       /* 18 bit, the top 16 wired */
    0x11, DLY | 0, 255,                  /* sleep out */
    0x20, DLY | 0, 120,                  /* no inversion */
    0x29, 0,                             /* display on */
};

static void st7701_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_ST_SDA) | (1ULL << PIN_ST_SCL),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(PIN_ST_SCL, 0);

    exio_set(EXIO_LCD_RST, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    exio_set(EXIO_LCD_RST, true);
    vTaskDelay(pdMS_TO_TICKS(50));

    exio_set(EXIO_LCD_CS, false);
    for (size_t i = 0; i < sizeof ST7701_INIT; ) {
        uint8_t cmd = ST7701_INIT[i++];
        uint8_t n = ST7701_INIT[i++];
        st_word(false, cmd);
        for (int k = 0; k < (n & 0x7F); k++) st_word(true, ST7701_INIT[i++]);
        if (n & DLY) vTaskDelay(pdMS_TO_TICKS(ST7701_INIT[i++]));
    }
    exio_set(EXIO_LCD_CS, true);
    ESP_LOGI(TAG, "ST7701 set up");
}

/* ------------------------------------------------------------- RGB panel */
static void panel_init(void)
{
    const int data_pins[] = PIN_LCD_DATA;
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 1,
        /* internal RAM bounce buffer: the panel keeps its pixels while the
         * CPU reads faces out of PSRAM (the 7" drifted sideways without) */
        .bounce_buffer_size_px = LCD_H_RES * 10,
        .psram_trans_align = 64,
        .sram_trans_align = 4,
        .hsync_gpio_num = PIN_LCD_HSYNC,
        .vsync_gpio_num = PIN_LCD_VSYNC,
        .de_gpio_num = PIN_LCD_DE,
        .pclk_gpio_num = PIN_LCD_PCLK,
        .disp_gpio_num = -1,
        .timings = {
            .pclk_hz = LCD_PCLK_HZ,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = 8,
            .hsync_back_porch = 10,
            .hsync_front_porch = 50,
            .vsync_pulse_width = 3,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
        },
        .flags.fb_in_psram = true,
    };
    memcpy(cfg.data_gpio_nums, data_pins, sizeof(data_pins));
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_LOGI(TAG, "RGB panel up, %dx%d at %d MHz", LCD_H_RES, LCD_V_RES,
             LCD_PCLK_HZ / 1000000);
}

/* ------------------------------------------------------------- backlight */
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
    uint32_t duty = (uint32_t)percent * 1023 / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* ---------------------------------------------------------------- buzzer */
/* n beeps, 80 ms on, 120 ms off, from a timer: never blocks the UI */
#define BEEP_ON_US  80000
#define BEEP_OFF_US 120000
static esp_timer_handle_t s_beep_timer;
static int s_beep_steps;                /* half periods left */

static void beep_cb(void *arg)
{
    (void)arg;
    if (s_beep_steps <= 0) {
        exio_set(EXIO_BUZZER, false);
        return;
    }
    bool on = s_beep_steps % 2 == 0;
    exio_set(EXIO_BUZZER, on);
    s_beep_steps--;
    esp_timer_start_once(s_beep_timer, on ? BEEP_ON_US : BEEP_OFF_US);
}

void hw_beep(int n)
{
    if (n <= 0 || s_beep_steps > 0) return;
    s_beep_steps = 2 * n;
    esp_timer_start_once(s_beep_timer, 1000);
}

static void buzzer_init(void)
{
    const esp_timer_create_args_t a = { .callback = beep_cb, .name = "beep" };
    ESP_ERROR_CHECK(esp_timer_create(&a, &s_beep_timer));
}

/* ----------------------------------------------------------------- touch */
static bool s_touch_ok;

static bool touch_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_TP_INT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    exio_set(EXIO_TP_RST, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    exio_set(EXIO_TP_RST, true);
    vTaskDelay(pdMS_TO_TICKS(60));
    /* the CST820 dozes off after a few seconds untouched; 0xFE = 0xFF
     * keeps it awake */
    const uint8_t d[2] = { 0xFE, 0xFF };
    s_touch_ok = i2c_write(CST820_ADDR, d, 2) == ESP_OK;
    ESP_LOGI(TAG, "CST820 %s", s_touch_ok ? "ready" : "not answering");
    return s_touch_ok;
}

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    static lv_coord_t lx, ly;
    uint8_t b[6];
    data->state = LV_INDEV_STATE_RELEASED;
    if (i2c_read(CST820_ADDR, 0x01, b, sizeof b) == ESP_OK && b[1] > 0) {
        lv_coord_t x = (lv_coord_t)(((b[2] & 0x0F) << 8) | b[3]);
        lv_coord_t y = (lv_coord_t)(((b[4] & 0x0F) << 8) | b[5]);
#if TOUCH_SWAP_XY
        lv_coord_t t = x; x = y; y = t;
#endif
#if TOUCH_MIRROR_X
        x = LCD_H_RES - 1 - x;
#endif
#if TOUCH_MIRROR_Y
        y = LCD_V_RES - 1 - y;
#endif
        if (x >= LCD_H_RES) x = LCD_H_RES - 1;
        if (y >= LCD_V_RES) y = LCD_V_RES - 1;
        lx = x;
        ly = y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    data->point.x = lx;
    data->point.y = ly;
}

/* ------------------------------------------------------------------ LVGL */
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px)
{
    esp_lcd_panel_draw_bitmap(s_panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px);
    lv_disp_flush_ready(drv);
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
    assert(b1);
    lv_disp_draw_buf_init(&buf, b1, NULL, px);
    lv_disp_drv_init(&drv);
    drv.hor_res = LCD_H_RES;
    drv.ver_res = LCD_V_RES;
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

/* ------------------------------------------------------------------ init */
void hw_init(void)
{
    i2c_init();
    tca_init();
    i2c_scan();
    backlight_init();
    buzzer_init();
    st7701_init();
    panel_init();
    touch_init();
    lvgl_init();
}
