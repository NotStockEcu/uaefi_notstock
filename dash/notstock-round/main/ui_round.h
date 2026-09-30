/* Round gauge UI: one value at a time on a round panel, swipe left / right
 * for the next one. Long press anywhere: the menu (gauges, DPF status).
 * A particulate filter regeneration pops up over whatever is shown, with a
 * beep. ESP-free, so tools/sim renders it on a PC.
 *
 * Panel size: 480 for the Waveshare ESP32-S3-Touch-LCD-2.1 (default), 466
 * for a 1.32" AMOLED (-DRND_SIZE=466). Everything is laid out from the
 * centre, so either works.
 */
#pragma once
#include <stdbool.h>
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

/* provided by the platform: n short beeps, must not block */
void rnd_beep(int n);

void ui_round_create(void);
void ui_round_update(const rnd_data_t *d);   /* call at ~30 Hz */
void ui_round_page(int page);                 /* what a swipe does */
int  ui_round_current(void);
