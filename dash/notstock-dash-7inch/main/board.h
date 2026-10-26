/* ===========================================================================
 *  This copy of the project is built for the ESP32-S3-Touch-LCD-7 (7 inch).
 *  The 5 inch variant differs only in DASH_BOARD below and in
 *  sdkconfig.defaults (16 MB flash, console on USB Serial/JTAG).
 * ===========================================================================
 *
 * Board support for two Waveshare panels.
 *
 *   BOARD_WS_LCD7   ESP32-S3-Touch-LCD-7   800x480, N8R8
 *   BOARD_WS_LCD5   ESP32-S3-Touch-LCD-5   800x480, N16R8   (SKU 28117)
 *
 * The RGB data and sync pins are identical on both boards, which is why a
 * firmware built for the 7 inch still puts a correct picture on the 5 inch.
 * What differs is CAN and what EXIO5 does, and those are the two things that
 * were broken.
 *
 * Sources:
 *   https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-7
 *   https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-5
 */
#pragma once

#define BOARD_WS_LCD7 7
#define BOARD_WS_LCD5 5

#define DASH_BOARD BOARD_WS_LCD7

#if DASH_BOARD != BOARD_WS_LCD7 && DASH_BOARD != BOARD_WS_LCD5
#error "DASH_BOARD must be BOARD_WS_LCD7 or BOARD_WS_LCD5"
#endif

/* ------------------------------------------------------------ panel, common */
#define LCD_H_RES 800
#define LCD_V_RES 480

/* 21 MHz is what Waveshare's own demo uses on both panels. */
#define LCD_PCLK_HZ (21 * 1000 * 1000)

#define PIN_LCD_VSYNC 3
#define PIN_LCD_HSYNC 46
#define PIN_LCD_DE    5
#define PIN_LCD_PCLK  7

/* RGB565 data order expected by esp_lcd: B0..B4, G0..G5, R0..R4. On both
 * panels the low bits are not wired, so B3..B7 / G2..G7 / R3..R7. */
#define PIN_LCD_DATA { \
    14, 38, 18, 17, 10,         /* B3 B4 B5 B6 B7 */ \
    39,  0, 45, 48, 47, 21,     /* G2 G3 G4 G5 G6 G7 */ \
     1,  2, 42, 41, 40          /* R3 R4 R5 R6 R7 */ \
}

/* -------------------------------------------------------- I2C, touch, common */
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9

/* GT911 capacitive touch shares the I2C bus. INT doubles as the address
 * select line during reset, so it is driven as an output at boot. */
#define PIN_TP_INT  4

/* CH422G: the register is the I2C address itself, one data byte per write. */
#define CH422G_ADDR_MODE 0x24
#define CH422G_ADDR_OUT  0x38
#define CH422G_MODE_PUSH_PULL 0x01

#define EXIO_TP_RST  (1 << 1)
#define EXIO_LCD_BL  (1 << 2)
#define EXIO_LCD_RST (1 << 3)
#define EXIO_SD_CS   (1 << 4)

/* ------------------------------------------------------------------ per board */
#if DASH_BOARD == BOARD_WS_LCD7

#define BOARD_NAME "ESP32-S3-Touch-LCD-7"

/* CAN and USB-OTG share GPIO19/20. EXIO5 on the CH422G selects which one is
 * connected, so the USB-C OTG port is dead while the dash runs. Flash and
 * monitor over the UART Type-C port. */
#define PIN_TWAI_TX 20
#define PIN_TWAI_RX 19

#define EXIO_CAN_SEL (1 << 5)   /* high = CAN, low = USB OTG */
#define BOARD_HAS_CAN_SEL 1

#else   /* BOARD_WS_LCD5 */

#define BOARD_NAME "ESP32-S3-Touch-LCD-5"

/* This board brings CAN out on its own pins, so nothing is shared with USB
 * and both Type-C ports stay usable. */
#define PIN_TWAI_TX 15
#define PIN_TWAI_RX 16

/* EXIO5 here is DI1, an isolated digital INPUT. Driving it as an output the
 * way the 7 inch build does is wrong, and is why this board misbehaved. */
#define BOARD_HAS_CAN_SEL 0

#endif
