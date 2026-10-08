/* EMU pages of the 2" gauge, see ui_l2.h. Cards on the honeycomb: a dark
 * glass panel with a yellow edge on the left that turns orange or red with
 * the channel. */
#include "ui_l2.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

LV_FONT_DECLARE(l2_num_64);
LV_FONT_DECLARE(l2_num_46);
LV_FONT_DECLARE(l2_num_36);
LV_FONT_DECLARE(l2_txt_20);
LV_FONT_DECLARE(l2_txt_15);
LV_IMG_DECLARE(l2_bg);

#define C_YEL     lv_color_hex(0xFFD500)     /* NOT STOCK */
#define C_YEL_DIM lv_color_hex(0x6B5A00)
#define C_TEXT    lv_color_hex(0xF2F2F2)
#define C_SMALL   lv_color_hex(0x9AA0A6)
#define C_WARN    lv_color_hex(0xFF8A00)
#define C_RED     lv_color_hex(0xFF2A2A)
#define C_COLD    lv_color_hex(0x3AA0FF)
#define C_LED_ON  lv_color_hex(0x2BFF5A)
#define C_SEG_OFF lv_color_hex(0x24272B)
#define C_DOT     lv_color_hex(0x3A3F45)
#define C_CARD    lv_color_hex(0x000000)

/* --------------------------------------------------------------- data */
static float fresh(float x, int64_t at, int64_t now)
{
    return at != 0 && now - at < L2_STALE_US ? x : NAN;
}

void l2_view_from(const emu_values_t *v, int64_t now_us, l2_view_t *o)
{
    const int64_t *f = v->frame_us;
    float map = fresh(v->map_kpa, f[0], now_us);
    /* boost over the EMU's barometer; before that comes: sea level */
    float baro = fresh(v->baro_kpa, f[2], now_us);
    if (!(baro > 50 && baro < 130)) baro = 101.3f;
    o->x[L2_BOOST] = (map - baro) / 100.0f;      /* NAN stays NAN */
    float lam = fresh(v->lambda, f[3], now_us);
    o->x[L2_LAMBDA] = lam > 0 ? lam : NAN;
    o->x[L2_IAT] = fresh(v->iat, f[0], now_us);
    o->x[L2_CLT] = fresh(v->clt, f[2], now_us);
    o->x[L2_TPS] = fresh(v->tps, f[0], now_us);
    o->x[L2_RPM] = fresh(v->rpm, f[0], now_us);
    o->x[L2_OILT] = fresh(v->oilt, f[2], now_us);
    o->x[L2_OILP] = fresh(v->oilp, f[2], now_us);
    o->x[L2_BATT] = fresh(v->batt, f[4], now_us);
    o->x[L2_EGT] = fresh(v->egt1, f[3], now_us);
    bool ef = f[4] && now_us - f[4] < L2_STALE_US;
    for (int i = 0; i < L2_N; i++) o->err[i] = false;
    o->err[L2_BOOST]  = ef && (v->err & EMU_ERR_MAP);
    o->err[L2_LAMBDA] = ef && (v->err & EMU_ERR_WBO);
    o->err[L2_CLT]    = ef && (v->err & EMU_ERR_CLT);
    o->err[L2_IAT]    = ef && (v->err & EMU_ERR_IAT);
    o->err[L2_EGT]    = ef && (v->err & EMU_ERR_EGT1);
    o->fan = f[6] && now_us - f[6] < L2_STALE_US
           ? (v->outflags[3] & EMU_OUT4_FAN) != 0 : -1;
    o->link = false;
    for (int i = 0; i < EMU_FRAMES; i++) {
        if (f[i] && now_us - f[i] < L2_STALE_US) o->link = true;
    }
}

/* ----------------------------------------------------------- channels */
typedef struct {
    const char *name, *unit, *fmt;
    float lo, hi;                 /* the bar */
    float lo_alarm, lo_warn;      /* NAN: none */
    float hi_warn, hi_alarm;
    bool  lo_cold;                /* under lo_warn is cold (blue), not bad */
} chan_t;

