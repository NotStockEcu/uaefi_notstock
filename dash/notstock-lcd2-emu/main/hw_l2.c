/* RP2350-Touch-LCD-2 board layer, see hw_l2.h. The panel's init sequence is
 * Waveshare's (LCD_2in.c); the pixels go out by DMA in 16-bit SPI frames,
 * so LVGL's RGB565 needs no byte swap. */
#include "hw_l2.h"

#include <stdbool.h>
#include <stdio.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "lvgl.h"
#include "pico/stdlib.h"

#define W 320
#define H 240
#define LCD_SPI   spi0
#define LCD_HZ    (75 * 1000 * 1000)    /* clk_peri / 2, the most SPI does */
#define TP_I2C    i2c0
#define TP_ADDR   0x15
#define BUF_LINES 40

/* ------------------------------------------------------------- panel */
static void lcd_cmd(uint8_t c, const uint8_t *d, int n)
{
    spi_set_format(LCD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(L2_PIN_LCD_CS, 0);
    gpio_put(L2_PIN_LCD_DC, 0);
    spi_write_blocking(LCD_SPI, &c, 1);
    if (n) {
        gpio_put(L2_PIN_LCD_DC, 1);
        spi_write_blocking(LCD_SPI, d, n);
    }
    gpio_put(L2_PIN_LCD_CS, 1);
}

#define CMD(c, ...) do { static const uint8_t d_[] = { __VA_ARGS__ }; \
                         lcd_cmd(c, d_, sizeof d_); } while (0)

static void lcd_init(void)
{
    gpio_put(L2_PIN_LCD_RST, 1);
    sleep_ms(50);
    gpio_put(L2_PIN_LCD_RST, 0);
    sleep_ms(50);
    gpio_put(L2_PIN_LCD_RST, 1);
    sleep_ms(120);

    lcd_cmd(0x11, NULL, 0);                    /* out of sleep */
    sleep_ms(120);
    /* MADCTL: MV (landscape) + BGR, as the demo's HORIZONTAL; flipped:
     * MX + MY on top */
    CMD(0x36, L2_FLIP ? 0xE8 : 0x28);
    CMD(0x3A, 0x05);                           /* 16 bit / pixel */
    CMD(0xF0, 0xC3);
    CMD(0xF0, 0x96);
    CMD(0xB4, 0x01);
    CMD(0xB7, 0xC6);
    CMD(0xC0, 0x80, 0x45);
    CMD(0xC1, 0x13);
    CMD(0xC2, 0xA7);
    CMD(0xC5, 0x0A);
    CMD(0xE8, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33);
    CMD(0xE0, 0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30, 0x33, 0x47, 0x17,
        0x13, 0x13, 0x2B, 0x31);
    CMD(0xE1, 0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F, 0x33, 0x47, 0x38,
        0x15, 0x16, 0x2C, 0x32);
    CMD(0xF0, 0x3C);
    CMD(0xF0, 0x69);
    sleep_ms(120);
    lcd_cmd(0x21, NULL, 0);                    /* inversion on (IPS) */
    lcd_cmd(0x29, NULL, 0);                    /* display on */
}

static int s_dma = -1;
static lv_disp_drv_t *s_flushing;

