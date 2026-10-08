/* Board drivers, see hw.c */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "ui_round.h"

void hw_init(void);                  /* I2C, expander, panel, touch, LVGL */
void hw_backlight(uint8_t percent);  /* 0..100 */
void hw_beep(int n);                 /* n short beeps, does not block */
void exio_set(uint8_t mask, bool on);  /* 2.1" only: its TCA9554 outputs */
/* the boot logo fading in and into the gauge screen already loaded */
void hw_boot(const lv_img_dsc_t *logo);
/* GPS and accelerometer, where the board has them (the AMOLED 1.75-G);
 * the others say not present */
void hw_motion_fill(rnd_data_t *d);
void hw_g_zero(void);
