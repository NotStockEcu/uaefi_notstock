/* NOT STOCK dash - Waveshare ESP32-S3-Touch-LCD-7 + rusEFI verbose CAN
 *
 * Build: idf.py set-target esp32s3 && idf.py build flash monitor
 * Flash over the UART Type-C port, not the OTG one (OTG shares the CAN pins).
 */
#include <string.h>

#include "board.h"
#include "boot_anim.h"
#include "rusefi_can.h"
#include "settings.h"
#include "touch.h"
#include "ui.h"

#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "dash";

/* Set to true to run the layout off a synthetic generator, no CAN needed. */
#define START_IN_DEMO false

#define LVGL_BUF_LINES 40
#define LVGL_TICK_MS   2

static esp_lcd_panel_handle_t s_panel;
/* While set, LVGL renders into this buffer instead of the panel. The boot
 * animation uses it to get a finished dash frame to crossfade into. */
static uint16_t *s_shadow;
static uint8_t s_exio;

/* ------------------------------------------------------------ CH422G / I2C */
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
}

/* The CH422G is addressed oddly: the register *is* the I2C address and the
 * transfer carries a single data byte. */
static void ch422g_raw(uint8_t addr, uint8_t val)
{
    ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_NUM_0, addr, &val, 1,
                                              pdMS_TO_TICKS(100)));
}

void exio_set(uint8_t mask, bool on)
{
    if (on) s_exio |= mask;
    else    s_exio &= (uint8_t)~mask;
    ch422g_raw(CH422G_ADDR_OUT, s_exio);
}

/* Walks the bus and logs what answers. The quickest way to tell a wiring or
 * address problem from a driver problem when a board misbehaves. */
static void i2c_scan(void)
{
    char line[128];
    int n = 0;
    int len = snprintf(line, sizeof line, "i2c devices:");
    for (int a = 0x08; a < 0x78; a++) {
        i2c_cmd_handle_t c = i2c_cmd_link_create();
        i2c_master_start(c);
        i2c_master_write_byte(c, (a << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(c);
        esp_err_t e = i2c_master_cmd_begin(I2C_NUM_0, c, pdMS_TO_TICKS(20));
        i2c_cmd_link_delete(c);
        if (e == ESP_OK && len < (int)sizeof line - 8) {
            len += snprintf(line + len, sizeof line - len, " 0x%02X", a);
            n++;
        }
    }
    ESP_LOGI(TAG, "%s%s", line, n ? "" : " none found");
}

static void ch422g_init(void)
{
    ch422g_raw(CH422G_ADDR_MODE, CH422G_MODE_PUSH_PULL);

    /* Backlight stays off until the panel is running so the first frame is
     * not a screenful of noise. */
    s_exio = EXIO_TP_RST | EXIO_LCD_RST | EXIO_SD_CS;
#if BOARD_HAS_CAN_SEL
    /* 7 inch only: hands GPIO19/20 to the CAN transceiver instead of USB.
     * The 5 inch has CAN on its own pins and uses this bit as an isolated
     * input, so driving it there would be wrong. */
    s_exio |= EXIO_CAN_SEL;
#endif
    ch422g_raw(CH422G_ADDR_OUT, s_exio);
    vTaskDelay(pdMS_TO_TICKS(100));
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
        /* Bounce buffer in internal RAM keeps the panel fed while the CPU
         * hammers PSRAM. Without it the display drifts sideways. */
        .bounce_buffer_size_px = LCD_H_RES * 10,
        .psram_trans_align = 64,
        .sram_trans_align = 4,
        .hsync_gpio_num = PIN_LCD_HSYNC,
        .vsync_gpio_num = PIN_LCD_VSYNC,
        .de_gpio_num = PIN_LCD_DE,
        .pclk_gpio_num = PIN_LCD_PCLK,
        .disp_gpio_num = -1,            /* backlight is on the CH422G */
        .timings = {
            .pclk_hz = LCD_PCLK_HZ,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 16,
            .vsync_front_porch = 16,
            .flags.pclk_active_neg = true,
        },
        .flags.fb_in_psram = true,
    };
    memcpy(cfg.data_gpio_nums, data_pins, sizeof(data_pins));

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_LOGI(TAG, "RGB panel up, %dx%d", LCD_H_RES, LCD_V_RES);
}

/* ------------------------------------------------------------------- LVGL */
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                     lv_color_t *px)
{
    if (s_shadow) {
        int w = area->x2 - area->x1 + 1;
        for (int y = area->y1; y <= area->y2; y++) {
            memcpy(s_shadow + y * LCD_H_RES + area->x1, px, w * 2);
            px += w;
        }
    } else {
        esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1,
                                  area->x2 + 1, area->y2 + 1, px);
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
    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_disp_drv_t drv;

    size_t px = LCD_H_RES * LVGL_BUF_LINES;
    lv_color_t *b1 = heap_caps_malloc(px * sizeof(lv_color_t),
                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(b1);
    lv_disp_draw_buf_init(&draw_buf, b1, NULL, px);

    lv_disp_drv_init(&drv);
    drv.hor_res = LCD_H_RES;
    drv.ver_res = LCD_V_RES;
    drv.flush_cb = flush_cb;
    drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&drv);

    const esp_timer_create_args_t t = {
        .callback = tick_cb, .name = "lv_tick",
    };
    esp_timer_handle_t h;
    ESP_ERROR_CHECK(esp_timer_create(&t, &h));
    ESP_ERROR_CHECK(esp_timer_start_periodic(h, LVGL_TICK_MS * 1000));
}

