/* Round gauge UI, see ui_round.h.
 *
 * Each page is one pre-rendered face (tools/gen_faces.py: background, groove,
 * red zone, scale, icon, title) with LVGL drawing only what moves on top: the
 * value arc with a two-layer glow, the readout, the unit, the peak and the
 * page dots. The arc geometry comes from faces.h, so it lands in the baked
 * groove.
 *
 * A page change swaps the face image; the arc then sweeps up from the bottom
 * of the new scale on its own, because the shown value is smoothed towards
 * the live one, and the readout fades in.
 */
#include "ui_round.h"
#include "faces.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

_Static_assert(FACE_SIZE == RND_W, "faces.c was generated for another panel "
               "size: python tools/gen_faces.py --size RND_SIZE");
_Static_assert(FACE_COUNT == RND_COUNT, "pages in gen_faces.py and "
               "ui_round.h differ");

LV_FONT_DECLARE(rnd_112);
LV_FONT_DECLARE(rnd_84);
LV_FONT_DECLARE(rnd_26);
LV_FONT_DECLARE(rnd_18);

#define C_W      lv_color_hex(0xFFFFFF)
#define C_GREY   lv_color_hex(0x8A9096)
#define C_DIM    lv_color_hex(0x5A5F66)
#define C_DOT    lv_color_hex(0x3A3F45)
#define C_RED    lv_color_hex(0xFF3030)

#define CX       (RND_W / 2)
#define ARC_MAX  1000        /* arc range: fraction of the scale * 1000 */

/* readout per page; ranges and limits come from faces.h */
static const struct {
    int  dec;
    bool peak;
} FMT[RND_COUNT] = {
    [RND_WATER]   = { 0, true },
    [RND_OIL]     = { 0, true },
    [RND_BOOST]   = { 2, true },
    [RND_INTAKE]  = { 0, true },
    [RND_EXHAUST] = { 0, true },
    [RND_RPM]     = { 0, false },
    [RND_DPF]     = { 0, true },
};

/* value arc and its glow: width added, opacity */
static const struct { int extra; lv_opa_t opa; } GLOW[] = {
    { 30, 18 }, { 14, 45 }, { 0, LV_OPA_COVER },
};
#define N_ARC (int)(sizeof GLOW / sizeof GLOW[0])

static lv_obj_t *scr, *face, *center;
static lv_obj_t *arc[N_ARC];
static lv_obj_t *val_lbl, *unit_lbl, *peak_lbl;
static lv_obj_t *dot[RND_COUNT];
static int page;
static float shown = NAN;          /* smoothed arc position, 0..ARC_MAX */
static float peak[RND_COUNT];
static int warn_on = -1;

/* ---------------------------------------------------------------- build */
static lv_obj_t *make_arc(int extra, lv_opa_t opa)
{
    int w = FACE_ARC_W + extra;
    int d = 2 * FACE_ARC_R + w;
    lv_obj_t *a = lv_arc_create(scr);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, d, d);
    lv_obj_center(a);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(a, FACE_START);
    lv_arc_set_bg_angles(a, 0, FACE_SWEEP);
    lv_arc_set_range(a, 0, ARC_MAX);
    lv_arc_set_value(a, 0);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, w, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, C_W, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(a, opa, LV_PART_INDICATOR);
    return a;
}

static lv_obj_t *label(lv_obj_t *par, const lv_font_t *f, lv_color_t c,
                       lv_coord_t y)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, RND_W);
    lv_obj_set_pos(l, 0, y);
    lv_label_set_text(l, "");
    return l;
}

static void show_dots(void)
{
    for (int i = 0; i < RND_COUNT; i++) {
        lv_obj_set_width(dot[i], i == page ? 22 : 8);
        lv_obj_set_style_bg_color(dot[i], i == page ? C_W : C_DOT, 0);
    }
}

static void set_arc_color(lv_color_t c)
{
    for (int i = 0; i < N_ARC; i++) {
        lv_obj_set_style_arc_color(arc[i], c, LV_PART_INDICATOR);
    }
}

void ui_round_page(int p)
{
    page = (p % RND_COUNT + RND_COUNT) % RND_COUNT;
    lv_img_set_src(face, face_img[page]);
    lv_label_set_text(unit_lbl, FACE_PAGE[page].unit);
    lv_label_set_text(peak_lbl, "");
    shown = NAN;
    warn_on = -1;
    for (int i = 0; i < N_ARC; i++) lv_arc_set_value(arc[i], 0);
    show_dots();
    lv_obj_fade_in(center, 250, 0);
}

int ui_round_current(void)
{
    return page;
}

