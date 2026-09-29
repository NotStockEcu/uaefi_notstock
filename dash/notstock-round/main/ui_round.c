/* Round gauge UI, see ui_round.h.
 *
 * One page per value. The outer ring is the bar (track, red zone above the
 * warn limit, value arc), inside it the ticks and scale numbers, in the
 * middle title, value, unit and the peak. The bottom quarter the 270 degree
 * scale leaves open carries the page dots.
 *
 * Both lv_meters are rebuilt on a page change (only one page's scale is ever
 * in the heap); the value arc then sweeps up from the bottom of the scale on
 * its own, because the shown value is smoothed towards the live one.
 */
#include "ui_round.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(rnd_112);
LV_FONT_DECLARE(rnd_84);
LV_FONT_DECLARE(rnd_26);
LV_FONT_DECLARE(rnd_18);

#define C_BG     lv_color_hex(0x000000)
#define C_W      lv_color_hex(0xFFFFFF)
#define C_TITLE  lv_color_hex(0xC9CED4)
#define C_GREY   lv_color_hex(0x8A9096)
#define C_DIM    lv_color_hex(0x4A4F55)
#define C_TRACK  lv_color_hex(0x16191C)
#define C_ZONE   lv_color_hex(0x5A1414)
#define C_RED    lv_color_hex(0xE22424)
#define C_TICK   lv_color_hex(0x5A5F66)

#define CX       (RND_W / 2)
#define RING_W   18          /* value arc width */
#define TICK_D   (RND_W - 52) /* diameter of the tick meter */
#define SWEEP    270
#define ROT      135         /* scale starts bottom left */

typedef struct {
    const char *title, *unit, *note;   /* note: scale multiplier, or NULL */
    int lo, hi;                        /* scale in value * mul */
    int mul;
    int ticks, major;                  /* tick count, every major-th labelled */
    int label_div;                     /* scale number = value / label_div */
    int dec;                           /* decimals of the readout */
    float warn;                        /* NAN: none */
    bool peak;
} page_t;

static const page_t PAGE[RND_COUNT] = {
    [RND_WATER]   = { "WATER",   "\xC2\xB0" "C", NULL,     40, 140, 1,   21, 4, 1,   0, 105,  true },
    [RND_OIL]     = { "OIL",     "\xC2\xB0" "C", NULL,     40, 160, 1,   25, 4, 1,   0, 130,  true },
    [RND_BOOST]   = { "BOOST",   "bar",          NULL,     0,  250, 100, 26, 5, 100, 2, 2.2f, true },
    [RND_INTAKE]  = { "INTAKE",  "\xC2\xB0" "C", NULL,     0,  100, 1,   21, 4, 1,   0, 60,   true },
    [RND_EXHAUST] = { "EXHAUST", "\xC2\xB0" "C", "x100",   0, 1000, 1,   21, 4, 100, 0, 750,  true },
    [RND_RPM]     = { "ENGINE",  "rpm",          "x1000",  0, 5000, 1,   21, 4, 1000, 0, 4500, false },
};

static lv_obj_t *scr, *ring, *tickm, *center;
static lv_obj_t *title_lbl, *val_lbl, *unit_lbl, *peak_lbl, *note_lbl;
static lv_obj_t *dot[RND_COUNT];
static lv_meter_indicator_t *arc_val, *arc_zone;
static lv_meter_scale_t *ring_scale;
static int page;
static float shown = NAN;          /* smoothed value on the arc */
static float peak[RND_COUNT];
static int warn_on = -1;

/* ---------------------------------------------------------------- scale */
static void tick_label_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->class_p != &lv_meter_class || dsc->type != LV_METER_DRAW_PART_TICK
        || dsc->text == NULL) {
        return;
    }
    /* LVGL 8.4 leaves text_length unset here; its buffer is 16 bytes */
    const page_t *p = &PAGE[page];
    int v = (int)dsc->value;
    if (v % p->label_div == 0) {
        lv_snprintf(dsc->text, 16, "%d", v / p->label_div);
    } else {
        lv_snprintf(dsc->text, 16, "%d.%d", v / p->label_div,
                    (v % p->label_div) * 10 / p->label_div);
    }
}

