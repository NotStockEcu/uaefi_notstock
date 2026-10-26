/* Waveshare ESP32-S3-Touch-AMOLED-1.75: 466 x 466 round AMOLED, CO5300 on
 * QSPI, CST9217 touch, ES8311 codec with an NS4150B amplifier and a speaker
 * connector, AXP2101 power, all chips on one I2C bus.
 *
 * Pins from Waveshare's hardware reference and maintained ESP-IDF BSP
 * (github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75):
 *   CO5300 QSPI: CS GPIO12, CLK GPIO38, SIO0..3 GPIO4..7, reset GPIO39,
 *   columns start at 6 (the visible 466 of 480)
 *   I2C: SCL GPIO14, SDA GPIO15. CST9217 0x5A (INT GPIO11, reset GPIO40),
 *   ES8311 0x18
 *   I2S to the ES8311: MCLK 42, BCLK 9, WS 45, DOUT 8; amplifier enable 46
 *
 * CAN: the SN65HVD230 board on the 8-pin header, IO17 to its CTX and IO18
 * to its CRX (silkscreen labels); 3V3 and GND feed it, VBUS takes the 5 V
 * supply. The USB-C stays the console and flashing
 * port. No backlight: brightness is a panel command.
 */
#pragma once

#define BOARD_NAME "ESP32-S3-Touch-AMOLED-1.75"

#define LCD_H_RES 466
#define LCD_V_RES 466
#define LCD_X_GAP 6
#define LCD_PCLK_HZ (40 * 1000 * 1000)

#define PIN_LCD_CS   12
#define PIN_LCD_CLK  38
#define PIN_LCD_D0   4
#define PIN_LCD_D1   5
#define PIN_LCD_D2   6
#define PIN_LCD_D3   7
#define PIN_LCD_RST  39

#define PIN_I2C_SCL  14
#define PIN_I2C_SDA  15
#define PIN_TP_INT   11
#define PIN_TP_RST   40

#define CST9217_ADDR 0x5A
#define ES8311_ADDR  0x18

#define PIN_I2S_MCLK 42
#define PIN_I2S_BCLK 9
#define PIN_I2S_WS   45
#define PIN_I2S_DOUT 8
#define PIN_PA       46

#define PIN_TWAI_TX 17      /* header IO17 */
#define PIN_TWAI_RX 18      /* header IO18 */

/* touch to screen: Waveshare's BSP mirrors both; flip these if a tap lands
 * mirrored */
#define TOUCH_SWAP_XY  0
#define TOUCH_MIRROR_X 1
#define TOUCH_MIRROR_Y 1
