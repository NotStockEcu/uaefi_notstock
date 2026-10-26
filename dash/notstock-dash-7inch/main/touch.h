#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

/* Resets and probes the GT911. Returns false if it does not answer, in which
 * case the dash still runs, just without the settings menu. */
bool touch_init(void);

/* Registers the panel as an LVGL pointer device. */
void touch_register_lvgl(void);

/* Raw read, mostly useful for debugging. */
bool touch_read(uint16_t *x, uint16_t *y);