static lv_obj_t *bare_meter(lv_coord_t d)
{
    lv_obj_t *m = lv_meter_create(scr);
    lv_obj_remove_style_all(m);
    lv_obj_set_size(m, d, d);
    lv_obj_center(m);
    lv_obj_clear_flag(m, LV_OBJ_FLAG_CLICKABLE);
    /* no needle pivot */
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_width(m, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(m, 0, LV_PART_INDICATOR);
    return m;
}

static void build_scale(void)
{
    const page_t *p = &PAGE[page];
    if (ring) lv_obj_del(ring);
    if (tickm) lv_obj_del(tickm);

    /* outer ring: track, red zone, value */
    ring = bare_meter(RND_W - 8);
    lv_obj_set_style_arc_rounded(ring, true, LV_PART_ITEMS);
    ring_scale = lv_meter_add_scale(ring);
    lv_meter_set_scale_ticks(ring, ring_scale, 0, 0, 0, C_BG);
    lv_meter_set_scale_range(ring, ring_scale, p->lo, p->hi, SWEEP, ROT);
    lv_meter_indicator_t *t = lv_meter_add_arc(ring, ring_scale, RING_W,
                                               C_TRACK, 0);
    lv_meter_set_indicator_start_value(ring, t, p->lo);
    lv_meter_set_indicator_end_value(ring, t, p->hi);
    arc_zone = NULL;
    if (!isnan(p->warn)) {
        arc_zone = lv_meter_add_arc(ring, ring_scale, RING_W, C_ZONE, 0);
        lv_meter_set_indicator_start_value(ring, arc_zone,
                                           (int32_t)lroundf(p->warn * p->mul));
        lv_meter_set_indicator_end_value(ring, arc_zone, p->hi);
    }
    arc_val = lv_meter_add_arc(ring, ring_scale, RING_W, C_W, 0);
    lv_meter_set_indicator_start_value(ring, arc_val, p->lo);
    lv_meter_set_indicator_end_value(ring, arc_val, p->lo);

    /* ticks and numbers */
    tickm = bare_meter(TICK_D);
    lv_obj_set_style_text_font(tickm, &rnd_26, LV_PART_TICKS);
    lv_obj_set_style_text_color(tickm, C_GREY, LV_PART_TICKS);
    lv_meter_scale_t *s = lv_meter_add_scale(tickm);
    lv_meter_set_scale_ticks(tickm, s, p->ticks, 2, 10, C_TICK);
    lv_meter_set_scale_major_ticks(tickm, s, p->major, 4, 20, C_TITLE, 26);
    lv_meter_set_scale_range(tickm, s, p->lo, p->hi, SWEEP, ROT);
    lv_obj_add_event_cb(tickm, tick_label_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

    lv_obj_move_background(tickm);
    lv_obj_move_background(ring);
}

/* ---------------------------------------------------------------- pages */
static void show_dots(void)
{
    for (int i = 0; i < RND_COUNT; i++) {
        lv_obj_set_width(dot[i], i == page ? 22 : 8);
        lv_obj_set_style_bg_color(dot[i], i == page ? C_W : C_DIM, 0);
    }
}

void ui_round_page(int p)
{
    page = (p % RND_COUNT + RND_COUNT) % RND_COUNT;
    const page_t *pg = &PAGE[page];
    build_scale();
    lv_label_set_text(title_lbl, pg->title);
    lv_label_set_text(unit_lbl, pg->unit);
    lv_label_set_text(note_lbl, pg->note ? pg->note : "");
    lv_label_set_text(peak_lbl, "");
    shown = NAN;
    warn_on = -1;
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

void ui_round_create(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);

    /* everything that fades on a page change */
    center = lv_obj_create(scr);
    lv_obj_remove_style_all(center);
    lv_obj_set_size(center, RND_W, RND_H);
    lv_obj_clear_flag(center, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    title_lbl = label(center, &rnd_26, C_TITLE, CX - 115);
    lv_obj_set_style_text_letter_space(title_lbl, 4, 0);
    val_lbl = label(center, &rnd_112, C_W, 0);
    unit_lbl = label(center, &rnd_26, C_GREY, CX + 65);
    peak_lbl = label(center, &rnd_18, C_DIM, CX + 103);
    lv_obj_set_style_text_letter_space(peak_lbl, 2, 0);
    note_lbl = label(scr, &rnd_18, C_DIM, CX + 139);

    for (int i = 0; i < RND_COUNT; i++) {
        dot[i] = lv_obj_create(scr);
        lv_obj_remove_style_all(dot[i]);
        lv_obj_set_size(dot[i], 8, 8);
        lv_obj_set_style_radius(dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(dot[i], LV_OBJ_FLAG_CLICKABLE);
    }
    /* dots centred as a row; the active one is wider */
    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 200, 10);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, CX + 185);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < RND_COUNT; i++) lv_obj_set_parent(dot[i], row);

    for (int i = 0; i < RND_COUNT; i++) peak[i] = NAN;
    ui_round_page(0);
    lv_scr_load(scr);
}

/* ---------------------------------------------------------------- values */
static void fmt(char *buf, size_t n, const page_t *p, float v)
{
    if (p->dec == 2) snprintf(buf, n, "%.2f", v);
    else             snprintf(buf, n, "%d", (int)lroundf(v));
}

void ui_round_update(const rnd_data_t *d)
{
    const page_t *p = &PAGE[page];
    float v = d->link ? d->v[page] : NAN;
    char buf[16];

    for (int i = 0; i < RND_COUNT; i++) {
        float x = d->link ? d->v[i] : NAN;
        if (PAGE[i].peak && !isnan(x) && (isnan(peak[i]) || x > peak[i])) {
            peak[i] = x;
        }
    }

    if (isnan(v)) {
        lv_label_set_text(val_lbl, "--");
        lv_obj_set_style_text_font(val_lbl, &rnd_112, 0);
        lv_label_set_text(peak_lbl, d->link ? "NOT READ" : "NO DATA");
        lv_obj_set_style_text_color(peak_lbl, C_RED, 0);
        shown = NAN;
        lv_meter_set_indicator_end_value(ring, arc_val, p->lo);
    } else {
        fmt(buf, sizeof buf, p, v);
        int digits = 0;
        for (const char *c = buf; *c; c++) digits += *c != '.';
        lv_obj_set_style_text_font(val_lbl, digits >= 4 ? &rnd_84 : &rnd_112,
                                   0);
        lv_label_set_text(val_lbl, buf);

        if (p->peak && !isnan(peak[page])) {
            char pk[16];
            fmt(pk, sizeof pk, p, peak[page]);
            snprintf(buf, sizeof buf, "MAX %s", pk);
            lv_label_set_text(peak_lbl, buf);
        } else {
            lv_label_set_text(peak_lbl, "");
        }
        lv_obj_set_style_text_color(peak_lbl, C_DIM, 0);

        /* the arc follows smoothly; after a page change it sweeps up */
        float target = v * p->mul;
        if (target < p->lo) target = (float)p->lo;
        if (target > p->hi) target = (float)p->hi;
        if (isnan(shown)) shown = (float)p->lo;
        shown += (target - shown) * 0.25f;
        lv_meter_set_indicator_end_value(ring, arc_val,
                                         (int32_t)lroundf(shown));
    }

    int w = !isnan(v) && !isnan(p->warn) && v >= p->warn;
    if (w != warn_on) {
        warn_on = w;
        lv_obj_set_style_text_color(val_lbl, w ? C_RED : C_W, 0);
        lv_meter_set_indicator_end_value(ring, arc_val,
            (int32_t)lroundf(isnan(shown) ? p->lo : shown));
        arc_val->type_data.arc.color = w ? C_RED : C_W;
        lv_obj_invalidate(ring);
    }
    /* value vertically centred on the dial, whatever the font */
    lv_obj_set_y(val_lbl, CX - lv_font_get_line_height(
        lv_obj_get_style_text_font(val_lbl, 0)) / 2 + 6);
}
