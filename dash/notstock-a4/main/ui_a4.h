/* The A4 gauge: one dial at a time after the Audi A4 B8 cluster at night
 * (red lit rim, white scale, orange-red needle, the value in the lower
 * right where the cluster has its "1/min x1000"),
 * swipe left / right for the next: oil, intake air, coolant, exhaust gas,
 * boost, DPF (no scale: the filter icon, soot and differential pressure
 * in digits; the icon amber while it regenerates, red when full).
 * A particulate filter regeneration lights the amber DPF lamp and says
 * REGENERACE over the value on every dial, with a chime at its start and
 * end. Double tap: night (dimmer) and back.
 * Platform-free (tools/sim); the data is the round gauge's rnd_data_t,
 * filled by its can_obd.c.
 */
#pragma once
#include <stdbool.h>
#include "lvgl.h"
#include "ui_round.h"

#define A4_SIZE 466

enum { A4_OIL, A4_IAT, A4_CLT, A4_EGT, A4_BOOST, A4_DPF, A4_PAGES };

/* by the platform */
void a4_backlight(uint8_t percent);
void a4_regen_sound(bool start);         /* must not block */

void ui_a4_create(int page);             /* builds and loads the screen */
void ui_a4_update(const rnd_data_t *d);  /* ~30 Hz */
void ui_a4_page(int page);
int  ui_a4_current(void);
/* the needle sweep to full scale and back, as the cluster at ignition on */
void ui_a4_sweep(void);
