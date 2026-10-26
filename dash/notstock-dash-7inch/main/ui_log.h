/* LOG screen: the last 30 s of selected channels as a live chart.
 *
 * Recording runs all the time, whichever screen is up, so after something
 * odd happens on the road the log already holds it. Long-press the top
 * right corner of the dash to open it; DASH (same corner) goes back.
 */
#pragma once
#include <stdint.h>
#include "lvgl.h"
#include "rusefi_can.h"

void ui_log_create(void);
lv_obj_t *ui_log_screen(void);

/* Called with every UI tick; keeps its own 10 Hz sample rate. */
void ui_log_sample(const dash_data_t *d, int64_t now_us);
