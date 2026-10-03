/* Waveshare ESP32-S3-Touch-LCD-2.1: 480x480 round ST7701 on RGB, CST820
 * touch, TCA9554 I/O expander, all on one I2C bus.
 *
 * Pin map as used by an open project for this board (Selbyl's Plane Radar,
 * LovyanGFX config) and the Waveshare pin table:
 *   ST7701 set-up: 3-wire SPI, SDA GPIO1, SCL GPIO2, CS EXIO3, reset EXIO1
 *   RGB: HSYNC 38, VSYNC 39, DE 40, PCLK 41, 16 data lines below
 *   I2C: SCL GPIO7, SDA GPIO15. TCA9554 0x20, CST820 0x15 (INT GPIO16,
 *   reset EXIO2). Buzzer EXIO8. Backlight GPIO6 (PWM).
 *
 * CAN: the SN65HVD230 board goes on the 4-pin UART header, GPIO43 (TXD) to
 * its CTX and GPIO44 (RXD) to its CRX. Those are the only free pins. The
 * console therefore runs on the native USB (USB Serial/JTAG), never UART0.
 */
#pragma once

#define BOARD_NAME "ESP32-S3-Touch-LCD-2.1"

#define LCD_H_RES 480
#define LCD_V_RES 480
#define LCD_PCLK_HZ (16 * 1000 * 1000)

#define PIN_LCD_HSYNC 38
#define PIN_LCD_VSYNC 39
#define PIN_LCD_DE    40
#define PIN_LCD_PCLK  41
/* esp_lcd order: B0..B4, G0..G5, R0..R4 */
#define PIN_LCD_DATA { \
     5, 45, 48, 47, 21,           /* B0..B4 */ \
    14, 13, 12, 11, 10,  9,       /* G0..G5 */ \
    46,  3,  8, 18, 17            /* R0..R4 */ \
}

/* ST7701 set-up interface, bit-banged */
#define PIN_ST_SDA 1
#define PIN_ST_SCL 2

#define PIN_I2C_SCL 7
#define PIN_I2C_SDA 15
#define PIN_TP_INT  16
#define PIN_BL      6

#define TCA9554_ADDR 0x20
#define CST820_ADDR  0x15

/* TCA9554 outputs, EXIO1..8 = bit 0..7 */
#define EXIO_LCD_RST (1 << 0)
#define EXIO_TP_RST  (1 << 1)
#define EXIO_LCD_CS  (1 << 2)
#define EXIO_SD_CS   (1 << 3)
#define EXIO_BUZZER  (1 << 7)

#define PIN_TWAI_TX 43
#define PIN_TWAI_RX 44

/* touch to screen: flip these if a tap lands mirrored */
#define TOUCH_SWAP_XY  0
#define TOUCH_MIRROR_X 0
#define TOUCH_MIRROR_Y 0
