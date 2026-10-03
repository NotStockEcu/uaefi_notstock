/* FUTURO look: one shared background (tools/gen_faces.py: black, a hex grid
 * fading out to the rim, thin cyan rings) and a ring of segments that light
 * up to the value, cyan into magenta, dim red past the warn limit and all
 * red over it. Icon and title on top, a pale neon readout in the middle. */
#include "ui_round_int.h"

#include <math.h>
#include <stdio.h>

#define SEGS        46
#define SEG_W       9
#define C_CYAN      lv_color_hex(0x00E5FF)
#define C_MAGENTA   lv_color_hex(0xFF2BD6)
#define C_ICE       lv_color_hex(0xC8FBFF)
#define C_SEG_OFF   lv_color_hex(0x0C2830)
#define C_SEG_ZONE  lv_color_hex(0x4A1024)
#define C_SEG_WARN  lv_color_hex(0xFF3040)
#define C_TEAL_DIM  lv_color_hex(0x2A8C9C)

static lv_obj_t *meter, *icon, *title, *val_lbl, *unit_lbl, *peak_lbl;
static lv_obj_t *lo_lbl, *hi_lbl, *regen_lbl, *dot[RND_PAGES];
static lv_meter_scale_t *scale;
static lv_meter_indicator_t *lit, *zone;
static float zone_at;
static int lit_at, warn_on, regen_on, big_on;


static void build(lv_obj_t *scr)
{
    lv_obj_t *bg = lv_img_create(scr);
    lv_img_set_src(bg, &face_futuro_bg);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_CLICKABLE);

    meter = lv_meter_create(scr);
    lv_obj_remove_style_all(meter);
    lv_obj_set_size(meter, 2 * FUTURO_SEG_R, 2 * FUTURO_SEG_R);
    lv_obj_center(meter);
    lv_obj_clear_flag(meter, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_width(meter, 0, LV_PART_INDICATOR);     /* no pivot */
    lv_obj_set_style_height(meter, 0, LV_PART_INDICATOR);
    lv_obj_set_style_text_opa(meter, LV_OPA_TRANSP, LV_PART_TICKS);
    scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, SEGS, SEG_W, FUTURO_SEG_L,
                             C_SEG_OFF);
    lv_meter_set_scale_major_ticks(meter, scale, 1000, SEG_W, FUTURO_SEG_L,
                                   C_SEG_OFF, 0);
    lv_meter_set_scale_range(meter, scale, 0, ARC_MAX, FACE_SWEEP,
                             FACE_START);
    /* the last indicator wins a segment: zone first, the value over it */
    zone = lv_meter_add_scale_lines(meter, scale, C_SEG_ZONE, C_SEG_ZONE,
                                    false, 0);
    lit = lv_meter_add_scale_lines(meter, scale, C_CYAN, C_MAGENTA, false, 0);
    lv_meter_set_indicator_start_value(meter, lit, 0);
    lv_meter_set_indicator_end_value(meter, lit, -1);

    icon = lv_img_create(scr);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, CX - 132);
    lv_obj_set_style_img_recolor(icon, C_CYAN, 0);
    lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);
    title = rnd_label(scr, &rnd_18, C_TEAL_DIM, CX - 72);
    lv_obj_set_style_text_letter_space(title, 5, 0);

    val_lbl = rnd_label(scr, &rnd_112, C_ICE, 0);
    unit_lbl = rnd_label(scr, &rnd_26, C_MAGENTA, CX + 64);
    peak_lbl = rnd_label(scr, &rnd_18, C_TEAL_DIM, CX + 102);
    lv_obj_set_style_text_letter_space(peak_lbl, 2, 0);

    /* the scale's ends, just inside the segments */
    lo_lbl = rnd_label(scr, &rnd_18, C_TEAL_DIM, CX + 128);
    hi_lbl = rnd_label(scr, &rnd_18, C_TEAL_DIM, CX + 128);
    lv_obj_set_width(lo_lbl, 90);
    lv_obj_set_width(hi_lbl, 90);
    lv_obj_set_x(lo_lbl, CX - 150);
    lv_obj_set_x(hi_lbl, CX + 60);

    regen_lbl = rnd_label(scr, &rnd_18, C_REGEN, CX + 150);
    lv_obj_set_style_text_letter_space(regen_lbl, 2, 0);
    rnd_dots(scr, CX + 185, dot);
}

static void page(int pg)
{
    char buf[16];
    const face_page_t *p = &FACE_PAGE[pg];
    lv_img_set_src(icon, page_icon[pg]);
    lv_label_set_text(title, rnd_page_name(pg));
    lv_label_set_text(unit_lbl, rnd_unit(pg));
    snprintf(buf, sizeof buf, "%g", p->lo);
    lv_label_set_text(lo_lbl, buf);
    snprintf(buf, sizeof buf, "%g", p->hi);
    lv_label_set_text(hi_lbl, buf);
    zone_at = NAN;
    lit_at = warn_on = regen_on = big_on = -1;
    rnd_dots_set(dot, pg, C_CYAN, lv_color_hex(0x123A44));
    lv_obj_fade_in(val_lbl, 250, 0);
}

static void draw(const rnd_view_t *v)
{
    if (v->warn_frac != zone_at) {
        zone_at = v->warn_frac;
        bool on = v->warn_frac < 1;
        lv_meter_set_indicator_start_value(meter, zone, on ?
            (int32_t)lroundf(v->warn_frac * ARC_MAX) : ARC_MAX + 1);
        lv_meter_set_indicator_end_value(meter, zone, ARC_MAX);
    }
    /* whole segments only: a new value redraws the ring when one flips */
    int n = (int)lroundf(v->frac * (SEGS - 1));
    if (v->frac < 0.01f) n = -1;
    if (n != lit_at) {
        lit_at = n;
        lv_meter_set_indicator_end_value(meter, lit, n < 0 ? -1 :
            (int32_t)lroundf((float)n * ARC_MAX / (SEGS - 1)));
    }
    if (v->warn != warn_on) {
        warn_on = v->warn;
        lit->type_data.scale_lines.color_start = v->warn ? C_SEG_WARN : C_CYAN;
        lit->type_data.scale_lines.color_end = v->warn ? C_SEG_WARN : C_MAGENTA;
        lv_obj_invalidate(meter);
        lv_obj_set_style_text_color(val_lbl, v->warn ? C_SEG_WARN : C_ICE, 0);
    }
    if (v->big != big_on) {
        big_on = v->big;
        lv_obj_set_style_text_font(val_lbl, v->big ? &rnd_112 : &rnd_84, 0);
        lv_obj_set_y(val_lbl, CX - lv_font_get_line_height(
            v->big ? &rnd_112 : &rnd_84) / 2 + 12);
    }
    lv_label_set_text(val_lbl, v->text);
    lv_label_set_text(peak_lbl, v->peak);
    lv_obj_set_style_text_color(peak_lbl, v->alert ? C_SEG_WARN : C_TEAL_DIM,
                                0);
    if (v->regen != regen_on) {
        regen_on = v->regen;
        lv_label_set_text(regen_lbl, v->regen ? TR("DPF REGEN", "REGEN. DPF")
                                                 : "");
    }
}

const rnd_look_t rnd_look_futuro = { build, page, draw };
