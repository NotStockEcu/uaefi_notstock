/* The EMU page of the round gauge: BOOST big at the top, AFR and CLT
 * below, each in a panel with a bar, the way ECUMaster's own dashes draw
 * channels. Panels flash red over the alarm limit. ESP-free (tools/sim).
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
    bool  link;       /* any stream frame within EMU_STALE_US */
    bool  err_map, err_wbo, err_clt;   /* the ECU flags the sensor failed */
} emu_view_t;

void emu_view_from(const emu_values_t *v, int64_t now_us, emu_view_t *out);

void ui_emu_create(void);               /* builds and loads the screen */
void ui_emu_update(const emu_view_t *v); /* ~30 Hz */
