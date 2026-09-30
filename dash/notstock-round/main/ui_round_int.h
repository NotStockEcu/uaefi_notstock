/* Shared between the round gauge UI files, not for the platform. */
#pragma once
#include "ui_round.h"
#include "faces.h"

LV_FONT_DECLARE(rnd_112);
LV_FONT_DECLARE(rnd_84);
LV_FONT_DECLARE(rnd_26);
LV_FONT_DECLARE(rnd_18);

#define C_W      lv_color_hex(0xFFFFFF)
#define C_GREY   lv_color_hex(0x8A9096)
#define C_DIM    lv_color_hex(0x5A5F66)
#define C_DOT    lv_color_hex(0x3A3F45)
#define C_RED    lv_color_hex(0xFF3030)
#define C_REGEN  lv_color_hex(0xFF9A1F)
#define C_PANEL  lv_color_hex(0x16191C)
#define C_EDGE   lv_color_hex(0x2A2D31)

#define CX       (RND_W / 2)
#define ARC_MAX  1000        /* arc range: fraction of the scale * 1000 */

/* the value arc with its glow, on the faces' groove geometry */
#define N_ARC 3
void rnd_arcs(lv_obj_t *par, lv_obj_t *out[N_ARC]);
void rnd_arcs_set(lv_obj_t *a[N_ARC], float frac_1000, lv_color_t c);

lv_obj_t *rnd_label(lv_obj_t *par, const lv_font_t *f, lv_color_t c,
                    lv_coord_t y);

/* screens */
lv_obj_t *rnd_gauge_screen(void);
void rnd_menu_create(void);
void rnd_menu_open(void);
void rnd_dpf_create(void);
lv_obj_t *rnd_dpf_screen(void);
void rnd_dpf_update(const rnd_data_t *d);

/* regeneration: popup, beep, and the flag the gauges show */
#define REGEN_TEMP 400.0f    /* filter hotter than this: regenerating */
void rnd_regen_watch(const rnd_data_t *d);
bool rnd_regen_active(void);
