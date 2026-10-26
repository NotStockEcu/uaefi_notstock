/* The updater's screen on the Waveshare ESP32-S3-Touch-LCD-1.85: I2C, the
 * TCA9554 (the panel's reset), the backlight and the ST77916 set up as in
 * ../../notstock-round-lcd185/main/hw_lcd185.c; text from font.h (8 bit
 * alpha, generated) drawn into a 360 x 360 frame in PSRAM. */
#include "lcd_text.h"
#include "font.h"
#include "board_lcd185.h"
#include "esp_lcd_st77916.h"

#include <string.h>

#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "lcd";

#define STRIP 40                         /* rows per transfer */

static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_sent;
static uint16_t *s_fb;                   /* RGB565, native order */
static uint16_t *s_strip;                /* DMA, panel order (high byte first) */

/* ------------------------------------------------- I2C and the expander */
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

static void exio(uint8_t reg, uint8_t v)
{
    const uint8_t d[2] = { reg, v };
    i2c_master_write_to_device(I2C_NUM_0, TCA9554_ADDR, d, 2, pdMS_TO_TICKS(50));
}

/* both resets low, the two pins as outputs, then out of reset */
static void tca_init(void)
{
    exio(0x01, 0);
    exio(0x03, (uint8_t)~(EXIO_TP_RST | EXIO_LCD_RST));
    vTaskDelay(pdMS_TO_TICKS(10));
    exio(0x01, EXIO_TP_RST | EXIO_LCD_RST);
    vTaskDelay(pdMS_TO_TICKS(60));
}

static void backlight(uint32_t duty)
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
        .duty = duty,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&c));
}

/* ---------------------------------------------------------------- panel */
static bool sent_cb(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *ed,
                    void *ctx)
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
        .max_transfer_sz = LCD_H_RES * STRIP * 2 + 64,
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
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &h));
    uint8_t id[4] = { 0 };
    esp_lcd_panel_io_rx_param(h, (0x0B << 24) | (0x04 << 8), id, sizeof id);
    ESP_ERROR_CHECK(esp_lcd_panel_io_del(h));
    ESP_LOGI(TAG, "ST77916 ID %02X %02X %02X %02X", id[0], id[1], id[2], id[3]);

    io.pclk_hz = LCD_PCLK_HZ;
    io.on_color_trans_done = sent_cb;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &h));
    static st77916_vendor_config_t vendor = {
        .flags = { .use_qspi_interface = 1 },
    };
    if (id[0] == 0x00 && id[1] == 0x02 && id[2] == 0x7F && id[3] == 0x7F) {
        vendor.init_cmds = st77916_init_new;
        vendor.init_cmds_size = (uint16_t)st77916_init_new_n;
    }
    const esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(h, &dev, &s_panel));
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_disp_on_off(s_panel, true);
}

void lcd_init(void)
{
    s_sent = xSemaphoreCreateBinary();
    s_fb = heap_caps_calloc(LCD_H_RES * LCD_V_RES, 2, MALLOC_CAP_SPIRAM);
    s_strip = heap_caps_malloc(LCD_H_RES * STRIP * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(s_fb && s_strip);
    i2c_init();
    tca_init();
    backlight(0);
    panel_init();
    lcd_show();                          /* black before the light */
    backlight(1023);
}

void lcd_clear(void)
{
    memset(s_fb, 0, LCD_H_RES * LCD_V_RES * 2);
}

void lcd_show(void)
{
    for (int y = 0; y < LCD_V_RES; y += STRIP) {
        int n = LCD_V_RES - y < STRIP ? LCD_V_RES - y : STRIP;
        const uint16_t *s = s_fb + y * LCD_H_RES;
        for (int i = 0; i < n * LCD_H_RES; i++) s_strip[i] = __builtin_bswap16(s[i]);
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES, y + n, s_strip);
        xSemaphoreTake(s_sent, pdMS_TO_TICKS(200));
    }
}

/* ----------------------------------------------------------------- text */
static uint32_t utf8_next(const char **p)
{
    const uint8_t *s = (const uint8_t *)*p;
    uint32_t c = *s++;
    if (c >= 0xE0 && s[0] && s[1]) {
        c = ((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F);
        s += 2;
    } else if (c >= 0xC0 && s[0]) {
        c = ((c & 0x1F) << 6) | (s[0] & 0x3F);
        s += 1;
    }
    *p = (const char *)s;
    return c;
}

static const glyph_t *glyph(const font_t *f, uint32_t cp)
{
    for (int i = 0; i < f->n; i++) {
        if (f->g[i].cp == cp) return &f->g[i];
    }
    return &f->g['?' - 32];
}

static inline uint16_t blend(uint16_t bg, uint16_t fg, uint8_t a)
{
    int r = ((fg >> 11) * a + (bg >> 11) * (255 - a)) / 255;
    int g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * (255 - a)) / 255;
    int b = ((fg & 31) * a + (bg & 31) * (255 - a)) / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void lcd_text(int y, bool big, uint16_t color, const char *utf8)
{
    const font_t *f = big ? &font_big : &font_small;
    int w = 0;
    for (const char *p = utf8; *p;) w += glyph(f, utf8_next(&p))->adv;
    int x = (LCD_H_RES - w) / 2;
    for (const char *p = utf8; *p;) {
        const glyph_t *g = glyph(f, utf8_next(&p));
        const uint8_t *bm = f->bm + g->off;
        for (int r = 0; r < g->h; r++) {
            int py = y + g->y + r;
            if (py < 0 || py >= LCD_V_RES) continue;
            for (int c = 0; c < g->w; c++) {
                int px = x + g->x + c;
                uint8_t a = bm[r * g->w + c];
                if (a && px >= 0 && px < LCD_H_RES) {
                    uint16_t *d = &s_fb[py * LCD_H_RES + px];
                    *d = blend(*d, color, a);
                }
            }
        }
        x += g->adv;
    }
}

void lcd_bar(int y, int percent, uint16_t color)
{
    const int w = 220, h = 12, x0 = (LCD_H_RES - w) / 2;
    for (int r = 0; r < h; r++) {
        uint16_t *row = s_fb + (y + r) * LCD_H_RES + x0;
        for (int c = 0; c < w; c++) {
            bool edge = r == 0 || r == h - 1 || c == 0 || c == w - 1;
            bool in = percent >= 0 && c < w * percent / 100;
            row[c] = in ? color : edge ? C_GREY : 0;
        }
    }
}
