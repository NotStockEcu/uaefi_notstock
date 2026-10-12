/* T5.1 pages of the 2.8" gauge, see ui_t5.h. Cards on the honeycomb, as on
 * the 2" EMU gauge (../notstock-lcd2-emu/main/ui_l2.c): a dark glass panel
 * with a yellow edge on the left that turns orange or red with the value. */
#include "ui_t5.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

LV_FONT_DECLARE(l2_num_64);
LV_FONT_DECLARE(l2_num_46);
LV_FONT_DECLARE(l2_txt_20);
LV_FONT_DECLARE(l2_txt_15);
LV_IMG_DECLARE(l2_bg);

#define C_YEL     lv_color_hex(0xFFD500)     /* NOT STOCK */
#define C_TEXT    lv_color_hex(0xF2F2F2)
#define C_SMALL   lv_color_hex(0x9AA0A6)
#define C_WARN    lv_color_hex(0xFF8A00)
#define C_RED     lv_color_hex(0xFF2A2A)
#define C_COLD    lv_color_hex(0x3AA0FF)
#define C_LED_ON  lv_color_hex(0x2BFF5A)
#define C_SEG_OFF lv_color_hex(0x24272B)
#define C_DOT     lv_color_hex(0x3A3F45)
#define C_CARD    lv_color_hex(0x000000)

/* ----------------------------------------------------------- channels */
typedef struct {
    const char *name, *unit, *fmt;
    float lo, hi;                 /* the bar */
    float lo_alarm, lo_warn;      /* NAN: none */
    float hi_warn, hi_alarm;
    bool  lo_cold;                /* under lo_warn is cold (blue), not bad */
} chan_t;

/* the T5.1 2.0 TDI (CAAC, 140 hp): boost about 1.4 bar at full load, EGT
 * sensor 1 is before the turbo */
static const chan_t CH[T5_N] = {
    [T5_BOOST] = { "BOOST", "bar", "%.2f", 0.0f, 2.5f,
                   NAN, NAN, 1.6f, 1.9f, false },
    [T5_EGT]   = { "EGT", "\xC2\xB0" "C", "%.0f", 0, 1000,
                   NAN, NAN, 750, 830, false },
    [T5_OILT]  = { "OIL", "\xC2\xB0" "C", "%.0f", 40, 150,
                   NAN, 60, 115, 125, true },
    [T5_CLT]   = { "COOLANT", "\xC2\xB0" "C", "%.0f", 20, 120,
                   NAN, 60, 100, 108, true },
    [T5_IAT]   = { "INTAKE", "\xC2\xB0" "C", "%.0f", -20, 80,
                   NAN, NAN, 50, 65, false },
    [T5_RPM]   = { "RPM", "", "%.0f", 0, 5000,
                   NAN, NAN, 4200, 4600, false },
    [T5_SPEED] = { "SPEED", "km/h", "%.0f", 0, 200,
                   NAN, NAN, NAN, NAN, false },
    /* the soot limit: the 7" dash's guess (ui_theme_obd.c) until a
     * regeneration has been seen */
    [T5_SOOT]  = { "SOOT", "g", "%.1f", 0, 30,
                   NAN, NAN, 20, 24, false },
    [T5_DP]    = { "DPF", "mbar", "%.0f", 0, 300,
                   NAN, NAN, NAN, NAN, false },
    [T5_KM]    = { "REGEN", "km ago", "%.0f", 0, 1000,
                   NAN, NAN, NAN, NAN, false },
};

typedef enum { L_OK, L_COLD, L_WARN, L_ALARM } level_t;

static level_t level(const chan_t *c, float x)
{
    if (x >= c->hi_alarm || x <= c->lo_alarm) return L_ALARM;
    if (x >= c->hi_warn) return L_WARN;
    if (x <= c->lo_warn) return c->lo_cold ? L_COLD : L_WARN;
    return L_OK;
}

static lv_color_t level_color(level_t l)
{
    switch (l) {
    case L_ALARM: return C_RED;
    case L_WARN:  return C_WARN;
    case L_COLD:  return C_COLD;
    default:      return C_YEL;
    }
}

static void fmt(char *buf, size_t n, const char *f, float x)
{
    snprintf(buf, n, f, x);
    if (buf[0] == '-' && atof(buf) == 0) snprintf(buf, n, f, 0.0);
}

