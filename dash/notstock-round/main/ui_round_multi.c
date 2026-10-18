/* Round gauge UI: MULTI, several values on one page. See ui_round.h.
 *
 * One big value in the middle with its scale over the top, three small
 * gauges in a row below it, like a CAN Checked MFD's multi view, drawn in
 * the chosen look (see below). What goes where
 * is g_rnd_set.multi[], set on the editor screen (SETTINGS -> PAGES ->
 * MULTI -> EDIT): tap a slot for the next value.
 *
 * MULTI is a page like the gauges (it has its place in the swipe order and
 * can be hidden). On the gauge screen it is a black layer over the look,
 * shown only on its page; gestures, long press and double tap go through to
 * the screen as on any other page.
 */
#include "ui_round_int.h"
#include "red.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MINI_Y      (CX + MULTI_MINI_Y)  /* small gauges: centre line */
#define MINI_DX     MULTI_MINI_DX
#define SOOT_MAX    40.0f

#define C_TRACK     lv_color_hex(0x1A1D21)
#define C_ZONE      lv_color_hex(0x5A1414)

/* the order a tap cycles through */
static const uint8_t CHOICES[] = {
    RND_WATER, RND_OIL, RND_BOOST, RND_INTAKE, RND_EXHAUST, RND_RPM,
    RND_WARN_SOOT, RND_MV_NONE,
};

/* -------------------------------------------------------- the values */
static float value_of(const rnd_data_t *d, int id)
{
    if (!d->link) return NAN;
    if (id == RND_WARN_SOOT) return d->dpf.soot_g;
    return id >= 0 && id < RND_COUNT ? d->v[id] : NAN;
}

static void range_of(int id, float *lo, float *hi)
{
    if (id == RND_WARN_SOOT) {
        *lo = 0;
        *hi = SOOT_MAX;
    } else {
        *lo = FACE_PAGE[id].lo;
        *hi = FACE_PAGE[id].hi;
    }
}

static void fmt(char *buf, size_t n, int id, float v)
{
    if (isnan(v))               snprintf(buf, n, "--");
    else if (id == RND_BOOST)   snprintf(buf, n, "%.2f", v);
    else if (id == RND_WARN_SOOT) snprintf(buf, n, "%.1f", v);
    else                        snprintf(buf, n, "%d", (int)lroundf(v));
}

static float frac_of(int id, float v)
{
    float lo, hi;
    range_of(id, &lo, &hi);
    float f = (v - lo) / (hi - lo);
    return isnan(f) ? 0 : f < 0 ? 0 : f > 1 ? 1 : f;
}

/* ----------------------------------------------------------- the page */
/* How a slot is drawn depends on the look:
 *   NOTSTOCK  a white arc (the big one over the top, small ones 270 degrees)
 *   RETRO     needles on a pre-rendered dial (face_retro_multi): the big
 *             one over the top half, small ones on their own dials
 *   RED       the same in the A4 gauge's dress (red_multi): red lit rim
 *             and rings, white dashes, red needles and zones */
enum { ST_NOTSTOCK, ST_RETRO, ST_RED };

typedef struct {
    lv_obj_t *box, *val, *name, *unit;
    /* NOTSTOCK: arc and red zone; RETRO: the red band */
    lv_obj_t *arc, *zone;
    /* RETRO */
    lv_obj_t *needle;
    lv_point_t np[2];
    int cx, cy, r0, r1;
    /* all */
    int start, sweep;            /* degrees the scale covers */
    float shown;
    int id, warn, big;
} slot_t;

static lv_obj_t *layer, *regen_lbl, *dot[RND_PAGES];
static slot_t slot[RND_MULTI_SLOTS];
static int regen_on, style;