static const chan_t CH[L2_N] = {
    [L2_BOOST]  = { "BOOST", "bar", "%.2f", -1.0f, 2.5f,
                    NAN, NAN, 1.8f, 2.2f, false },
    /* AFR 10.5 / 11.2 .. 15.2 / 16.0, as on the round EMU gauge */
    [L2_LAMBDA] = { "LAMBDA", "", "%.2f", 0.70f, 1.30f,
                    0.714f, 0.762f, 1.034f, 1.088f, false },
    [L2_IAT]    = { "IAT", "\xC2\xB0" "C", "%.0f", -20, 80,
                    NAN, NAN, 50, 65, false },
    [L2_CLT]    = { "CLT", "\xC2\xB0" "C", "%.0f", 20, 120,
                    NAN, 60, 100, 108, true },
    [L2_TPS]    = { "THROTTLE", "%", "%.0f", 0, 100,
                    NAN, NAN, NAN, NAN, false },
    [L2_RPM]    = { "RPM", "", "%.0f", 0, 8000,
                    NAN, NAN, 6500, 7200, false },
    [L2_OILT]   = { "OIL", "\xC2\xB0" "C", "%.0f", 40, 150,
                    NAN, 60, 120, 135, true },
    [L2_OILP]   = { "OIL", "bar", "%.1f", 0, 8,
                    0.5f, 1.0f, NAN, NAN, false },
    [L2_BATT]   = { "BATT", "V", "%.1f", 10, 16,
                    11.5f, 12.0f, 14.8f, 15.2f, false },
    [L2_EGT]    = { "EGT", "\xC2\xB0" "C", "%.0f", 0, 1000,
                    NAN, NAN, 900, 950, false },
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
    lv_obj_set_size(o, L2_W, L2_H);
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

/* ---------------------------------------------------- BOOST: LED bar */
#define SEG_N   35              /* -1.0 .. 2.5 bar, 0.1 a segment */
#define SEG_W   5
#define SEG_GAP 1
#define SEG_X   15
#define SEG_ZERO 10             /* the segment that starts at 0 bar */

static struct {
    card_t c;
    lv_obj_t *seg[SEG_N];
    lv_color_t seg_col[SEG_N];
    lv_obj_t *val, *max, *peak;
    float max_v;
    int lit;                    /* segments shown last time, -99: none */
} s_boost;

static lv_color_t seg_color(int i)
{
    float x = -1.0f + (i + 0.5f) * 0.1f;
    if (x < 0) return C_YEL_DIM;             /* vacuum */
    return level_color(level(&CH[L2_BOOST], x));
}

static void boost_create(lv_obj_t *p)
{
    const lv_coord_t Y = 26, H = 118;
    s_boost.c = card(p, 6, Y, 228, H);
    lv_obj_t *c = s_boost.c.card;
    title(c, L2_BOOST, 12, 6, &l2_txt_20);
    s_boost.max = text(c, &l2_txt_15, C_SMALL, "");
    lv_obj_set_pos(s_boost.max, 12, 30);
    s_boost.val = text_r(c, &l2_num_64, C_TEXT, 220, 0, 150);
    for (int i = 0; i < SEG_N; i++) {
        lv_coord_t x = SEG_X - 6 + i * (SEG_W + SEG_GAP);
        s_boost.seg_col[i] = seg_color(i);
        s_boost.seg[i] = rect(c, x, 72, SEG_W, 18, C_SEG_OFF);
        lv_obj_set_style_radius(s_boost.seg[i], 1, 0);
    }
    /* the peak: a white segment cap */
    s_boost.peak = rect(c, 0, 69, SEG_W, 24, C_TEXT);
    lv_obj_set_style_bg_opa(s_boost.peak, LV_OPA_80, 0);
    lv_obj_add_flag(s_boost.peak, LV_OBJ_FLAG_HIDDEN);
    static const char *const LBL[] = { "-1", "0", "1", "2" };
    for (int k = 0; k < 4; k++) {
        lv_coord_t x = SEG_X - 6 + (k * 10) * (SEG_W + SEG_GAP);
        rect(c, x, 92, 1, 4, C_SMALL);
        lv_obj_t *l = text(c, &l2_txt_15, C_SMALL, LBL[k]);
        lv_obj_set_pos(l, x, 96);
    }
    s_boost.max_v = NAN;
    s_boost.lit = -99;
}

static void boost_update(const l2_view_t *v, bool blink_on)
{
    float x = v->x[L2_BOOST];
    char buf[24];
    if (v->err[L2_BOOST]) {
        lv_label_set_text(s_boost.val, "");
        lv_label_set_text(s_boost.max, "SENSOR ERR");
        lv_obj_set_style_text_color(s_boost.max, C_WARN, 0);
        x = NAN;
    } else if (isnan(x)) {
        lv_label_set_text(s_boost.val, "--");
    } else {
        fmt(buf, sizeof buf, CH[L2_BOOST].fmt, x);
        lv_label_set_text(s_boost.val, buf);
    }
    level_t lv = isnan(x) ? L_OK : level(&CH[L2_BOOST], x);
    lv_obj_set_style_text_color(s_boost.val,
        lv == L_ALARM && !blink_on ? C_RED : C_TEXT, 0);
    lv_obj_set_style_bg_color(s_boost.c.edge, level_color(lv), 0);
    if (!v->err[L2_BOOST]) {
        if (isnan(s_boost.max_v)) {
            lv_label_set_text(s_boost.max, "");
        } else {
            fmt(buf, sizeof buf, "MAX %.2f", s_boost.max_v);
            lv_label_set_text(s_boost.max, buf);
        }
        lv_obj_set_style_text_color(s_boost.max, C_SMALL, 0);
    }

    /* segments: from the zero one to the value, either way */
    int lit = -99;
    if (!isnan(x)) {
        lit = (int)floorf(x * 10.0f + 1e-3f);    /* 0.1 bar steps */
        if (lit < -SEG_ZERO) lit = -SEG_ZERO;
        if (lit > SEG_N - SEG_ZERO) lit = SEG_N - SEG_ZERO;
    }
    if (lit != s_boost.lit) {
        for (int i = 0; i < SEG_N; i++) {
            int k = i - SEG_ZERO;                 /* -10 .. 24 */
            bool on = lit != -99 && (lit >= 0 ? (k >= 0 && k < lit)
                                              : (k < 0 && k >= lit));
            lv_obj_set_style_bg_color(s_boost.seg[i],
                on ? s_boost.seg_col[i] : C_SEG_OFF, 0);
        }
        s_boost.lit = lit;
    }
    if (!isnan(s_boost.max_v) && s_boost.max_v >= 0.1f) {
        int pi = SEG_ZERO + (int)floorf(s_boost.max_v * 10.0f + 1e-3f) - 1;
        if (pi > SEG_N - 1) pi = SEG_N - 1;
        lv_obj_set_x(s_boost.peak, SEG_X - 6 + pi * (SEG_W + SEG_GAP));
        lv_obj_clear_flag(s_boost.peak, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_boost.peak, LV_OBJ_FLAG_HIDDEN);
    }
}

/* -------------------------------------------- LAMBDA: needle on a scale */
#define LAM_X 12
#define LAM_W 204               /* 0.70 .. 1.30 */
#define LAM_Y 68

static struct {
    card_t c;
    lv_obj_t *val, *afr, *needle;
} s_lam;

static lv_coord_t lam_x(float l)
{
    return LAM_X + (lv_coord_t)lroundf(frac(&CH[L2_LAMBDA], l) * LAM_W);
}

static void lam_zone(lv_obj_t *c, float a, float b, lv_color_t col)
{
    lv_coord_t x0 = lam_x(a), x1 = lam_x(b);
    rect(c, x0, LAM_Y, x1 - x0, 6, col);
}

static void lambda_create(lv_obj_t *p)
{
    const lv_coord_t Y = 148, H = 102;
    s_lam.c = card(p, 6, Y, 228, H);
    lv_obj_t *c = s_lam.c.card;
    title(c, L2_LAMBDA, 12, 6, &l2_txt_20);
    s_lam.afr = text(c, &l2_txt_15, C_SMALL, "");
    lv_obj_set_pos(s_lam.afr, 12, 30);
    s_lam.val = text_r(c, &l2_num_64, C_TEXT, 220, 0, 150);
    const chan_t *L = &CH[L2_LAMBDA];
    lam_zone(c, L->lo, L->lo_alarm, C_RED);
    lam_zone(c, L->lo_alarm, L->lo_warn, C_WARN);
    lam_zone(c, L->lo_warn, L->hi_warn, C_YEL_DIM);
    lam_zone(c, L->hi_warn, L->hi_alarm, C_WARN);
    lam_zone(c, L->hi_alarm, L->hi, C_RED);
    static const float T[] = { 0.70f, 0.80f, 0.90f, 1.00f, 1.10f, 1.20f, 1.30f };
    for (int k = 0; k < 7; k++) {
        lv_coord_t x = lam_x(T[k]);
        bool one = k == 3;
        rect(c, x - (one ? 1 : 0), LAM_Y + 7, one ? 2 : 1, one ? 7 : 4,
             one ? C_TEXT : C_SMALL);
        if (k % 3 == 0) {                         /* 0.7, 1.0, 1.3 */
            char b[8];
            snprintf(b, sizeof b, "%.1f", T[k]);
            lv_obj_t *l = text(c, &l2_txt_15, one ? C_TEXT : C_SMALL, b);
            lv_obj_set_pos(l, x - (k == 0 ? 0 : k == 6 ? 18 : 9), LAM_Y + 13);
        }
    }
    s_lam.needle = rect(c, 0, LAM_Y - 6, 4, 18, C_TEXT);
    lv_obj_set_style_border_color(s_lam.needle, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(s_lam.needle, 1, 0);
    lv_obj_add_flag(s_lam.needle, LV_OBJ_FLAG_HIDDEN);
}

static void lambda_update(const l2_view_t *v, bool blink_on)
{
    float x = v->x[L2_LAMBDA];
    char buf[24];
    level_t lv = L_OK;
    if (v->err[L2_LAMBDA]) {
        lv_label_set_text(s_lam.val, "");
        lv_label_set_text(s_lam.afr, "SENSOR ERR");
        lv_obj_set_style_text_color(s_lam.afr, C_WARN, 0);
        lv_obj_add_flag(s_lam.needle, LV_OBJ_FLAG_HIDDEN);
    } else if (isnan(x)) {
        lv_label_set_text(s_lam.val, "--");
        lv_label_set_text(s_lam.afr, "");
        lv_obj_add_flag(s_lam.needle, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv = level(&CH[L2_LAMBDA], x);
        fmt(buf, sizeof buf, "%.2f", x);
        lv_label_set_text(s_lam.val, buf);
        fmt(buf, sizeof buf, "AFR %.1f", x * L2_STOICH);
        lv_label_set_text(s_lam.afr, buf);
        lv_obj_set_style_text_color(s_lam.afr, C_SMALL, 0);
        lv_obj_set_x(s_lam.needle, lam_x(x) - 2);
        lv_obj_set_style_bg_color(s_lam.needle,
            lv == L_OK ? C_TEXT : level_color(lv), 0);
        lv_obj_clear_flag(s_lam.needle, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_text_color(s_lam.val,
        lv == L_ALARM && !blink_on ? C_RED : C_TEXT, 0);
    lv_obj_set_style_bg_color(s_lam.c.edge, level_color(lv), 0);
}

/* ------------------------------------------------- tiles: value + bar */
typedef struct {
    int ch;
    card_t c;
    lv_obj_t *val, *bar, *track;
    const lv_font_t *num;
    lv_coord_t bar_w;
} tile_t;

static tile_t s_tile[L2_N];
static int s_tiles;

static void tile_create(lv_obj_t *p, int ch, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h, const lv_font_t *num)
{
    tile_t *t = &s_tile[s_tiles++];
    t->ch = ch;
    t->c = card(p, x, y, w, h);
    lv_obj_t *c = t->c.card;
    title(c, ch, 12, 4, &l2_txt_15);
    /* digits sit 4 px over the bar */
    t->num = num;
    t->val = text_r(c, num, C_TEXT, w - 8,
                    h - 12 - lv_font_get_line_height(num) + num->base_line, w - 16);
    t->bar_w = w - 20;
    t->track = rect(c, 12, h - 8, t->bar_w, 3, C_SEG_OFF);
    t->bar = rect(c, 12, h - 8, 0, 3, C_YEL);
}

static void tile_update(tile_t *t, const l2_view_t *v, bool blink_on)
{
    const chan_t *c = &CH[t->ch];
    float x = v->x[t->ch];
    char buf[24];
    level_t lv = L_OK;
    if (v->err[t->ch]) {
        lv_obj_set_style_text_font(t->val, &l2_txt_20, 0);   /* digits only in num */
        lv_label_set_text(t->val, "SENSOR ERR");
        lv_obj_set_style_text_color(t->val, C_WARN, 0);
        lv_obj_set_width(t->bar, 0);
        lv_obj_set_style_bg_color(t->c.edge, C_WARN, 0);
        return;
    }
    lv_obj_set_style_text_font(t->val, t->num, 0);
    if (isnan(x)) {
        lv_label_set_text(t->val, "--");
        lv_obj_set_width(t->bar, 0);
    } else {
        lv = level(c, x);
        /* oil pressure is low with the engine off: no alarm then */
        if (t->ch == L2_OILP && !(v->x[L2_RPM] > 400)) lv = L_OK;
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
static lv_obj_t *s_page[L2_PAGES];
static lv_obj_t *s_link, *s_fan, *s_fan_lbl, *s_dots[L2_PAGES];
static int s_cur;
static uint32_t s_frames;

static void gesture_cb(lv_event_t *e)
{
    (void)e;
    lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (d == LV_DIR_LEFT) ui_l2_page((s_cur + 1) % L2_PAGES);
    if (d == LV_DIR_RIGHT) ui_l2_page((s_cur + L2_PAGES - 1) % L2_PAGES);
}

void ui_l2_create(void)
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
    for (int i = 0; i < L2_PAGES; i++) {
        s_dots[i] = rect(scr, L2_W / 2 - 7 + i * 10, 19, 4, 4, C_DOT);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, 0);
    }
    s_fan = rect(scr, 196, 3, 38, 17, C_DOT);
    lv_obj_set_style_bg_opa(s_fan, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(s_fan, 8, 0);
    lv_obj_set_style_border_width(s_fan, 1, 0);
    lv_obj_set_style_border_color(s_fan, C_DOT, 0);
    s_fan_lbl = text(s_fan, &l2_txt_15, C_DOT, "FAN");
    lv_obj_center(s_fan_lbl);

    /* page 1: boost, lambda, IAT, CLT */
    s_page[0] = page_obj(scr);
    boost_create(s_page[0]);
    lambda_create(s_page[0]);
    tile_create(s_page[0], L2_IAT, 6, 254, 111, 56, &l2_num_36);
    tile_create(s_page[0], L2_CLT, 123, 254, 111, 56, &l2_num_36);

    /* page 2: six tiles */
    s_page[1] = page_obj(scr);
    static const int P2[] = { L2_TPS, L2_RPM, L2_OILT, L2_OILP, L2_BATT, L2_EGT };
    for (int i = 0; i < 6; i++) {
        tile_create(s_page[1], P2[i], i % 2 ? 123 : 6, 26 + (i / 2) * 96,
                    111, 92, &l2_num_46);
    }

    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);
    ui_l2_page(0);
    lv_scr_load(scr);
}

void ui_l2_page(int page)
{
    if (page < 0 || page >= L2_PAGES) page = 0;
    s_cur = page;
    for (int i = 0; i < L2_PAGES; i++) {
        if (i == page) lv_obj_clear_flag(s_page[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_page[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_dots[i], i == page ? C_YEL : C_DOT, 0);
    }
}

void ui_l2_update(const l2_view_t *v)
{
    bool blink_on = (s_frames++ / 8) % 2;        /* ~2 Hz at 30 Hz */
    lv_obj_set_style_bg_color(s_link, v->link ? C_LED_ON : C_RED, 0);
    bool fan = v->fan == 1;
    lv_obj_set_style_bg_opa(s_fan, fan ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(s_fan, C_COLD, 0);
    lv_obj_set_style_border_color(s_fan, fan ? C_COLD : C_DOT, 0);
    lv_obj_set_style_text_color(s_fan_lbl, fan ? C_TEXT : C_DOT, 0);

    float b = v->x[L2_BOOST];             /* the max, on either page */
    if (!v->err[L2_BOOST] && !isnan(b) && (isnan(s_boost.max_v) || b > s_boost.max_v)) {
        s_boost.max_v = b;
    }
    if (s_cur == 0) {
        boost_update(v, blink_on);
        lambda_update(v, blink_on);
    }
    for (int i = 0; i < s_tiles; i++) {
        if (lv_obj_get_parent(s_tile[i].c.card) == s_page[s_cur]) {
            tile_update(&s_tile[i], v, blink_on);
        }
    }
}
