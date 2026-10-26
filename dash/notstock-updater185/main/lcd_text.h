/* The updater's screen: the 1.85" panel brought up as hw_lcd185.c does,
 * a frame in PSRAM, lines of text centred, a progress bar. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define C_WHITE  RGB(255, 255, 255)
#define C_GREY   RGB(150, 150, 150)
#define C_ORANGE RGB(255, 154, 31)
#define C_GREEN  RGB(60, 210, 90)
#define C_RED    RGB(255, 60, 60)

void lcd_init(void);
void lcd_clear(void);
void lcd_text(int y, bool big, uint16_t color, const char *utf8);   /* centred, y = top */
void lcd_bar(int y, int percent, uint16_t color);                  /* 0..100; < 0: none */
void lcd_show(void);                                               /* the frame out */