/* band: the arc's own band (track or red zone); value: the indicator */
enum { ARC_BAND, ARC_VALUE };
static lv_obj_t *arc_at(lv_obj_t *par, int d, int w, int start, int sweep,
                        int kind, lv_color_t c)
{
    lv_obj_t *a = lv_arc_create(par);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, d, d);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(a, start);
    lv_arc_set_bg_angles(a, 0, sweep);
    lv_arc_set_range(a, 0, 1000);
    lv_arc_set_value(a, 0);
    lv_obj_set_style_arc_width(a, w, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, w, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
    if (kind == ARC_BAND) {
        lv_obj_set_style_arc_color(a, c, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    } else {
        lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_color(a, c, LV_PART_INDICATOR);
    }
    return a;
}

static lv_obj_t *label_in(lv_obj_t *par, const lv_font_t *f, lv_color_t c)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, "");
    return l;
}

/* a label centred on x, its middle at y */
static lv_obj_t *label_at(lv_obj_t *par, const lv_font_t *f, lv_color_t c,
                          int x, int y, int w)
{
    lv_obj_t *l = label_in(par, f, c);
    lv_obj_set_width(l, w);
    lv_obj_set_pos(l, x - w / 2, y - lv_font_get_line_height(f) / 2);
    return l;
}

static lv_obj_t *box_in(lv_obj_t *par)
{
    lv_obj_t *b = lv_obj_create(par);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, RND_W, RND_H);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return b;
}

static lv_obj_t *line_in(lv_obj_t *par, int w, lv_color_t c, lv_opa_t opa)
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

static lv_obj_t *disc(lv_obj_t *par, int x, int y, int d, lv_color_t c,
                      lv_color_t edge)
{
    lv_obj_t *o = lv_obj_create(par);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, d, d);
    lv_obj_set_pos(o, x - d / 2, y - d / 2);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, edge, 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/* -------------------------------------------------------- NOTSTOCK */
#define NS_R      214
#define NS_W      22
#define NS_START  162
#define NS_SWEEP  216
#define MINI_D    (2 * MULTI_MINI_R)
#define MINI_W    9

static void build_notstock(void)
{
    slot_t *s = &slot[0];
    s->box = box_in(layer);
    s->start = NS_START;
    s->sweep = NS_SWEEP;
    int d = 2 * NS_R + NS_W;
    lv_obj_center(arc_at(s->box, d, NS_W, NS_START, NS_SWEEP, ARC_BAND,
                         C_TRACK));
    s->zone = arc_at(s->box, d, NS_W, NS_START, NS_SWEEP, ARC_BAND, C_ZONE);
    lv_obj_center(s->zone);
    s->arc = arc_at(s->box, d, NS_W, NS_START, NS_SWEEP, ARC_VALUE, C_W);
    lv_obj_center(s->arc);
    s->name = rnd_label(s->box, &rnd_18, C_GREY, 86);
    lv_obj_set_style_text_letter_space(s->name, 4, 0);
    s->val = rnd_label(s->box, &rnd_112, C_W, 0);
    s->unit = rnd_label(s->box, &rnd_26, C_GREY, CX - 20);

    for (int i = 1; i < RND_MULTI_SLOTS; i++) {
        s = &slot[i];
        int x = CX + (i - 2) * MULTI_MINI_DX, y = CX + MULTI_MINI_Y;
        s->box = box_in(layer);
        s->start = 135;
        s->sweep = 270;
        lv_obj_t *a = arc_at(s->box, MINI_D, MINI_W, 135, 270, ARC_BAND,
                             C_TRACK);
        lv_obj_set_pos(a, x - MULTI_MINI_R, y - MULTI_MINI_R);
        s->zone = arc_at(s->box, MINI_D, MINI_W, 135, 270, ARC_BAND, C_ZONE);
        lv_obj_set_pos(s->zone, x - MULTI_MINI_R, y - MULTI_MINI_R);
        s->arc = arc_at(s->box, MINI_D, MINI_W, 135, 270, ARC_VALUE, C_W);
        lv_obj_set_pos(s->arc, x - MULTI_MINI_R, y - MULTI_MINI_R);
        s->val = label_at(s->box, &rnd_26, C_W, x, y, 120);
        s->name = label_at(s->box, &rnd_18, C_GREY, x, y + MULTI_MINI_R - 8,
                           140);
    }
}

