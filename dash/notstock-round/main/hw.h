/* Board drivers, see hw.c */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "ui_round.h"
#include "esp_err.h"
#include <stddef.h>

void hw_init(void);                  /* I2C, expander, panel, touch, LVGL */
void hw_backlight(uint8_t percent);  /* 0..100 */
void hw_beep(int n);                 /* n short beeps, does not block */
/* PCM, 16 kHz mono 16 bit, on the speaker; does not block (the samples
 * must stay); false: no speaker, or it is busy */
bool hw_play(const int16_t *pcm, size_t n);
void hw_volume(uint8_t percent);     /* the speaker's, 10..100; no buzzer */
void exio_set(uint8_t mask, bool on);  /* 2.1" only: its TCA9554 outputs */
/* a whole new screen at once, not drawn down the panel strip by strip:
 * begin before it changes, end after lv_timer_handler (AMOLED boards) */
void hw_flip_begin(void);
void hw_flip_end(void);
/* the boot logo fading in and into the gauge screen already loaded */
void hw_boot(const lv_img_dsc_t *logo);
/* the G-meter, motion.c: started by each board's hw_init, on its I2C */
void motion_start(void);
void hw_motion_fill(rnd_data_t *d);
void hw_g_zero(void);
esp_err_t hw_i2c_write(uint8_t addr, const uint8_t *d, size_t n);
esp_err_t hw_i2c_write_read(uint8_t addr, const uint8_t *w, size_t wn,
                            uint8_t *r, size_t rn);
