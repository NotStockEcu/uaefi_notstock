/* Round gauge UI: one value at a time on a round panel, swipe left / right
 * for the next one. Long press anywhere: the menu (gauges, DPF status,
 * settings: look, beep on/off, warn limits).
 * A particulate filter regeneration pops up over whatever is shown, with a
 * beep. ESP-free, so tools/sim renders it on a PC.
 *
 * Panel size: 480 for the Waveshare ESP32-S3-Touch-LCD-2.1 (default), 466
 * for a 1.32" AMOLED (-DRND_SIZE=466). Everything is laid out from the
 * centre, so either works.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifndef RND_SIZE
#define RND_SIZE 480
#endif
#define RND_W RND_SIZE
#define RND_H RND_SIZE

/* page order; must match PAGES in tools/gen_faces.py */
enum { RND_WATER, RND_OIL, RND_BOOST, RND_INTAKE, RND_EXHAUST, RND_RPM,
       RND_COUNT };

/* live values, NAN where unknown; link false: nothing from the car */
typedef struct {
    float v[RND_COUNT];
    struct {                  /* particulate filter (VW UDS, see the 7" dash) */
        float soot_g;         /* calculated */
        float soot_meas_g;    /* measured */
        float dp_hpa;         /* differential pressure */
        float dist_km;        /* since the last regeneration */
        float temp_c;         /* simulated filter surface temperature */
    } dpf;
    bool  link;
} rnd_data_t;

/* Settings. The UI edits g_rnd_set in place; the platform loads it before
 * ui_round_create() (or keeps rnd_settings_defaults()) and stores it when
 * rnd_settings_save() is called, on leaving the settings screens. */
enum { RND_WARN_SOOT = RND_COUNT, RND_WARN_COUNT };  /* after the pages */
enum { RND_LOOK_NOTSTOCK, RND_LOOK_RETRO, RND_LOOK_FUTURO, RND_LOOK_COUNT };

typedef struct {
    uint8_t look;                     /* RND_LOOK_* */
    bool    beep;                     /* beep on regeneration start / end */
    float   warn[RND_WARN_COUNT];     /* red above this: pages, DPF soot g */
} rnd_settings_t;

extern rnd_settings_t g_rnd_set;
void rnd_settings_defaults(void);

/* provided by the platform */
void rnd_beep(int n);                 /* n short beeps, must not block */
void rnd_settings_save(void);         /* store g_rnd_set */

/* builds every screen; boot: the NOT STOCK logo first, fading in from black
 * and then into the gauges (RND_BOOT_MS in all), else the gauges at once */
void ui_round_create(bool boot);
#define RND_BOOT_IN_MS    1200    /* logo out of black */
#define RND_BOOT_HOLD_MS  1500
#define RND_BOOT_X_MS     900     /* logo into the gauges */
#define RND_BOOT_MS (RND_BOOT_IN_MS + RND_BOOT_HOLD_MS + RND_BOOT_X_MS)
void ui_round_update(const rnd_data_t *d);   /* call at ~30 Hz */
void ui_round_page(int page);                 /* what a swipe does */
int  ui_round_current(void);