static void dma_done(void)
{
    if (!dma_channel_get_irq0_status(s_dma)) return;
    dma_channel_acknowledge_irq0(s_dma);
    /* the last halfword still leaves the FIFO */
    while (spi_is_busy(LCD_SPI)) tight_loop_contents();
    gpio_put(L2_PIN_LCD_CS, 1);
    if (s_flushing) {
        lv_disp_drv_t *d = s_flushing;
        s_flushing = NULL;
        lv_disp_flush_ready(d);
    }
}

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px)
{
    uint8_t ca[4] = { a->x1 >> 8, a->x1 & 0xFF, a->x2 >> 8, a->x2 & 0xFF };
    uint8_t ra[4] = { a->y1 >> 8, a->y1 & 0xFF, a->y2 >> 8, a->y2 & 0xFF };
    lcd_cmd(0x2A, ca, 4);
    lcd_cmd(0x2B, ra, 4);
    lcd_cmd(0x2C, NULL, 0);
    uint32_t n = (uint32_t)(a->x2 - a->x1 + 1) * (a->y2 - a->y1 + 1);
    /* pixel data: 16-bit frames, MSB first, as the panel wants RGB565 */
    spi_set_format(LCD_SPI, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(L2_PIN_LCD_DC, 1);
    gpio_put(L2_PIN_LCD_CS, 0);
    s_flushing = drv;
    dma_channel_set_read_addr(s_dma, px, false);
    dma_channel_set_trans_count(s_dma, n, true);
}

/* ------------------------------------------------------------- touch */
static bool tp_read(uint16_t *x, uint16_t *y)
{
    uint8_t reg = 0x02, d[5];                  /* fingers, X hi/lo, Y hi/lo */
    if (i2c_write_timeout_us(TP_I2C, TP_ADDR, &reg, 1, true, 2000) != 1) return false;
    if (i2c_read_timeout_us(TP_I2C, TP_ADDR, d, 5, false, 3000) != 5) return false;
    if ((d[0] & 0x0F) == 0) return false;
    int rx = ((d[1] & 0x0F) << 8) | d[2];      /* panel native: 240 wide */
    int ry = ((d[3] & 0x0F) << 8) | d[4];      /* 320 high */
    /* to landscape, as the demo's CST816D_Get_Point() */
    int lx = ry, ly = (H - 1) - rx;
    if (L2_FLIP) {
        lx = (W - 1) - lx;
        ly = (H - 1) - ly;
    }
    if (lx < 0) lx = 0;
    if (lx >= W) lx = W - 1;
    if (ly < 0) ly = 0;
    if (ly >= H) ly = H - 1;
    *x = (uint16_t)lx;
    *y = (uint16_t)ly;
    return true;
}

static void touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    static uint16_t lx, ly;
    uint16_t x, y;
    if (tp_read(&x, &y)) {
        lx = x;
        ly = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point.x = lx;
    data->point.y = ly;
}

static void tp_init(void)
{
    /* the reset is the panel's (GPIO20), already pulsed by lcd_init */
    sleep_ms(50);
    uint8_t id_reg = 0xA7, id = 0;
    i2c_write_timeout_us(TP_I2C, TP_ADDR, &id_reg, 1, true, 2000);
    i2c_read_timeout_us(TP_I2C, TP_ADDR, &id, 1, false, 2000);
    printf("touch: chip id 0x%02X%s\n", id, id == 0xB6 ? " (CST816D)" : " (?)");
    static const uint8_t cfg[][2] = {
        { 0xFE, 0x01 },                        /* no auto sleep */
        { 0xFA, 0x41 },                        /* IRQ on touch + change */
        { 0xED, 0x01 },
        { 0xEE, 0x01 },
    };
    for (unsigned i = 0; i < sizeof cfg / sizeof cfg[0]; i++) {
        i2c_write_timeout_us(TP_I2C, TP_ADDR, cfg[i], 2, false, 2000);
    }
}

/* --------------------------------------------------------- backlight */
void hw_backlight(uint8_t percent)
{
    if (percent > 100) percent = 100;
    pwm_set_gpio_level(L2_PIN_LCD_BL, percent);
}

/* --------------------------------------------------------------- init */
void hw_init(void)
{
    gpio_init(L2_PIN_LCD_CS);
    gpio_set_dir(L2_PIN_LCD_CS, GPIO_OUT);
    gpio_put(L2_PIN_LCD_CS, 1);
    gpio_init(L2_PIN_LCD_DC);
    gpio_set_dir(L2_PIN_LCD_DC, GPIO_OUT);
    gpio_init(L2_PIN_LCD_RST);
    gpio_set_dir(L2_PIN_LCD_RST, GPIO_OUT);
    gpio_put(L2_PIN_LCD_RST, 1);

    /* backlight off until there is a picture */
    gpio_set_function(L2_PIN_LCD_BL, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(L2_PIN_LCD_BL);
    pwm_set_wrap(slice, 100);
    pwm_set_clkdiv(slice, 50);                 /* ~30 kHz */
    pwm_set_gpio_level(L2_PIN_LCD_BL, 0);
    pwm_set_enabled(slice, true);

    unsigned hz = spi_init(LCD_SPI, LCD_HZ);
    gpio_set_function(L2_PIN_LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(L2_PIN_LCD_MOSI, GPIO_FUNC_SPI);
    printf("lcd: SPI %u kHz\n", hz / 1000);

    i2c_init(TP_I2C, 400 * 1000);
    gpio_set_function(L2_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(L2_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(L2_PIN_SDA);
    gpio_pull_up(L2_PIN_SCL);

    lcd_init();
    tp_init();

    s_dma = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(s_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, spi_get_dreq(LCD_SPI, true));
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    dma_channel_configure(s_dma, &c, &spi_get_hw(LCD_SPI)->dr, NULL, 0, false);
    dma_channel_set_irq0_enabled(s_dma, true);
    irq_add_shared_handler(DMA_IRQ_0, dma_done, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(DMA_IRQ_0, true);

    lv_init();
    static lv_disp_draw_buf_t db;
    static lv_color_t b1[W * BUF_LINES], b2[W * BUF_LINES];
    lv_disp_draw_buf_init(&db, b1, b2, W * BUF_LINES);
    static lv_disp_drv_t dd;
    lv_disp_drv_init(&dd);
    dd.hor_res = W;
    dd.ver_res = H;
    dd.flush_cb = flush_cb;
    dd.draw_buf = &db;
    lv_disp_drv_register(&dd);

    static lv_indev_drv_t id;
    lv_indev_drv_init(&id);
    id.type = LV_INDEV_TYPE_POINTER;
    id.read_cb = touch_cb;
    lv_indev_drv_register(&id);
}
