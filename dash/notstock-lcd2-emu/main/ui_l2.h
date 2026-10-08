/* The EMU pages of the 2" NOT STOCK gauge (240 x 320, portrait): the
 * honeycomb of the round EMU gauge, NOT STOCK yellow.
 * Page 1: BOOST (an LED bar, 0.1 bar a segment), LAMBDA (a needle on a
 * 0.70 .. 1.30 scale, AFR under it), IAT and CLT.
 * Page 2: Throttle, RPM, oil temperature, oil pressure, battery, EGT.
 * Swipe left / right. At the top: the link dot (green: stream frames
 * coming, red: none), NOT STOCK and the page dots, FAN lit blue while the
 * EMU runs the coolant fan.
 * Platform-free (tools/sim).
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "emu_stream.h"
#include "lvgl.h"

#define L2_W 240
#define L2_H 320

#define L2_STOICH   14.7f      /* AFR = lambda x this (petrol) */
#define L2_STALE_US 1000000    /* a frame older than this: no value */

enum { L2_BOOST, L2_LAMBDA, L2_IAT, L2_CLT,
       L2_TPS, L2_RPM, L2_OILT, L2_OILP, L2_BATT, L2_EGT, L2_N };

/* what the pages show, NAN: no value */
typedef struct {
    float x[L2_N];        /* boost: bar over the barometer */
    bool  err[L2_N];      /* the ECU says: sensor failed */
    bool  link;           /* any stream frame within L2_STALE_US */
    int   fan;            /* coolant fan: 1 running, 0 off, -1 not known */
} l2_view_t;

void l2_view_from(const emu_values_t *v, int64_t now_us, l2_view_t *out);

#define L2_PAGES 2
void ui_l2_create(void);                 /* builds and loads the screen */
void ui_l2_update(const l2_view_t *v);   /* ~30 Hz */
void ui_l2_page(int page);               /* what a swipe does */
