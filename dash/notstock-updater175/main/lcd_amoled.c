/* The updater's screen on the ESP32-S3-Touch-AMOLED-1.75 (lcd_text.h): the
 * CO5300 over QSPI set up as ../../notstock-round-amoled/main/hw_amoled.c
 * does, text from font.h drawn into a 466 x 466 frame in PSRAM. The
 * updater lays its lines out for the 1.85"'s 360 rows: y is scaled to 466. */
#include "lcd_text.h"
#include "font.h"
#include "board_amoled.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "lcd";

#define STRIP   40                       /* rows per transfer (even) */
#define OP_CMD   0x02
#define OP_PIXEL 0x32
#define Y(y)    ((y) * LCD_V_RES / 360)  /* the updater's 360-row layout */

static esp_lcd_panel_io_handle_t s_io;
static SemaphoreHandle_t s_sent;
static uint16_t *s_fb;                   /* RGB565, native order */
static uint16_t *s_strip;                /* DMA, panel order (high byte first) */

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

static void cmd(uint8_t c, const uint8_t *p, size_t n)
{
    esp_lcd_panel_io_tx_param(s_io, (OP_CMD << 24) | (c << 8), p, n);
}

static void panel_init(void)
{
    gpio_config_t rst = { .pin_bit_mask = 1ULL << PIN_LCD_RST, .mode = GPIO_MODE_OUTPUT };
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
        .max_transfer_sz = LCD_H_RES * STRIP * 2 + 64,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    const esp_lcd_panel_io_spi_config_t io = {
        .cs_gpio_num = PIN_LCD_CS,
        .dc_gpio_num = -1,
        .spi_mode = 0,
        .pclk_hz = LCD_PCLK_HZ,
        .trans_queue_depth = 4,
        .on_color_trans_done = sent_cb,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags = { .quad_mode = 1 },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io, &s_io));

    /* Waveshare's set-up, as hw_amoled.c; dark until the first frame */
    static const struct { uint8_t cmd, n, d[4]; uint16_t ms; } INIT[] = {
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
        { 0x11, 0, { 0 }, 300 },            /* out of sleep */
        { 0x29, 0, { 0 }, 20 },             /* on */
    };
    for (size_t i = 0; i < sizeof INIT / sizeof INIT[0]; i++) {
        cmd(INIT[i].cmd, INIT[i].n ? INIT[i].d : NULL, INIT[i].n);
        if (INIT[i].ms) vTaskDelay(pdMS_TO_TICKS(INIT[i].ms));
    }
    ESP_LOGI(TAG, "CO5300 up");
}

void lcd_init(void)
{
    s_sent = xSemaphoreCreateBinary();
    s_fb = heap_caps_calloc(LCD_H_RES * LCD_V_RES, 2, MALLOC_CAP_SPIRAM);
    s_strip = heap_caps_malloc(LCD_H_RES * STRIP * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(s_fb && s_strip);
    panel_init();
    lcd_show();                          /* black before the light */
    const uint8_t full = 0xFF;
    cmd(0x51, &full, 1);
}

void lcd_clear(void)
{
    memset(s_fb, 0, LCD_H_RES * LCD_V_RES * 2);
}

void lcd_show(void)
{
    for (int y = 0; y < LCD_V_RES; y += STRIP) {
        int n = LCD_V_RES - y < STRIP ? LCD_V_RES - y : STRIP;
        const uint8_t c[4] = { 0, LCD_X_GAP, (LCD_X_GAP + LCD_H_RES - 1) >> 8,
                               (LCD_X_GAP + LCD_H_RES - 1) & 0xFF };
        const uint8_t r[4] = { y >> 8, y & 0xFF, (y + n - 1) >> 8, (y + n - 1) & 0xFF };
        cmd(0x2A, c, 4);
        cmd(0x2B, r, 4);
        const uint16_t *s = s_fb + y * LCD_H_RES;
        for (int i = 0; i < n * LCD_H_RES; i++) s_strip[i] = __builtin_bswap16(s[i]);
        esp_lcd_panel_io_tx_color(s_io, (OP_PIXEL << 24) | (0x2C << 8), s_strip,
                                  n * LCD_H_RES * 2);
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
    y = Y(y);
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
    const int w = 260, h = 14, x0 = (LCD_H_RES - w) / 2;
    y = Y(y);
    for (int r = 0; r < h; r++) {
        uint16_t *row = s_fb + (y + r) * LCD_H_RES + x0;
        for (int c = 0; c < w; c++) {
            bool edge = r == 0 || r == h - 1 || c == 0 || c == w - 1;
            bool in = percent >= 0 && c < w * percent / 100;
            row[c] = in ? color : edge ? C_GREY : 0;
        }
    }
}
