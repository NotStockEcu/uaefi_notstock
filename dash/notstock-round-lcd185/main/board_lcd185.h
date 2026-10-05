/* Waveshare ESP32-S3-Touch-LCD-1.85: 360 x 360 round LCD, ST77916 on QSPI,
 * CST816 touch, TCA9554 I/O expander, PCM5101 audio DAC with a speaker
 * amplifier.
 *
 * Pins from Waveshare's demo for the 1.85/1.85C (ST77916.c, CST816.c,
 * TCA9554PWR.c) and the ESPHome configs for the board:
 *   ST77916 QSPI: CLK GPIO40, D0..3 GPIO46/45/42/41, CS GPIO21, TE GPIO18,
 *   reset on EXIO2. Backlight GPIO5 (PWM).
 *   I2C: SCL GPIO10, SDA GPIO11 (board V2). TCA9554 0x20, CST816 0x15
 *   (INT GPIO4, reset EXIO1). Board V1 had the touch on its own pins,
 *   SDA GPIO1 / SCL GPIO3: hw_lcd185.c looks there when it is not found.
 *   I2S to the PCM5101: BCK GPIO48, LRCK GPIO38, DOUT GPIO47 (no MCLK).
 *
 * CAN: on the 28-pin 1.27 mm header (2 x 14), as Waveshare's 1.85C
 * schematic numbers it: GPIO12 (pin 20) to the transceiver's TXD, GPIO13
 * (pin 18) from its RXD; 5 V in on pin 1 (USB_5V), 3V3 on pins 9/10, GND
 * on 3/4/11/12. Check against the board's own silkscreen first.
 */
#pragma once

#define BOARD_NAME "ESP32-S3-Touch-LCD-1.85"

#define LCD_H_RES 360
#define LCD_V_RES 360
#define LCD_PCLK_HZ (80 * 1000 * 1000)

#define PIN_LCD_CS   21
#define PIN_LCD_CLK  40
#define PIN_LCD_D0   46
#define PIN_LCD_D1   45
#define PIN_LCD_D2   42
#define PIN_LCD_D3   41
#define PIN_BL       5

#define PIN_I2C_SCL  10
#define PIN_I2C_SDA  11
#define PIN_TP_SCL_V1 3
#define PIN_TP_SDA_V1 1
#define PIN_TP_INT   4

#define TCA9554_ADDR 0x20
#define CST816_ADDR  0x15
#define EXIO_TP_RST  (1 << 0)      /* EXIO1 */
#define EXIO_LCD_RST (1 << 1)      /* EXIO2 */

#define PIN_I2S_BCK  48
#define PIN_I2S_WS   38
#define PIN_I2S_DOUT 47

#define PIN_TWAI_TX 12      /* header pin 20 */
#define PIN_TWAI_RX 13      /* header pin 18 */

/* touch to screen: flip these if a tap lands mirrored */
#define TOUCH_SWAP_XY  0
#define TOUCH_MIRROR_X 0
#define TOUCH_MIRROR_Y 0