/* ------------------------------------------------------------ boot screen */
/* Logo rises out of black, holds, crossfades into the dash. The frames are
 * written straight into the panel's frame buffer by boot_anim.c; going
 * through LVGL made the fade stutter. During the hold LVGL renders the dash
 * into a shadow buffer, so the crossfade has a finished frame to blend into
 * and ends on exactly what LVGL believes is on screen. */
static void fb_present(uint16_t *fb)
{
    /* the frame buffer is its own draw buffer: the driver only syncs */
    esp_lcd_panel_draw_bitmap(s_panel, 0, 0, LCD_H_RES, LCD_V_RES, fb);
}

static void boot_run(void)
{
    uint16_t *fb = NULL;
    if (esp_lcd_rgb_panel_get_frame_buffer(s_panel, 1, (void **)&fb) != ESP_OK
        || fb == NULL) {
        ESP_LOGW(TAG, "no frame buffer, skipping the boot logo");
        lv_timer_handler();
        exio_set(EXIO_LCD_BL, true);
        return;
    }

    memset(fb, 0, LCD_H_RES * LCD_V_RES * 2);
    fb_present(fb);
    exio_set(EXIO_LCD_BL, true);

    int64_t t0 = esp_timer_get_time();
    int last = -1;
    for (;;) {
        uint32_t ms = (uint32_t)((esp_timer_get_time() - t0) / 1000);
        int lv = boot_in_level(ms);
        if (lv != last) {
            boot_draw_logo(fb, lv);
            fb_present(fb);
            last = lv;
        }
        if (ms >= BOOT_IN_MS) break;
        vTaskDelay(1);
    }

    uint16_t *shadow = heap_caps_malloc(LCD_H_RES * LCD_V_RES * 2,
                                        MALLOC_CAP_SPIRAM);
    if (!shadow) {
        ESP_LOGW(TAG, "no memory for the crossfade, cutting to the dash");
        vTaskDelay(pdMS_TO_TICKS(BOOT_HOLD_MS));
        lv_obj_invalidate(lv_scr_act());
        return;
    }

    /* hold: LVGL runs normally but draws into the shadow, so the needles
     * settle on live values while the logo is up */
    s_shadow = shadow;
    lv_obj_invalidate(lv_scr_act());
    int64_t hold_end = t0 + (int64_t)(BOOT_IN_MS + BOOT_HOLD_MS) * 1000;
    while (esp_timer_get_time() < hold_end) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);

    int64_t t1 = esp_timer_get_time();
    last = -1;
    for (;;) {
        uint32_t ms = (uint32_t)((esp_timer_get_time() - t1) / 1000);
        int lv = boot_fade_level(ms);
        if (lv != last) {
            boot_draw_cross(fb, shadow, lv);
            fb_present(fb);
            last = lv;
        }
        if (lv >= 32) break;
        vTaskDelay(1);
    }

    /* the frame buffer now holds exactly the last LVGL frame */
    s_shadow = NULL;
    heap_caps_free(shadow);
}

/* ------------------------------------------------------------------- main */
void app_main(void)
{
    ESP_LOGI(TAG, "NOT STOCK dash, board %s", BOARD_NAME);
    settings_load();

    i2c_init();
    i2c_scan();
    ch422g_init();
    panel_init();
    lvgl_init();

    if (touch_init()) {
        touch_register_lvgl();
    } else {
        ESP_LOGW(TAG, "no touch, settings menu unreachable");
    }

    ui_create();

    /* Demo mode is a setting now, so CAN always comes up: switching the
     * setting off in the menu makes live data appear without a reflash.
     * Started before the boot logo so the dash fades in on live values. */
    rusefi_can_start();

    boot_run();     /* also switches the backlight on */

    while (1) {
        uint32_t next = lv_timer_handler();
        if (next == LV_NO_TIMER_READY || next > 20) next = 20;
        if (next < 2) next = 2;
        vTaskDelay(pdMS_TO_TICKS(next));
    }
}
