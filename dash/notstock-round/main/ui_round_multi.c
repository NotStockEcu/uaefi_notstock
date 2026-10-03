/* Round gauge UI: MULTI, several values on one page. See ui_round.h.
 *
 * One big value in the middle with an arc over the top, three small gauges
 * in a row below it, like a CAN Checked MFD's multi view. What goes where
 * is g_rnd_set.multi[], set on the editor screen (SETTINGS -> PAGES ->
 * MULTI -> EDIT): tap a slot for the next value.
 *
 * MULTI is a page like the gauges (it has its place in the swipe order and
 * can be hidden). On the gauge screen it is a black layer over the look,
 * shown only on its page; gestures, long press and double tap go through to
 * the screen as on any other page.
 */
#include "ui_round_int.h"

#include <math.h>
#include <stdio.h>

#define MAIN_R      214          /* centre line of the big arc */
#define MAIN_W      22
#define MAIN_START  162          /* degrees, 0 = 3 o'clock, clockwise */
#define MAIN_SWEEP  216          /* over the top */
#define MINI_Y      (CX + 92)    /* small gauges: centre line */
#define MINI_DX     132
#define MINI_D      104          /* small arc's outer diameter */
#define MINI_W      9
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
typedef struct {
    lv_obj_t *box, *zone, *arc, *val, *name, *unit;
    float shown;
    int id, warn, big;
} slot_t;

static lv_obj_t *layer, *regen_lbl, *dot[RND_PAGES];
static slot_t slot[RND_MULTI_SLOTS];
static int regen_on;

/* bg: the arc's own band (the track, or the red zone); value: the
 * indicator drawn over it (the reading) */
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

static void build_main(slot_t *s)
{
    s->box = lv_obj_create(layer);
    lv_obj_remove_style_all(s->box);
    lv_obj_set_size(s->box, RND_W, RND_H);
    lv_obj_clear_flag(s->box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    int d = 2 * MAIN_R + MAIN_W;
    lv_obj_center(arc_at(s->box, d, MAIN_W, MAIN_START, MAIN_SWEEP, ARC_BAND,
                         C_TRACK));
    s->zone = arc_at(s->box, d, MAIN_W, MAIN_START, MAIN_SWEEP, ARC_BAND,
                     C_ZONE);
    lv_obj_center(s->zone);
    s->arc = arc_at(s->box, d, MAIN_W, MAIN_START, MAIN_SWEEP, ARC_VALUE, C_W);
    lv_obj_center(s->arc);

    s->name = rnd_label(s->box, &rnd_18, C_GREY, 86);
    lv_obj_set_style_text_letter_space(s->name, 4, 0);
    s->val = rnd_label(s->box, &rnd_112, C_W, 0);
    s->unit = rnd_label(s->box, &rnd_26, C_GREY, CX - 20);
}

static void build_mini(slot_t *s, int i)
{
    int x = CX + (i - 2) * MINI_DX;
    s->box = lv_obj_create(layer);
    lv_obj_remove_style_all(s->box);
    lv_obj_set_size(s->box, MINI_D + 40, MINI_D + 40);
    lv_obj_set_pos(s->box, x - (MINI_D + 40) / 2, MINI_Y - MINI_D / 2 - 6);
    lv_obj_clear_flag(s->box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_align(arc_at(s->box, MINI_D, MINI_W, 135, 270, ARC_BAND, C_TRACK),
                 LV_ALIGN_TOP_MID, 0, 6);
    s->zone = arc_at(s->box, MINI_D, MINI_W, 135, 270, ARC_BAND, C_ZONE);
    lv_obj_align(s->zone, LV_ALIGN_TOP_MID, 0, 6);
    s->arc = arc_at(s->box, MINI_D, MINI_W, 135, 270, ARC_VALUE, C_W);
    lv_obj_align(s->arc, LV_ALIGN_TOP_MID, 0, 6);

    s->val = label_in(s->box, &rnd_26, C_W);
    lv_obj_align(s->val, LV_ALIGN_TOP_MID, 0, 6 + MINI_D / 2 - 15);
    s->name = label_in(s->box, &rnd_18, C_GREY);
    lv_obj_align(s->name, LV_ALIGN_TOP_MID, 0, 6 + MINI_D - 20);
    s->unit = NULL;
}

void rnd_multi_build(lv_obj_t *gauge_scr)
{
    layer = lv_obj_create(gauge_scr);
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, RND_W, RND_H);
    lv_obj_set_style_bg_color(layer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(layer, LV_OPA_COVER, 0);
    lv_obj_clear_flag(layer, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    build_main(&slot[0]);
    for (int i = 1; i < RND_MULTI_SLOTS; i++) build_mini(&slot[i], i);
    regen_lbl = rnd_label(layer, &rnd_18, C_REGEN, CX + 10);
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
    rnd_dots_set(dot, RND_MULTI, C_W, C_DOT);
    regen_on = -1;
    for (int i = 0; i < RND_MULTI_SLOTS; i++) {
        slot[i].id = -1;              /* set up again on the next update */
        slot[i].shown = NAN;
    }
    lv_obj_fade_in(layer, 250, 0);
}

/* a slot took a new value: name, unit, red zone */
static void slot_setup(slot_t *s, int id)
{
    s->id = id;
    s->warn = s->big = -1;
    s->shown = NAN;
    if (id == RND_MV_NONE) {
        lv_obj_add_flag(s->box, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s->box, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s->name, rnd_page_name(id));
    if (s->unit) lv_label_set_text(s->unit, rnd_unit(id));
    /* red zone from the warn limit to the end of the scale */
    int sweep = s == &slot[0] ? MAIN_SWEEP : 270;
    float z = frac_of(id, g_rnd_set.warn[id]);
    if (z < 1) {
        lv_obj_clear_flag(s->zone, LV_OBJ_FLAG_HIDDEN);
        lv_arc_set_bg_angles(s->zone, (uint16_t)lroundf(z * sweep),
                             (uint16_t)sweep);
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
        lv_arc_set_value(s->arc, (int16_t)lroundf(s->shown * 1000));

        int w = !isnan(v) && v >= g_rnd_set.warn[id];
        if (w != s->warn) {
            s->warn = w;
            lv_obj_set_style_text_color(s->val, w ? C_RED : C_W, 0);
            lv_obj_set_style_arc_color(s->arc, w ? C_RED : C_W,
                                       LV_PART_INDICATOR);
        }
        if (i == 0) {
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
