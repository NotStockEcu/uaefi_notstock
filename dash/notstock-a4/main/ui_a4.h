/* The A4 gauge: one dial at a time after the Audi A4 B8 cluster at night
 * (red lit rim, white scale, red needle, the value in the lower
 * right where the cluster has its "1/min x1000"), swipe left / right for
 * the next: oil, intake air, coolant, exhaust gas, boost, DPF (no scale:
 * the filter icon, soot and differential pressure in digits; the icon
 * amber while it regenerates, red when full).
 * Past each dial's limit (a setting) its ticks and numerals are red, with
 * a red band inside the ticks: finely broken at the limit, the pieces
 * growing to a solid band at the end of the scale.
 * A particulate filter regeneration beeps, brings up the DPF page (the
 * dial it covered comes back after) and lights the amber DPF lamp with
 * REGENERACE over the value on the other dials; it beeps once at its end.
 * The DPF page also has the filter's (modelled) surface temperature.
 * Double tap: night (dimmer) and back. Long press: the menu, LIMITY
 * (each dial's limit) and PORADI (the dials' order in the swipe, shown or
 * hidden), ui_a4_menu.c.
 * Platform-free (tools/sim); the data is the round gauge's rnd_data_t,
 * filled by its can_obd.c.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "ui_round.h"

#define A4_SIZE 466

enum { A4_OIL, A4_IAT, A4_CLT, A4_EGT, A4_BOOST, A4_DPF, A4_PAGES };

/* settings, edited in place by the menu; the platform loads them before
 * ui_a4_create() (or keeps a4_settings_defaults()) and stores them when
 * a4_settings_save() is called, on leaving a settings screen */
typedef struct {
    uint8_t order[A4_PAGES];   /* the dials in swipe order */
    uint8_t hidden;            /* bit per dial (A4_*): left out */
    float   warn[A4_PAGES];    /* red from here: the dials' units, DPF soot g */
} a4_settings_t;
extern a4_settings_t g_a4_set;
void a4_settings_defaults(void);

/* limits: what the menu allows */
typedef struct {
    const char *name, *unit;
    float lo, hi, step, def;
    int   dec;
} a4_limit_t;
extern const a4_limit_t A4_LIMIT[A4_PAGES];

/* by the platform */
void a4_backlight(uint8_t percent);
void a4_regen_sound(bool start);         /* must not block */
void a4_settings_save(void);
/* the screen is about to change all over: draw it whole, then show it */
void a4_flip(void);

void ui_a4_create(void);                 /* builds the screens, loads the dial */
void ui_a4_update(const rnd_data_t *d);  /* ~30 Hz */
void ui_a4_page(int page);               /* show this dial (A4_*) */
int  ui_a4_current(void);
/* the needle sweep to full scale and back, as the cluster at ignition on */
void ui_a4_sweep(void);

/* between ui_a4.c and ui_a4_menu.c */
lv_obj_t *a4_gauge_screen(void);
void a4_settings_changed(void);          /* limits or order: redraw the dial */
void a4_menu_create(void);
void a4_menu_open(void);
