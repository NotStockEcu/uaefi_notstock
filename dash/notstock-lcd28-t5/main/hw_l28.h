/* The Waveshare RP2350-Touch-LCD-2.8 under LVGL: the 2.8" panel (240 x 320,
 * ST7789 on SPI1, run landscape 320 x 240, DMA), the CST328 touch (I2C1,
 * shared with the QMI8658 and the PCF85063), the backlight (PWM). Pins from
 * Waveshare's schematic (its pin table):
 *   LCD   SCK 10, MOSI 11, (MISO 12), CS 13, DC 14, RST 15, backlight 16
 *   I2C1  SDA 6, SCL 7: touch 0x1A (RST 17, INT 18), IMU INT 8 / 9,
 *         RTC INT 5
 *   I2S to the PCM5101A: BCK 2, LRCK 3, DIN 4
 *   SD card 19 .. 24, battery: key 25, enable 26, ADC 27
 *   free on the connectors: GPIO0 / 1 (UART), 28, 29
 */
#pragma once
#include <stdint.h>

#define L28_PIN_LCD_SCK  10
#define L28_PIN_LCD_MOSI 11
#define L28_PIN_LCD_CS   13
#define L28_PIN_LCD_DC   14
#define L28_PIN_LCD_RST  15
#define L28_PIN_LCD_BL   16
#define L28_PIN_SDA      6
#define L28_PIN_SCL      7
#define L28_PIN_TP_RST   17
#define L28_PIN_TP_INT   18

/* the panel turned: 0 landscape, 1 the same upside down */
#ifndef L28_FLIP
#define L28_FLIP 0
#endif
/* the panel's colour order: 1 if red and blue come out swapped */
#ifndef L28_BGR
#define L28_BGR 0
#endif

void hw_init(void);                 /* panel, touch, LVGL display + input */
void hw_backlight(uint8_t percent); /* 0..100 */
