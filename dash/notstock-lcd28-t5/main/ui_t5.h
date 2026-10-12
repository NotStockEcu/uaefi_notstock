/* The VW T5.1 pages of the 2.8" NOT STOCK gauge (320 x 240, landscape): the
 * honeycomb and the cards of the 2" EMU gauge, NOT STOCK yellow.
 * Page 1: BOOST (an LED bar, 0.1 bar a segment, MAX since power-on) and
 * EGT on the left, OIL and CLT on the right.
 * Page 2: IAT, RPM, SPEED; the particulate filter: SOOT, its differential
 * pressure, the distance since the last regeneration.
 * Swipe left / right. At the top: the link dot (green: the ECU answers,
 * red: not), NOT STOCK and the page dots, the OBD state (SCAN, NO ECU).
 * Platform-free (tools/sim).
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#define T5_W 320
#define T5_H 240

enum { T5_BOOST, T5_EGT, T5_OILT, T5_CLT,
       T5_IAT, T5_RPM, T5_SPEED, T5_SOOT, T5_DP, T5_KM, T5_N };
enum { T5_ST_POLLING, T5_ST_SCANNING, T5_ST_NO_ECU };

/* what the pages show, NAN: no value */
typedef struct {
    float x[T5_N];        /* boost: bar over the barometer */
    bool  link;           /* the ECU answered lately */
    int   state;          /* T5_ST_* */
} t5_view_t;

#define T5_PAGES 2
void ui_t5_create(void);                 /* builds and loads the screen */
void ui_t5_update(const t5_view_t *v);   /* ~30 Hz */
void ui_t5_page(int page);               /* what a swipe does */
