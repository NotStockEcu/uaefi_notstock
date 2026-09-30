/* RETRO look: a mechanical instrument of the VDO kind. Pre-rendered dial per
 * page (tools/gen_faces.py: chrome bezel, black dial, white print, glass
 * sheen, the readout window's frame); live: the red band from the warn
 * limit, a red needle with its shadow and a black hub, a thin tell-tale
 * needle left at the peak, and the value in the window. */
#include "ui_round_int.h"

#include <math.h>

#define NEEDLE_TIP   (RETRO_TICK_R - 8)
#define NEEDLE_TAIL  (-RND_W * 7 / 100)
#define TELL_IN      (RND_W * 29 / 100)
#define C_NEEDLE     lv_color_hex(0xE8261E)
#define C_TELL       lv_color_hex(0xC86A1E)
#define C_BAND       lv_color_hex(0xC81E1E)

static lv_obj_t *face, *zone, *needle, *shadow, *tell, *hub, *cap;
static lv_obj_t *win_lbl, *alert_lbl, *regen_lbl, *dot[RND_COUNT];
static lv_point_t pn[2], ps[2], pt[2];
static float zone_at, needle_at, tell_at;
static int warn_on, regen_on;

static lv_obj_t *line(lv_obj_t *par, int w, lv_color_t c, lv_opa_t opa)
{
    lv_obj_t *l = lv_line_create(par);
    lv_obj_set_pos(l, 0, 0);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_set_style_line_color(l, c, 0);
    lv_obj_set_style_line_opa(l, opa, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

static lv_obj_t *disc(lv_obj_t *par, int d, lv_color_t c, lv_color_t edge,
                      int edge_w)
{
    lv_obj_t *o = lv_obj_create(par);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, d, d);
    lv_obj_center(o);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, edge, 0);
    lv_obj_set_style_border_width(o, edge_w, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void build(lv_obj_t *scr)
{
    face = lv_img_create(scr);
    lv_obj_set_pos(face, 0, 0);
    lv_obj_clear_flag(face, LV_OBJ_FLAG_CLICKABLE);
    zone = rnd_zone_at(scr, RETRO_ZONE_R, RETRO_ZONE_W, C_BAND);
    lv_obj_set_style_arc_rounded(zone, false, LV_PART_MAIN);

    /* the readout in the window */
    win_lbl = rnd_label(scr, &rnd_barlow_46, lv_color_white(), 0);
    lv_obj_set_y(win_lbl, CX + (RETRO_WIN_TOP + RETRO_WIN_BOT) / 2 -
                 lv_font_get_line_height(&rnd_barlow_46) / 2 + 2);
    alert_lbl = rnd_label(scr, &rnd_18, C_RED, CX + 54);
    regen_lbl = rnd_label(scr, &rnd_18, C_REGEN, CX + 30);
    lv_obj_set_style_text_letter_space(regen_lbl, 2, 0);
    rnd_dots(scr, CX + 192, dot);

    /* tell-tale under the needle, the needle's shadow, the needle, hub */
    tell = line(scr, 3, C_TELL, LV_OPA_COVER);
    shadow = line(scr, 9, lv_color_black(), LV_OPA_50);
    needle = line(scr, 7, C_NEEDLE, LV_OPA_COVER);
    hub = disc(scr, 46, lv_color_hex(0x111111), lv_color_hex(0x55575A), 3);
    cap = disc(scr, 18, lv_color_hex(0x2A2B2D), lv_color_hex(0x77797C), 1);
}

static void page(int pg)
{
    lv_img_set_src(face, face_retro_img[pg]);
    zone_at = needle_at = tell_at = NAN;
    warn_on = regen_on = -1;
    rnd_dots_set(dot, pg, lv_color_white(), lv_color_hex(0x55575A));
}

static void set_line(lv_obj_t *l, lv_point_t *p, float frac, int r0, int r1,
                     int dx, int dy)
{
    float a = (FACE_START + frac * FACE_SWEEP) * (float)M_PI / 180.0f;
    float c = cosf(a), s = sinf(a);
    p[0].x = (lv_coord_t)lroundf(CX + r0 * c) + dx;
    p[0].y = (lv_coord_t)lroundf(CX + r0 * s) + dy;
    p[1].x = (lv_coord_t)lroundf(CX + r1 * c) + dx;
    p[1].y = (lv_coord_t)lroundf(CX + r1 * s) + dy;
    lv_line_set_points(l, p, 2);
}

static void draw(const rnd_view_t *v)
{
    if (v->warn_frac != zone_at) {
        zone_at = v->warn_frac;
        rnd_zone_set(zone, v->warn_frac);
    }
    if (fabsf(v->frac - needle_at) > 0.0005f || isnan(needle_at)) {
        needle_at = v->frac;
        set_line(needle, pn, v->frac, NEEDLE_TAIL, NEEDLE_TIP, 0, 0);
        set_line(shadow, ps, v->frac, NEEDLE_TAIL, NEEDLE_TIP, 3, 4);
    }
    /* the tell-tale stays where the value peaked; none for rpm */
    if (v->peak_frac != tell_at) {
        tell_at = v->peak_frac;
        if (isnan(v->peak_frac)) {
            lv_obj_add_flag(tell, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(tell, LV_OBJ_FLAG_HIDDEN);
            set_line(tell, pt, v->peak_frac, TELL_IN, NEEDLE_TIP - 10, 0, 0);
        }
    }
    lv_label_set_text(win_lbl, v->text);
    lv_label_set_text(alert_lbl, v->alert ? v->peak : "");
    if (v->warn != warn_on) {
        warn_on = v->warn;
        lv_obj_set_style_text_color(win_lbl, v->warn ? C_RED
                                                     : lv_color_white(), 0);
    }
    if (v->regen != regen_on) {
        regen_on = v->regen;
        lv_label_set_text(regen_lbl, v->regen ? "DPF REGEN" : "");
    }
}

const rnd_look_t rnd_look_retro = { build, page, draw };