/* ------------------------------------------------------------ RETRO */
static void build_retro(void)
{
    lv_obj_t *bg = lv_img_create(layer);
    lv_img_set_src(bg, &face_retro_multi);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_CLICKABLE);

    /* big one: band, title, window readout, unit; the needle last */
    slot_t *s = &slot[0];
    s->box = box_in(layer);
    s->start = 180;
    s->sweep = 180;
    int bd = 2 * RETRO_ZONE_R + RETRO_ZONE_W;
    s->zone = arc_at(s->box, bd, RETRO_ZONE_W, 180, 180, ARC_BAND, C_BAND);
    lv_obj_center(s->zone);
    s->name = label_at(s->box, &rnd_barlow_23, C_INK_DIM, CX, CX - 128, 300);
    lv_obj_set_style_text_letter_space(s->name, 6, 0);
    s->val = label_at(s->box, &rnd_barlow_46, C_W, CX,
                      CX + (RMULTI_WIN_TOP + RMULTI_WIN_BOT) / 2 + 2,
                      RMULTI_WIN_W);
    s->unit = label_at(s->box, &rnd_barlow_20, C_INK_DIM, CX, CX - 40, 200);
    s->cx = s->cy = CX;
    s->r0 = -14;
    s->r1 = RETRO_TICK_R - 8;

    for (int i = 1; i < RND_MULTI_SLOTS; i++) {
        s = &slot[i];
        int x = CX + (i - 2) * MULTI_MINI_DX, y = CX + MULTI_MINI_Y;
        s->box = box_in(layer);
        s->start = 135;
        s->sweep = 270;
        int r = MULTI_MINI_R - 14;
        s->zone = arc_at(s->box, 2 * r + 4, 4, 135, 270, ARC_BAND, C_BAND);
        lv_obj_set_pos(s->zone, x - r - 2, y - r - 2);
        s->val = label_at(s->box, &rnd_barlow_23, C_W, x, y + 22, 90);
        s->name = label_at(s->box, &rnd_barlow_20, C_INK_DIM, x,
                           y + MULTI_MINI_R + 12, 140);
        lv_obj_set_style_text_letter_space(s->name, 2, 0);
        s->cx = x;
        s->cy = y;
        s->r0 = -6;
        s->r1 = MULTI_MINI_R - 12;
        s->needle = line_in(s->box, 4, C_NEEDLE, LV_OPA_COVER);
        disc(s->box, x, y, 12, lv_color_hex(0x111111), lv_color_hex(0x55575A));
    }
    s = &slot[0];
    s->needle = line_in(s->box, 7, C_NEEDLE, LV_OPA_COVER);
    disc(s->box, CX, CX, 40, lv_color_hex(0x111111), lv_color_hex(0x55575A));
}

/* -------------------------------------------------------------- RED */
LV_FONT_DECLARE(red_fis_54);
LV_FONT_DECLARE(red_txt_22);
#define C_RED_LIT  lv_color_hex(0xF02A26)
#define C_RED_VAL  lv_color_hex(0xEEEEEC)
#define C_RED_DIM  lv_color_hex(0xA0A0A0)

/* the big one's red past the limit, as on the RED dials: red ticks over
 * the white ones, and a band broken at the limit into short pieces that
 * grow until it is solid towards the end */
#define RB_MAX    80
#define RB_TICKS  21
static lv_obj_t *rb_piece[RB_MAX], *rb_end, *rb_tick[RB_TICKS];
static lv_point_t rb_piece_pt[RB_MAX][2], rb_tick_pt[RB_TICKS][2];

