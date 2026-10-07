/* The EMU pages of the round gauge, in the manner of ECUMaster's round
 * gauges: a honeycomb background, a vertical bar with its scale on the
 * left, channels stacked on the right with their maximum under them.
 * Page 1: BOOST (bar), AFR, TPS. Page 2: CLT (bar), IAT. Swipe left /
 * right. The dot at the top is the link: green data, red none; FAN next
 * to it lights blue while the coolant fan runs.
 * ESP-free (tools/sim).
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "emu_stream.h"
#include "lvgl.h"

#define EMU_W 480
#define EMU_H 480

#define EMU_STOICH   14.7f      /* AFR = lambda x this (petrol) */
#define EMU_STALE_US 1000000    /* a frame older than this: no value */

/* what the page shows, NAN: no value */
typedef struct {
    float boost;      /* bar over the barometer */
    float afr;
    float clt;        /* degC */
    float iat;        /* degC */
    float tps;        /* % */
    bool  link;       /* any stream frame within EMU_STALE_US */
    bool  err_map, err_wbo, err_clt, err_iat;  /* the ECU says: failed */
    int   fan;        /* coolant fan: 1 running, 0 off, -1 not known */
} emu_view_t;

void emu_view_from(const emu_values_t *v, int64_t now_us, emu_view_t *out);

#define EMU_PAGES 2
void ui_emu_create(void);               /* builds and loads the screen */
void ui_emu_update(const emu_view_t *v); /* ~30 Hz */
void ui_emu_page(int page);             /* what a swipe does */
