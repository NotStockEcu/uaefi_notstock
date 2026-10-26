/* The Waveshare RP2350-Touch-LCD-2 under LVGL: the 2" panel (SPI0, landscape
 * 320 x 240, DMA), the CST816D touch (I2C0, shared with the QMI8658), the
 * backlight (PWM). Pins from Waveshare's schematic and demo (DEV_Config.h):
 *   LCD   SCK 18, MOSI 19, CS 17, DC 16, RST 20 (also the touch reset),
 *         backlight 15
 *   I2C0  SDA 12, SCL 13: touch 0x15 (INT 29), IMU 0x6B (INT1 14)
 *   battery ADC 28, SD card 24..27, camera 0..11, 22, 23
 */
#pragma once
#include <stdint.h>

#define L2_PIN_LCD_SCK  18
#define L2_PIN_LCD_MOSI 19
#define L2_PIN_LCD_CS   17
#define L2_PIN_LCD_DC   16
#define L2_PIN_LCD_RST  20
#define L2_PIN_LCD_BL   15
#define L2_PIN_SDA      12
#define L2_PIN_SCL      13
#define L2_PIN_TP_INT   29

/* the panel turned: 0 as Waveshare's demo (HORIZONTAL), 1 upside down */
#ifndef L2_FLIP
#define L2_FLIP 0
#endif

void hw_init(void);                 /* panel, touch, LVGL display + input */
void hw_backlight(uint8_t percent); /* 0..100 */
