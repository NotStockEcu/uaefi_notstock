/* NOTSTOCK look: a pre-rendered face per page (tools/gen_faces.py:
 * background, groove, scale, icon, title), the red zone over the groove,
 * a glowing value arc, the readout, the peak and the page dots. */
#include "ui_round_int.h"

#include <math.h>

static lv_obj_t *face, *center, *zone, *arc[N_ARC];
static lv_obj_t *val_lbl, *unit_lbl, *peak_lbl, *regen_lbl, *dot[RND_COUNT];
static float zone_at;
static int warn_on, regen_on, big_on;

static void build(lv_obj_t *scr)
{
    face = lv_img_create(scr);
    lv_obj_set_pos(face, 0, 0);
    lv_obj_clear_flag(face, LV_OBJ_FLAG_CLICKABLE);
    zone = rnd_zone(scr);
    rnd_arcs(scr, arc);

    /* the readout, faded in on a page change */
    center = lv_obj_create(scr);
    lv_obj_remove_style_all(center);
    lv_obj_set_size(center, RND_W, RND_H);
    lv_obj_clear_flag(center, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    val_lbl = rnd_label(center, &rnd_112, C_W, 0);
    unit_lbl = rnd_label(center, &rnd_26, C_GREY, CX + 68);
    peak_lbl = rnd_label(center, &rnd_18, C_DIM, CX + 106);
    lv_obj_set_style_text_letter_space(peak_lbl, 2, 0);

    rnd_dots(scr, CX + 185, dot);
    regen_lbl = rnd_label(scr, &rnd_18, C_REGEN, CX + 198);
    lv_obj_set_style_text_letter_space(regen_lbl, 2, 0);
}

static void page(int pg)
{
    lv_img_set_src(face, face_img[pg]);
    lv_label_set_text(unit_lbl, FACE_PAGE[pg].unit);
    lv_label_set_text(peak_lbl, "");
    zone_at = NAN;
    warn_on = regen_on = big_on = -1;
    rnd_arcs_set(arc, 0, C_W);
    rnd_dots_set(dot, pg, C_W, C_DOT);
    lv_obj_fade_in(center, 250, 0);
}

static void draw(const rnd_view_t *v)
{
    if (v->warn_frac != zone_at) {
        zone_at = v->warn_frac;
        rnd_zone_set(zone, v->warn_frac);
    }
    if (v->big != big_on) {
        big_on = v->big;
        lv_obj_set_style_text_font(val_lbl, v->big ? &rnd_112 : &rnd_84, 0);
        /* value vertically centred on the dial, whatever the font */
        lv_obj_set_y(val_lbl, CX - lv_font_get_line_height(
            v->big ? &rnd_112 : &rnd_84) / 2 + 14);
    }
    lv_label_set_text(val_lbl, v->text);
    lv_label_set_text(peak_lbl, v->peak);
    lv_obj_set_style_text_color(peak_lbl, v->alert ? C_RED : C_DIM, 0);
    rnd_arcs_set(arc, v->frac * ARC_MAX,
                 v->warn ? C_RED : C_W);
    if (v->warn != warn_on) {
        warn_on = v->warn;
        lv_obj_set_style_text_color(val_lbl, v->warn ? C_RED : C_W, 0);
    }
    if (v->regen != regen_on) {
        regen_on = v->regen;
        lv_label_set_text(regen_lbl, v->regen ? "DPF REGEN" : "");
    }
}

const rnd_look_t rnd_look_notstock = { build, page, draw };