static void gesture_cb(lv_event_t *e)
{
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)  ui_round_page(page + 1);
    if (dir == LV_DIR_RIGHT) ui_round_page(page - 1);
}

void ui_round_create(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);

    face = lv_img_create(scr);
    lv_obj_set_pos(face, 0, 0);
    lv_obj_clear_flag(face, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < N_ARC; i++) arc[i] = make_arc(GLOW[i].extra,
                                                      GLOW[i].opa);

    /* the readout, faded in on a page change */
    center = lv_obj_create(scr);
    lv_obj_remove_style_all(center);
    lv_obj_set_size(center, RND_W, RND_H);
    lv_obj_clear_flag(center, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    val_lbl = label(center, &rnd_112, C_W, 0);
    unit_lbl = label(center, &rnd_26, C_GREY, CX + 68);
    peak_lbl = label(center, &rnd_18, C_DIM, CX + 106);
    lv_obj_set_style_text_letter_space(peak_lbl, 2, 0);

    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 220, 10);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, CX + 185);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < RND_COUNT; i++) {
        dot[i] = lv_obj_create(row);
        lv_obj_remove_style_all(dot[i]);
        lv_obj_set_size(dot[i], 8, 8);
        lv_obj_set_style_radius(dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(dot[i], LV_OBJ_FLAG_CLICKABLE);
    }

    for (int i = 0; i < RND_COUNT; i++) peak[i] = NAN;
    ui_round_page(0);
    lv_scr_load(scr);
}

/* ---------------------------------------------------------------- values */
static void fmt(char *buf, size_t n, int pg, float v)
{
    if (FMT[pg].dec == 2) snprintf(buf, n, "%.2f", v);
    else                  snprintf(buf, n, "%d", (int)lroundf(v));
}

void ui_round_update(const rnd_data_t *d)
{
    const face_page_t *p = &FACE_PAGE[page];
    float v = d->link ? d->v[page] : NAN;
    char buf[16];

    for (int i = 0; i < RND_COUNT; i++) {
        float x = d->link ? d->v[i] : NAN;
        if (FMT[i].peak && !isnan(x) && (isnan(peak[i]) || x > peak[i])) {
            peak[i] = x;
        }
    }

    float target = 0;
    if (isnan(v)) {
        lv_obj_set_style_text_font(val_lbl, &rnd_112, 0);
        lv_label_set_text(val_lbl, "--");
        lv_label_set_text(peak_lbl, d->link ? "NOT READ" : "NO DATA");
        lv_obj_set_style_text_color(peak_lbl, C_RED, 0);
    } else {
        fmt(buf, sizeof buf, page, v);
        int digits = 0;
        for (const char *c = buf; *c; c++) digits += *c != '.';
        lv_obj_set_style_text_font(val_lbl, digits >= 4 ? &rnd_84 : &rnd_112,
                                   0);
        lv_label_set_text(val_lbl, buf);

        if (FMT[page].peak && !isnan(peak[page])) {
            char pk[16];
            fmt(pk, sizeof pk, page, peak[page]);
            snprintf(buf, sizeof buf, "MAX %s", pk);
            lv_label_set_text(peak_lbl, buf);
        } else {
            lv_label_set_text(peak_lbl, "");
        }
        lv_obj_set_style_text_color(peak_lbl, C_DIM, 0);
        target = (v - p->lo) / (p->hi - p->lo) * ARC_MAX;
        if (target < 0) target = 0;
        if (target > ARC_MAX) target = ARC_MAX;
    }

    /* smoothed: after a page change the arc sweeps up from zero */
    if (isnan(shown)) shown = 0;
    shown += (target - shown) * 0.25f;
    for (int i = 0; i < N_ARC; i++) {
        lv_arc_set_value(arc[i], (int16_t)lroundf(shown));
    }
    /* nothing to show: no arc at all, not even the rounded start cap */
    for (int i = 0; i < N_ARC; i++) {
        if (shown < 1) lv_obj_add_flag(arc[i], LV_OBJ_FLAG_HIDDEN);
        else           lv_obj_clear_flag(arc[i], LV_OBJ_FLAG_HIDDEN);
    }

    int w = !isnan(v) && v >= p->warn;
    if (w != warn_on) {
        warn_on = w;
        lv_obj_set_style_text_color(val_lbl, w ? C_RED : C_W, 0);
        set_arc_color(w ? C_RED : C_W);
    }
    /* value vertically centred on the dial, whatever the font */
    lv_obj_set_y(val_lbl, CX - lv_font_get_line_height(
        lv_obj_get_style_text_font(val_lbl, 0)) / 2 + 14);
}