static float frac(const chan_t *c, float x)
{
    float k = (x - c->lo) / (c->hi - c->lo);
    return k < 0 ? 0 : k > 1 ? 1 : k;
}

/* ------------------------------------------------------------ widgets */
static lv_obj_t *rect(lv_obj_t *par, lv_coord_t x, lv_coord_t y,
                      lv_coord_t w, lv_coord_t h, lv_color_t c)
{
    lv_obj_t *o = lv_obj_create(par);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static lv_obj_t *text(lv_obj_t *par, const lv_font_t *f, lv_color_t c,
                      const char *t)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_label_set_text(l, t);
    return l;
}

/* a right-aligned label: its right edge stays at x */
static lv_obj_t *text_r(lv_obj_t *par, const lv_font_t *f, lv_color_t c,
                        lv_coord_t x, lv_coord_t y, lv_coord_t w)
{
    lv_obj_t *l = text(par, f, c, "");
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(l, w);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(l, x - w, y);
    return l;
}

/* the card: dark glass, rounded, the accent edge on the left */
typedef struct {
    lv_obj_t *card, *edge;
} card_t;

static card_t card(lv_obj_t *par, lv_coord_t x, lv_coord_t y,
                   lv_coord_t w, lv_coord_t h)
{
    card_t c;
    c.card = rect(par, x, y, w, h, C_CARD);
    lv_obj_set_style_bg_opa(c.card, LV_OPA_50, 0);
    lv_obj_set_style_radius(c.card, 6, 0);
    lv_obj_set_style_clip_corner(c.card, true, 0);
    lv_obj_set_style_border_color(c.card, lv_color_hex(0x30343A), 0);
    lv_obj_set_style_border_width(c.card, 1, 0);
    c.edge = rect(c.card, 0, 0, 4, h, C_YEL);
    return c;
}

static lv_obj_t *page_obj(lv_obj_t *scr)
{
    lv_obj_t *o = lv_obj_create(scr);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, T5_W, T5_H);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static void title(lv_obj_t *par, int ch, lv_coord_t x, lv_coord_t y,
                  const lv_font_t *f)
{
    lv_obj_t *n = text(par, f, C_YEL, CH[ch].name);
    lv_obj_set_pos(n, x, y);
    if (CH[ch].unit[0]) {
        lv_obj_t *u = text(par, &l2_txt_15, C_SMALL, CH[ch].unit);
        lv_obj_align_to(u, n, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -1);
    }
}

/* the two big cards on the left of page 1; 4 px between cards */
#define CARD_W 196
#define CARD_H 100

/* ---------------------------------------------------- BOOST: LED bar */
#define SEG_N   25              /* 0 .. 2.5 bar, 0.1 a segment */
#define SEG_W   6
#define SEG_GAP 1
#define SEG_X   12

static struct {
    card_t c;
    lv_obj_t *seg[SEG_N];
    lv_color_t seg_col[SEG_N];
    lv_obj_t *val, *max, *peak;
    float max_v;
    int lit;                    /* segments shown last time, -1: none */
} s_boost;

static void boost_create(lv_obj_t *p)
{
    s_boost.c = card(p, 6, 26, CARD_W, CARD_H);
    lv_obj_t *c = s_boost.c.card;
    title(c, T5_BOOST, 12, 6, &l2_txt_20);
    s_boost.max = text(c, &l2_txt_15, C_SMALL, "");
    lv_obj_set_pos(s_boost.max, 12, 30);
    s_boost.val = text_r(c, &l2_num_64, C_TEXT, CARD_W - 8, 0, 130);
    for (int i = 0; i < SEG_N; i++) {
        lv_coord_t x = SEG_X + i * (SEG_W + SEG_GAP);
        s_boost.seg_col[i] = level_color(level(&CH[T5_BOOST], (i + 0.5f) * 0.1f));
        s_boost.seg[i] = rect(c, x, 64, SEG_W, 14, C_SEG_OFF);
        lv_obj_set_style_radius(s_boost.seg[i], 1, 0);
    }
    /* the peak: a white segment cap */
    s_boost.peak = rect(c, 0, 61, SEG_W, 20, C_TEXT);
    lv_obj_set_style_bg_opa(s_boost.peak, LV_OPA_80, 0);
    lv_obj_add_flag(s_boost.peak, LV_OBJ_FLAG_HIDDEN);
    static const char *const LBL[] = { "0", "1", "2" };
    for (int k = 0; k < 3; k++) {
        lv_coord_t x = SEG_X + (k * 10) * (SEG_W + SEG_GAP);
        rect(c, x, 79, 1, 3, C_SMALL);
        lv_obj_t *l = text(c, &l2_txt_15, C_SMALL, LBL[k]);
        lv_obj_set_pos(l, x, 81);
    }
    s_boost.max_v = NAN;
    s_boost.lit = -1;
}