static lv_obj_t *red_line(lv_obj_t *par, int w)
{
    lv_obj_t *l = line_in(par, w, C_RED_LIT, LV_OPA_COVER);
    lv_obj_set_style_line_rounded(l, false, 0);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

static void red_zone(float z)
{
    const float r = (RED_R_BAND0 + RED_R_BAND1) / 2.0f, k = RND_W / 466.0f;
    int n = 0;
    float solid = NAN;
    for (int i = 0; i < RB_TICKS; i++) {
        bool on = z < 1 && i / 20.0f >= z - 1e-4f;
        if (!on) {
            lv_obj_add_flag(rb_tick[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        float a = (180 + 9 * i) * (float)M_PI / 180.0f, ln = (i % 5 ? 12 : 20) * k;
        rb_tick_pt[i][0].x = (lv_coord_t)lroundf(CX + (RED_R_TICK - ln) * cosf(a));
        rb_tick_pt[i][0].y = (lv_coord_t)lroundf(CX + (RED_R_TICK - ln) * sinf(a));
        rb_tick_pt[i][1].x = (lv_coord_t)lroundf(CX + RED_R_TICK * cosf(a));
        rb_tick_pt[i][1].y = (lv_coord_t)lroundf(CX + RED_R_TICK * sinf(a));
        lv_line_set_points(rb_tick[i], rb_tick_pt[i], 2);
        lv_obj_clear_flag(rb_tick[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (z < 1) {
        float a0 = 180 + 180 * (z < 0 ? 0 : z), a1 = 360, span = a1 - a0;
        for (float pos = a0; span > 0.3f && pos < a1 && n < RB_MAX;) {
            float t = (pos - a0) / span;
            float dash = 1.0f + 9.0f * powf(t, 1.3f);
            float gap = 2.4f * (1.0f - t / 0.8f);
            if (gap < 0.4f) {
                solid = pos;
                break;
            }
            float b0 = pos * (float)M_PI / 180.0f, b1 = fminf(pos + dash, a1) * (float)M_PI / 180.0f;
            rb_piece_pt[n][0].x = (lv_coord_t)lroundf(CX + r * cosf(b0));
            rb_piece_pt[n][0].y = (lv_coord_t)lroundf(CX + r * sinf(b0));
            rb_piece_pt[n][1].x = (lv_coord_t)lroundf(CX + r * cosf(b1));
            rb_piece_pt[n][1].y = (lv_coord_t)lroundf(CX + r * sinf(b1));
            lv_line_set_points(rb_piece[n], rb_piece_pt[n], 2);
            lv_obj_clear_flag(rb_piece[n], LV_OBJ_FLAG_HIDDEN);
            n++;
            pos += dash + gap;
        }
    }
    for (int i = n; i < RB_MAX; i++) lv_obj_add_flag(rb_piece[i], LV_OBJ_FLAG_HIDDEN);
    if (!isnan(solid) && 360 - solid >= 1.0f) {
        lv_arc_set_angles(rb_end, (uint16_t)lroundf(solid) % 360, 0);
        lv_obj_clear_flag(rb_end, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(rb_end, LV_OBJ_FLAG_HIDDEN);
    }
}

/* the A4 gauge's dials (ui_look_red.c): red_multi has the face, the big
 * scale over the top half and the three small dials; live the red zones,
 * the needles, name, value and unit */
static void build_red(void)
{
    lv_obj_t *bg = lv_img_create(layer);
    lv_img_set_src(bg, &red_multi);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_CLICKABLE);

    slot_t *s = &slot[0];
    s->box = box_in(layer);
    s->start = 180;
    s->sweep = 180;
    /* the zone: red_zone() (s->zone stays for slot_setup, never shown) */
    s->zone = arc_at(s->box, 2 * RED_R_BAND1, RED_R_BAND1 - RED_R_BAND0, 180, 180,
                     ARC_BAND, C_RED_LIT);
    lv_obj_add_flag(s->zone, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < RB_MAX; i++) rb_piece[i] = red_line(s->box, RED_R_BAND1 - RED_R_BAND0);
    rb_end = lv_arc_create(s->box);
    lv_obj_remove_style_all(rb_end);
    lv_obj_set_size(rb_end, 2 * RED_R_BAND1, 2 * RED_R_BAND1);
    lv_obj_center(rb_end);
    lv_arc_set_bg_angles(rb_end, 0, 0);
    lv_obj_set_style_arc_width(rb_end, RED_R_BAND1 - RED_R_BAND0, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(rb_end, C_RED_LIT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(rb_end, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(rb_end, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(rb_end, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(rb_end, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < RB_TICKS; i++) {
        rb_tick[i] = red_line(s->box, (i % 5 ? 4.5f : 7.0f) + 1.5f);
    }
    s->name = label_at(s->box, &red_txt_22, C_RED_DIM, CX, CX - 130, 300);
    lv_obj_set_style_text_letter_space(s->name, 3, 0);
    s->val = label_at(s->box, &red_fis_54, C_RED_VAL, CX, CX - 82, 260);
    s->unit = label_at(s->box, &red_txt_22, C_RED_DIM, CX, CX - 40, 220);
    s->cx = s->cy = CX;
    s->r0 = -16;
    s->r1 = RED_R_TICK - 26;

    for (int i = 1; i < RND_MULTI_SLOTS; i++) {
        s = &slot[i];
        int x = CX + (i - 2) * MULTI_MINI_DX, y = CX + MULTI_MINI_Y;
        s->box = box_in(layer);
        s->start = 135;
        s->sweep = 270;
        int r = MULTI_MINI_R - 18;
        s->zone = arc_at(s->box, 2 * r + 4, 4, 135, 270, ARC_BAND, C_RED_LIT);
        lv_obj_set_pos(s->zone, x - r - 2, y - r - 2);
        lv_obj_set_style_arc_opa(s->zone, LV_OPA_40, LV_PART_MAIN);   /* a hint */
        s->val = label_at(s->box, &red_txt_22, C_RED_VAL, x, y + 24, 90);
        s->name = label_at(s->box, &red_txt_22, C_RED_DIM, x, y + MULTI_MINI_R + 14, 140);
        s->cx = x;
        s->cy = y;
        s->r0 = -6;
        s->r1 = MULTI_MINI_R - 10;
        s->needle = line_in(s->box, 3, C_RED_LIT, LV_OPA_COVER);
        disc(s->box, x, y, 12, lv_color_hex(0x060607), lv_color_hex(0xB0181C));
    }
    s = &slot[0];
    s->needle = line_in(s->box, 6, C_RED_LIT, LV_OPA_COVER);
    disc(s->box, CX, CX, 44, lv_color_hex(0x060607), lv_color_hex(0xB0181C));
}

void rnd_multi_build(lv_obj_t *gauge_scr)
{
    style = g_rnd_set.look == RND_LOOK_RETRO ? ST_RETRO :
            g_rnd_set.look == RND_LOOK_RED ? ST_RED : ST_NOTSTOCK;
    memset(slot, 0, sizeof slot);
    layer = lv_obj_create(gauge_scr);
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, RND_W, RND_H);
    lv_obj_set_style_bg_color(layer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(layer, LV_OPA_COVER, 0);
    lv_obj_clear_flag(layer, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    if (style == ST_RETRO)    build_retro();
    else if (style == ST_RED) build_red();
    else                      build_notstock();

    regen_lbl = rnd_label(layer, &rnd_18, C_REGEN,
                          style == ST_RETRO ? CX + 20 : CX + 10);
    lv_obj_set_style_text_letter_space(regen_lbl, 2, 0);
    rnd_dots(layer, CX + 185, dot);
    for (int i = 0; i < RND_MULTI_SLOTS; i++) slot[i].id = -1;
    lv_obj_add_flag(layer, LV_OBJ_FLAG_HIDDEN);
}

void rnd_multi_show(bool on)
{
    if (!layer) return;
    if (!on) {
        lv_obj_add_flag(layer, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(layer, LV_OBJ_FLAG_HIDDEN);
    lv_color_t off_c = style == ST_RETRO ? lv_color_hex(0x55575A) : C_DOT;
    rnd_dots_set(dot, RND_MULTI, C_W, off_c);
    regen_on = -1;
    for (int i = 0; i < RND_MULTI_SLOTS; i++) {
        slot[i].id = -1;              /* set up again on the next update */
        slot[i].shown = NAN;
    }
    lv_obj_fade_in(layer, 250, 0);
}

/* where the reading stands, 0..1 */
static void slot_frac(slot_t *s, float f)
{
    if (s->arc && style == ST_NOTSTOCK) {
        lv_arc_set_value(s->arc, (int16_t)lroundf(f * 1000));
        /* nothing to show: no stub of an arc either */
        if (f < 0.001f) lv_obj_add_flag(s->arc, LV_OBJ_FLAG_HIDDEN);
        else            lv_obj_clear_flag(s->arc, LV_OBJ_FLAG_HIDDEN);
    }
    if (s->needle) {
        float a = (s->start + f * s->sweep) * (float)M_PI / 180.0f;
        float c = cosf(a), sn = sinf(a);
        s->np[0].x = (lv_coord_t)lroundf(s->cx + s->r0 * c);
        s->np[0].y = (lv_coord_t)lroundf(s->cy + s->r0 * sn);
        s->np[1].x = (lv_coord_t)lroundf(s->cx + s->r1 * c);
        s->np[1].y = (lv_coord_t)lroundf(s->cy + s->r1 * sn);
        lv_line_set_points(s->needle, s->np, 2);
    }
}

/* over the limit, or back under it */
static void slot_warn(slot_t *s, bool w)
{
    lv_obj_set_style_text_color(s->val, w ? C_RED : C_W, 0);
    if (s->arc && style == ST_NOTSTOCK) {
        lv_obj_set_style_arc_color(s->arc, w ? C_RED : C_W, LV_PART_INDICATOR);
    }
}

/* a slot took a new value: name, unit, red zone */
static void slot_setup(slot_t *s, int id)
{
    s->id = id;
    s->warn = s->big = -2;
    s->shown = NAN;
    if (id == RND_MV_NONE) {
        lv_obj_add_flag(s->box, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s->box, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s->name, rnd_page_name(id));
    if (s->unit) lv_label_set_text(s->unit, rnd_unit(id));
    /* red zone from the warn limit to the end of the scale */
    float z = frac_of(id, g_rnd_set.warn[id]);
    if (style == ST_RED && s == &slot[0]) {
        red_zone(z);
    } else if (z < 1) {
        lv_obj_clear_flag(s->zone, LV_OBJ_FLAG_HIDDEN);
        lv_arc_set_bg_angles(s->zone, (uint16_t)lroundf(z * s->sweep),
                             (uint16_t)s->sweep);
    } else {
        lv_obj_add_flag(s->zone, LV_OBJ_FLAG_HIDDEN);
    }
}

void rnd_multi_update(const rnd_data_t *d)
{
    for (int i = 0; i < RND_MULTI_SLOTS; i++) {
        slot_t *s = &slot[i];
        int id = g_rnd_set.multi[i];
        if (id != RND_MV_NONE && !(id < RND_COUNT || id == RND_WARN_SOOT)) {
            id = RND_MV_NONE;
        }
        if (id != s->id) slot_setup(s, id);
        if (id == RND_MV_NONE) continue;

        float v = value_of(d, id);
        char buf[16];
        fmt(buf, sizeof buf, id, v);
        lv_label_set_text(s->val, buf);

        float f = isnan(v) ? 0 : frac_of(id, v);
        if (isnan(s->shown)) s->shown = 0;
        s->shown += (f - s->shown) * 0.25f;
        slot_frac(s, s->shown);

        int w = !isnan(v) && v >= g_rnd_set.warn[id];
        if (w != s->warn) {
            s->warn = w;
            slot_warn(s, w);
        }
        if (i == 0 && style == ST_NOTSTOCK) {
            /* four digits and more: the smaller font, like the gauges */
            int digits = 0;
            for (const char *c = buf; *c; c++) digits += *c != '.';
            int big = digits < 4;
            if (big != s->big) {
                s->big = big;
                const lv_font_t *ft = big ? &rnd_112 : &rnd_84;
                lv_obj_set_style_text_font(s->val, ft, 0);
                lv_obj_set_y(s->val, CX - 78 - lv_font_get_line_height(ft) / 2);
            }
        }
    }
    int r = rnd_regen_active();
    if (r != regen_on) {
        regen_on = r;
        lv_label_set_text(regen_lbl, r ? TR("DPF REGEN", "REGEN. DPF") : "");
    }
}

/* ----------------------------------------------------- page icon */
/* MULTI has no drawn icon: a small picture of its layout, one wide bar
 * over three squares */
void rnd_multi_icon(lv_obj_t *par, lv_coord_t y, lv_obj_t **out)
{
    lv_obj_t *box = lv_obj_create(par);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 52, 52);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = lv_obj_create(box);
        lv_obj_remove_style_all(b);
        if (i == 0) {
            lv_obj_set_size(b, 52, 26);
            lv_obj_set_pos(b, 0, 0);
        } else {
            lv_obj_set_size(b, 14, 14);
            lv_obj_set_pos(b, (i - 1) * 19, 34);
        }
        lv_obj_set_style_radius(b, 4, 0);
        lv_obj_set_style_bg_color(b, C_W, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
    }
    *out = box;
}

/* ---------------------------------------------------------- the editor */
static lv_obj_t *ed_scr, *ed_lbl[RND_MULTI_SLOTS];

static void ed_show(void)
{
    for (int i = 0; i < RND_MULTI_SLOTS; i++) {
        int id = g_rnd_set.multi[i];
        lv_label_set_text(ed_lbl[i], id == RND_MV_NONE ?
                          TR("EMPTY", "NIC") : rnd_page_name(id));
        lv_obj_set_style_text_color(ed_lbl[i], id == RND_MV_NONE ? C_DIM : C_W,
                                    0);
    }
}

static void ed_tap(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    int n = (int)sizeof CHOICES;
    int at = 0;
    for (int k = 0; k < n; k++) {
        if (CHOICES[k] == g_rnd_set.multi[i]) at = k;
    }
    uint8_t next = CHOICES[(at + 1) % n];
    if (i == 0 && next == RND_MV_NONE) next = CHOICES[0];   /* big one: never empty */
    g_rnd_set.multi[i] = next;
    ed_show();
}

static void ed_back(lv_event_t *e)
{
    (void)e;
    rnd_settings_save();
    rnd_pages_open();
}

static lv_obj_t *ed_button(lv_coord_t x, lv_coord_t y, lv_coord_t w,
                           lv_coord_t h, int i)
{
    lv_obj_t *b = lv_obj_create(ed_scr);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_set_pos(b, x - w / 2, y - h / 2);
    lv_obj_set_style_radius(b, 22, 0);
    lv_obj_set_style_bg_color(b, C_PANEL, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, C_EDGE, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_bg_color(b, C_EDGE, LV_STATE_PRESSED);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(b, ed_tap, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    ed_lbl[i] = label_in(b, i ? &rnd_18 : &rnd_26, C_W);
    lv_obj_set_width(ed_lbl[i], w - 8);
    lv_label_set_long_mode(ed_lbl[i], LV_LABEL_LONG_WRAP);
    lv_obj_center(ed_lbl[i]);
    return b;
}

void rnd_multi_edit_create(void)
{
    if (ed_scr) lv_obj_del(ed_scr);           /* rebuilt for a new language */
    ed_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ed_scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ed_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ed_scr, LV_OBJ_FLAG_SCROLLABLE);
    rnd_on_long(ed_scr, ed_back);

    lv_obj_t *t = rnd_label(ed_scr, &rnd_18, C_GREY, 64);
    lv_obj_set_style_text_letter_space(t, 4, 0);
    lv_label_set_text(t, "MULTI");

    ed_button(CX, CX - 70, 300, 120, 0);
    for (int i = 1; i < RND_MULTI_SLOTS; i++) {
        ed_button(CX + (i - 2) * MINI_DX, MINI_Y, 128, 84, i);
    }
    lv_obj_t *h = rnd_label(ed_scr, &rnd_18, C_DIM, CX - 2);
    lv_label_set_text(h, TR("TAP: NEXT VALUE", "ŤUKNI: DALŠÍ HODNOTA"));
    h = rnd_label(ed_scr, &rnd_18, C_DIM, CX + 160);
    lv_label_set_text(h, TR("LONG PRESS: BACK", "PODRŽ: ZPĚT"));
}

void rnd_multi_edit_open(void)
{
    ed_show();
    lv_scr_load(ed_scr);
}
