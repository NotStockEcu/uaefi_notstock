/* Dash looks ("Look" in the settings menu).
 *
 * NOTSTOCK lives in ui.c; EMO, LONK and HILL each in their own ui_theme_*.c.
 * Only the selected look is built: switching deletes the old screen and
 * builds the new one, so the LVGL heap only ever holds one of them. Their
 * static artwork comes from tools/gen_themes.py (theme_art.h).
 *
 * The long-press corners (menu, night mode, LOG), the shift flash, the
 * brightness / night wash and the LOG recording are shared by every look.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"
#include "rusefi_can.h"

enum { LOOK_NOTSTOCK, LOOK_EMO, LOOK_LONK, LOOK_HILL, LOOK_COUNT };

/* link state handed to update(): what the "NO CAN / DEMO" text should say */
enum { LINK_NONE, LINK_OK, LINK_DEMO };

typedef struct {
    lv_obj_t *(*build)(void);   /* new screen, fully built, not loaded */
    void (*update)(const dash_data_t *d, int link);
    /* where the shift flash disc goes in rev counter mode; r 0 = the look
     * has no round rev counter and always flashes the whole screen */
    lv_coord_t flash_cx, flash_cy, flash_r;
} theme_t;

extern const theme_t theme_emo, theme_lonk, theme_hill;

/* ---- shared helpers, implemented in ui.c ---- */
lv_obj_t *ui_screen(lv_color_t bg);
lv_obj_t *ui_label(lv_obj_t *par, const lv_font_t *font, lv_color_t col,
                   const char *txt, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                   lv_text_align_t align);
lv_obj_t *ui_box(lv_obj_t *par, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                 lv_coord_t h);
lv_obj_t *ui_rect(lv_obj_t *par, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                  lv_coord_t h, lv_color_t col);
lv_obj_t *ui_img_at(lv_obj_t *par, const lv_img_dsc_t *src, lv_coord_t x,
                    lv_coord_t y);
void ui_text(lv_obj_t *label, const char *txt);   /* only if it changed */
void ui_unit_on_baseline(lv_obj_t *unit);
void ui_corners(lv_obj_t *scr);                   /* menu, night, LOG */
float ui_clampf(float v, float lo, float hi);

/* warn limits from the settings, NAN when switched off */
enum { LIM_CLT, LIM_IAT, LIM_BOOST, LIM_AFR, LIM_RPM };
float ui_limit(int which);
bool ui_over(int which, float v);
