/* Waveshare ESP32-S3-Touch-LCD-1.85: 360 x 360 round LCD, ST77916 on QSPI,
 * CST816 touch, TCA9554 I/O expander, PCM5101 audio DAC with a speaker
 * amplifier.
 *
 * Pins from Waveshare's schematic of the board (ESP32-S3-LCD-1.85) and
 * their demo for it:
 *   ST77916 QSPI: CLK GPIO40, D0..3 GPIO46/45/42/41, CS GPIO21, TE GPIO18,
 *   reset on EXIO2. Backlight GPIO5 (PWM).
 *   I2C GPIO10/11: TCA9554 0x20, IMU, RTC. CST816 0x15 on its own pins,
 *   SDA GPIO1 / SCL GPIO3 (INT GPIO4, reset EXIO1); hw_lcd185.c looks on
 *   10/11 first, as later boards have it there.
 *   I2S to the PCM5101: BCK GPIO48, LRCK GPIO38, DOUT GPIO47 (no MCLK).
 *
 * CAN: through the display's 4-pin 1.0 mm UART socket (pin 1 RXD/GPIO44,
 * 2 TXD/GPIO43, 3 3V3, 4 GND), 1:1 on a JST SH cable to the CAN board
 * (../../notstock-can185). GPIO44 drives the transceiver's TXD; GPIO43
 * reads its RXD through 1 k on that board, as the boot ROM prints on
 * GPIO43 for a moment after reset. The console is on the USB, nothing
 * else may use UART0.
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

#define PIN_TWAI_TX 44      /* UART socket pin 1 (RXD) */
#define PIN_TWAI_RX 43      /* UART socket pin 2 (TXD) */

/* touch to screen: flip these if a tap lands mirrored */
#define TOUCH_SWAP_XY  0
#define TOUCH_MIRROR_X 0
#define TOUCH_MIRROR_Y 0