static void boost_update(const t5_view_t *v, bool blink_on)
{
    float x = v->x[T5_BOOST];
    char buf[24];
    if (isnan(x)) {
        lv_label_set_text(s_boost.val, "--");
    } else {
        fmt(buf, sizeof buf, CH[T5_BOOST].fmt, x);
        /* "-0.05" in the big digits would run into the title */
        bool long_ = buf[0] == '-';
        lv_obj_set_style_text_font(s_boost.val, long_ ? &l2_num_46 : &l2_num_64, 0);
        lv_obj_set_y(s_boost.val, long_ ? 12 : 0);
        lv_label_set_text(s_boost.val, buf);
    }
    level_t lv = isnan(x) ? L_OK : level(&CH[T5_BOOST], x);
    lv_obj_set_style_text_color(s_boost.val,
        lv == L_ALARM && !blink_on ? C_RED : C_TEXT, 0);
    lv_obj_set_style_bg_color(s_boost.c.edge, level_color(lv), 0);
    if (isnan(s_boost.max_v)) {
        lv_label_set_text(s_boost.max, "");
    } else {
        fmt(buf, sizeof buf, "MAX %.2f", s_boost.max_v);
        lv_label_set_text(s_boost.max, buf);
    }

    int lit = -1;
    if (!isnan(x)) {
        lit = (int)floorf(x * 10.0f + 1e-3f);    /* 0.1 bar steps */
        if (lit < 0) lit = 0;
        if (lit > SEG_N) lit = SEG_N;
    }
    if (lit != s_boost.lit) {
        for (int i = 0; i < SEG_N; i++) {
            lv_obj_set_style_bg_color(s_boost.seg[i],
                i < lit ? s_boost.seg_col[i] : C_SEG_OFF, 0);
        }
        s_boost.lit = lit;
    }
    if (!isnan(s_boost.max_v) && s_boost.max_v >= 0.1f) {
        int pi = (int)floorf(s_boost.max_v * 10.0f + 1e-3f) - 1;
        if (pi > SEG_N - 1) pi = SEG_N - 1;
        lv_obj_set_x(s_boost.peak, SEG_X + pi * (SEG_W + SEG_GAP));
        lv_obj_clear_flag(s_boost.peak, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_boost.peak, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ------------------------------------------------- tiles: value + bar */
typedef struct {
    int ch;
    card_t c;
    lv_obj_t *val, *bar, *track;
    lv_coord_t bar_w;
} tile_t;

static tile_t s_tile[T5_N];
static int s_tiles;

static void tile_create(lv_obj_t *p, int ch, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h, const lv_font_t *num)
{
    tile_t *t = &s_tile[s_tiles++];
    t->ch = ch;
    t->c = card(p, x, y, w, h);
    lv_obj_t *c = t->c.card;
    bool big = w > 150;
    title(c, ch, 12, big ? 6 : 4, big ? &l2_txt_20 : &l2_txt_15);
    /* digits sit 4 px over the bar */
    t->val = text_r(c, num, C_TEXT, w - 8,
                    h - 12 - lv_font_get_line_height(num) + num->base_line,
                    big ? 130 : w - 16);
    t->bar_w = w - 20;
    t->track = rect(c, 12, h - 8, t->bar_w, 3, C_SEG_OFF);
    t->bar = rect(c, 12, h - 8, 0, 3, C_YEL);
}

static void tile_update(tile_t *t, const t5_view_t *v, bool blink_on)
{
    const chan_t *c = &CH[t->ch];
    float x = v->x[t->ch];
    char buf[24];
    level_t lv = L_OK;
    if (isnan(x)) {
        lv_label_set_text(t->val, "--");
        lv_obj_set_width(t->bar, 0);
    } else {
        lv = level(c, x);
        fmt(buf, sizeof buf, c->fmt, x);
        lv_label_set_text(t->val, buf);
        lv_obj_set_width(t->bar, (lv_coord_t)lroundf(frac(c, x) * t->bar_w));
    }
    lv_obj_set_style_text_color(t->val,
        lv == L_ALARM && !blink_on ? C_RED : C_TEXT, 0);
    lv_obj_set_style_bg_color(t->bar, level_color(lv), 0);
    lv_obj_set_style_bg_color(t->c.edge, level_color(lv), 0);
}

/* ------------------------------------------------------------- screen */
static lv_obj_t *s_page[T5_PAGES];
static lv_obj_t *s_link, *s_state, *s_dots[T5_PAGES];
static int s_cur;
static uint32_t s_frames;

static void gesture_cb(lv_event_t *e)
{
    (void)e;
    lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (d == LV_DIR_LEFT) ui_t5_page((s_cur + 1) % T5_PAGES);
    if (d == LV_DIR_RIGHT) ui_t5_page((s_cur + T5_PAGES - 1) % T5_PAGES);
}

void ui_t5_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_t *bg = lv_img_create(scr);
    lv_img_set_src(bg, &l2_bg);
    lv_obj_set_pos(bg, 0, 0);

    /* status line */
    s_link = rect(scr, 10, 7, 9, 9, C_RED);
    lv_obj_set_style_radius(s_link, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *ns = text(scr, &l2_txt_15, C_YEL, "NOT STOCK");
    lv_obj_align(ns, LV_ALIGN_TOP_MID, 0, 1);
    for (int i = 0; i < T5_PAGES; i++) {
        s_dots[i] = rect(scr, T5_W / 2 - 7 + i * 10, 19, 4, 4, C_DOT);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, 0);
    }
    s_state = text_r(scr, &l2_txt_15, C_SMALL, T5_W - 8, 1, 90);

    /* page 1: boost, EGT, oil, coolant */
    s_page[0] = page_obj(scr);
    boost_create(s_page[0]);
    tile_create(s_page[0], T5_EGT, 6, 130, CARD_W, CARD_H, &l2_num_64);
    tile_create(s_page[0], T5_OILT, 206, 26, 108, CARD_H, &l2_num_46);
    tile_create(s_page[0], T5_CLT, 206, 130, 108, CARD_H, &l2_num_46);

    /* page 2: six tiles */
    s_page[1] = page_obj(scr);
    static const int P2[] = { T5_IAT, T5_RPM, T5_SPEED, T5_SOOT, T5_DP, T5_KM };
    for (int i = 0; i < 6; i++) {
        tile_create(s_page[1], P2[i], 6 + (i % 3) * 104, 26 + (i / 3) * 104,
                    100, CARD_H, &l2_num_46);
    }

    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);
    ui_t5_page(0);
    lv_scr_load(scr);
}

void ui_t5_page(int page)
{
    if (page < 0 || page >= T5_PAGES) page = 0;
    s_cur = page;
    for (int i = 0; i < T5_PAGES; i++) {
        if (i == page) lv_obj_clear_flag(s_page[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_page[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_dots[i], i == page ? C_YEL : C_DOT, 0);
    }
}

void ui_t5_update(const t5_view_t *v)
{
    bool blink_on = (s_frames++ / 8) % 2;        /* ~2 Hz at 30 Hz */
    lv_obj_set_style_bg_color(s_link, v->link ? C_LED_ON : C_RED, 0);
    const char *st = v->state == T5_ST_SCANNING ? "SCAN"
                   : v->state == T5_ST_NO_ECU ? "NO ECU" : "";
    lv_label_set_text(s_state, st);
    lv_obj_set_style_text_color(s_state,
        v->state == T5_ST_NO_ECU ? C_RED : C_SMALL, 0);

    float b = v->x[T5_BOOST];             /* the max, on either page */
    if (!isnan(b) && (isnan(s_boost.max_v) || b > s_boost.max_v)) {
        s_boost.max_v = b;
    }
    if (s_cur == 0) boost_update(v, blink_on);
    for (int i = 0; i < s_tiles; i++) {
        if (lv_obj_get_parent(s_tile[i].c.card) == s_page[s_cur]) {
            tile_update(&s_tile[i], v, blink_on);
        }
    }
}
