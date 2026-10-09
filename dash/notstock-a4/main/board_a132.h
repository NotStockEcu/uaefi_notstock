/* Waveshare ESP32-S3-Touch-AMOLED-1.32, from its schematic: 466 x 466 CO5300
 * AMOLED on QSPI, touch on I2C (CST820, the CST816 register set), ES8311
 * codec with an NS4150B amplifier and a speaker. No IMU.
 *   QSPI: CS GPIO10, CLK 11, D0..D3 12..15, reset 8, TE 9
 *   I2C: SDA GPIO47, SCL 48. Touch 0x15 (INT 6, reset 7), ES8311 0x18
 *   I2S: MCLK 38, SCLK 39, codec out 40, LRCK 41, codec in 42; amplifier 46,
 *   codec power GPIO16
 *   power: BAT_EN GPIO18 keeps the battery switch on, PWR_KEY GPIO17
 *   12-pin header J1: 1 GND, 2 VSYS, 3 3V3, 4 GPIO0, 5 GPIO1, 6 GPIO2,
 *   7 SCL, 8 SDA, 9 USB D-, 10 USB D+, 11 TXD, 12 RXD
 * The same driver as the AMOLED 1.75 (../notstock-round-amoled/main/
 * hw_amoled.c), these pins instead of board_amoled.h.
 */
#pragma once

#define BOARD_NAME "ESP32-S3-Touch-AMOLED-1.32"

#define LCD_H_RES 466
#define LCD_V_RES 466
#define LCD_X_GAP 6           /* as on the 1.75's CO5300; a shifted picture: this */
#define LCD_PCLK_HZ (40 * 1000 * 1000)

#define PIN_LCD_CS   10
#define PIN_LCD_CLK  11
#define PIN_LCD_D0   12
#define PIN_LCD_D1   13
#define PIN_LCD_D2   14
#define PIN_LCD_D3   15
#define PIN_LCD_RST  8

#define PIN_I2C_SCL  48
#define PIN_I2C_SDA  47
#define PIN_TP_INT   6
#define PIN_TP_RST   7

#define TOUCH_CST8XX 1        /* CST820 at 0x15, not the 1.75's CST9217 */
#define CST8XX_ADDR  0x15
#define ES8311_ADDR  0x18

#define PIN_I2S_MCLK 38
#define PIN_I2S_BCLK 39
#define PIN_I2S_WS   41
#define PIN_I2S_DOUT 42       /* to the codec's DSDIN */
#define PIN_PA       46
#define PIN_CODEC_EN 16
#define PIN_BAT_EN   18

/* CAN on the 12-pin header: GPIO0 is a strapping pin, so 1 and 2 */
#define PIN_TWAI_TX 1         /* J1 pin 5 */
#define PIN_TWAI_RX 2         /* J1 pin 6 */

/* mounted with the USB-C at the bottom: the picture turned half round (in
 * hw_amoled.c's flush), the touch with it. 0 for the USB-C at the top. */
#define LCD_ROT180 1

/* touch to screen; flip these if a tap lands mirrored */
#define TOUCH_SWAP_XY  0
#define TOUCH_MIRROR_X LCD_ROT180
#define TOUCH_MIRROR_Y LCD_ROT180
