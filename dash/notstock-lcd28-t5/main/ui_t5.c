/* The 2.8" T5.1 gauge's screens, see ui_t5.h. The look is the round
 * gauge's NOTSTOCK one (../notstock-round/main/ui_look_notstock.c,
 * ui_round_multi.c, ui_round_dpf.c, ui_round_diag.c), laid out for a
 * landscape 320 x 240: its faces come from its own generator at 236 px
 * (tools/gen_faces28.py), its fonts (Orbitron) at the matching sizes. */
#include "ui_t5.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dtc_text.h"
#include "faces28.h"

LV_FONT_DECLARE(t5_v54);
LV_FONT_DECLARE(t5_v40);
LV_FONT_DECLARE(t5_v20);
LV_FONT_DECLARE(t5_v16);
LV_FONT_DECLARE(t5_code_36);
LV_FONT_DECLARE(t5_t14);
LV_FONT_DECLARE(t5_t11);

#define C_W      lv_color_hex(0xFFFFFF)
#define C_GREY   lv_color_hex(0x8A9096)
#define C_DIM    lv_color_hex(0x5A5F66)
#define C_DOT    lv_color_hex(0x3A3F45)
#define C_RED    lv_color_hex(0xFF3030)
#define C_ZONE   lv_color_hex(0x5A1414)
#define C_REGEN  lv_color_hex(0xFF9A1F)
#define C_OK     lv_color_hex(0x3CD070)
#define C_TRACK  lv_color_hex(0x1A1D21)
#define C_PANEL  lv_color_hex(0x16191C)
#define C_EDGE   lv_color_hex(0x2A2D31)
#define C_TITLE  lv_color_hex(0x969CA4)

#define BIG_C    (F28_SIZE / 2 + 2)          /* the big dial's centre, x = y */
#define REGEN_ON   400.0f    /* filter surface hotter than this: regenerating */
#define REGEN_OFF  350.0f    /* ... and over when it is under this again */
#define SOOT_MAX   40.0f     /* the DPF dial's scale, g */
#define NIGHT_PCT  25
#define BACK_MS    5000      /* after a regeneration: back to the gauges */

/* the text in the language picked; the fonts carry the Czech capitals */
#define TR(cs, en) (g_t5_set.lang == T5_LANG_EN ? (en) : (cs))

/* ------------------------------------------------------------ the values */
typedef struct {
    int x;                      /* index into t5_view_t.x */
    int face;                   /* F28_*, -1: not on the big dial */
    float lo, hi;               /* the scale */
    int dec;
    float lim_lo, lim_hi, step, lim_def;
    const char *cs, *en, *unit;
} chan_t;

static const chan_t CH[T5_CH_N] = {
    [T5_CH_BOOST] = { T5_BOOST, F28_BOOST, 0, 2.5f, 2, 0.5f, 2.5f, 0.05f, 1.9f,
                      "BOOST", "BOOST", "bar" },
    [T5_CH_EGT]   = { T5_EGT, F28_EGT, 0, 1000, 0, 400, 1000, 10, 800,
                      "EGT", "EGT", "°C" },
    [T5_CH_RPM]   = { T5_RPM, F28_RPM, 0, 5000, 0, 2500, 5000, 100, 4500,
                      "OTÁČKY", "RPM", "" },
    [T5_CH_IAT]   = { T5_IAT, F28_IAT, 0, 100, 0, 20, 100, 1, 60,
                      "SÁNÍ", "INTAKE", "°C" },
    [T5_CH_OIL]   = { T5_OILT, F28_OIL, 40, 160, 0, 80, 160, 1, 125,
                      "OLEJ", "OIL", "°C" },
    [T5_CH_CLT]   = { T5_CLT, F28_CLT, 40, 140, 0, 80, 130, 1, 105,
                      "VODA", "WATER", "°C" },
    [T5_CH_SOOT]  = { T5_SOOT, -1, 0, SOOT_MAX, 1, 10, 40, 0.5f, 24,
                      "SAZE DPF", "DPF SOOT", "g" },
    [T5_CH_SPEED] = { T5_SPEED, -1, 0, 200, 0, 50, 200, 5, 200,
                      "RYCHLOST", "SPEED", "km/h" },
};

static const char *ch_name(int c)
{
    return TR(CH[c].cs, CH[c].en);
}

static const char *ch_unit(int c)
{
    return c == T5_CH_RPM ? TR("ot/min", "rpm") : CH[c].unit;
}

/* ------------------------------------------------------------ settings */
t5_settings_t g_t5_set;

void t5_settings_defaults(void)
{
    memset(&g_t5_set, 0, sizeof g_t5_set);
    g_t5_set.ver = T5_SET_VER;
    g_t5_set.lang = T5_LANG_CS;
    g_t5_set.sound = T5_SND_BEEP;
    g_t5_set.volume = 2;
    g_t5_set.big = 1u << T5_CH_BOOST | 1u << T5_CH_EGT | 1u << T5_CH_RPM
                 | 1u << T5_CH_IAT;
    g_t5_set.small[0] = T5_CH_OIL;
    g_t5_set.small[1] = T5_CH_CLT;
    g_t5_set.small[2] = T5_CH_EGT;
    for (int i = 0; i < T5_CH_N; i++) g_t5_set.limit[i] = CH[i].lim_def;
}

bool t5_settings_valid(const t5_settings_t *s)
{
    if (s->ver != T5_SET_VER || s->lang > T5_LANG_EN || s->sound > T5_SND_BEEP
        || s->volume < 1 || s->volume > 3) return false;
    for (int i = 0; i < T5_SMALL; i++) if (s->small[i] >= T5_CH_N) return false;
    for (int i = 0; i < T5_CH_N; i++) {
        if (!(s->limit[i] >= CH[i].lim_lo && s->limit[i] <= CH[i].lim_hi)) return false;
        if ((s->big >> i & 1) && CH[i].face < 0) return false;
    }
    return true;
}

/* ------------------------------------------------------------ helpers */
static float frac01(float v, float lo, float hi)
{
    float f = (v - lo) / (hi - lo);
    return isnan(f) ? 0 : f < 0 ? 0 : f > 1 ? 1 : f;
}

static void fmt(char *buf, size_t n, int dec, float x)
{
    if (isnan(x)) {
        snprintf(buf, n, "--");
        return;
    }
    snprintf(buf, n, "%.*f", dec, x);
    if (buf[0] == '-' && atof(buf) == 0) snprintf(buf, n, "%.*f", dec, 0.0);
}

static int digits(const char *s)
{
    int n = 0;
    for (; *s; s++) n += *s >= '0' && *s <= '9';
    return n;
}

static lv_obj_t *screen_new(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    return s;
}

/* a label centred on cx */
static lv_obj_t *label_c(lv_obj_t *par, const lv_font_t *f, lv_color_t c,
                         lv_coord_t cx, lv_coord_t y, lv_coord_t w)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(l, w);
    lv_obj_set_pos(l, cx - w / 2, y);
    lv_label_set_text(l, "");
    return l;
}

static lv_obj_t *box(lv_obj_t *par, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                     lv_coord_t h, lv_color_t c)
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

/* an arc on a circle: band (the track or a zone: MAIN part) or value
 * (the INDICATOR, 0..1000 of the sweep) */
enum { BAND, VALUE };
static lv_obj_t *arc_mk(lv_obj_t *par, int cx, int cy, int r, int w,
                        int start, int sweep, int kind, lv_color_t c,
                        lv_opa_t opa, bool rounded)
{
    lv_obj_t *a = lv_arc_create(par);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, 2 * r + w, 2 * r + w);
    lv_obj_set_pos(a, cx - r - w / 2, cy - r - w / 2);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(a, start);
    lv_arc_set_bg_angles(a, 0, sweep);
    lv_arc_set_range(a, 0, 1000);
    lv_arc_set_value(a, 0);
    lv_obj_set_style_arc_width(a, w, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, w, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, rounded, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, rounded, LV_PART_INDICATOR);
    if (kind == BAND) {
        lv_obj_set_style_arc_color(a, c, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(a, opa, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    } else {
        lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_color(a, c, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(a, opa, LV_PART_INDICATOR);
    }
    return a;
}

static void zone_set(lv_obj_t *z, int sweep, float frac)
{
    if (!(frac < 1)) {
        lv_obj_add_flag(z, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (frac < 0) frac = 0;
    lv_obj_clear_flag(z, LV_OBJ_FLAG_HIDDEN);
    lv_arc_set_bg_angles(z, (uint16_t)lroundf(sweep * frac), sweep);
}

/* the value arc with its glow, as the round gauge's (width added, opacity) */
#define N_GLOW 3
static const struct { int extra; lv_opa_t opa; } GLOW[N_GLOW] = {
    { 15, 18 }, { 7, 45 }, { 0, LV_OPA_COVER },
};

static void glow_mk(lv_obj_t *par, int cx, int cy, int r, int w,
                    lv_obj_t *out[N_GLOW])
{
    for (int i = 0; i < N_GLOW; i++) {
        out[i] = arc_mk(par, cx, cy, r, w + GLOW[i].extra, F28_START, F28_SWEEP,
                        VALUE, C_W, GLOW[i].opa, true);
    }
}

static void glow_set(lv_obj_t *a[N_GLOW], float frac, lv_color_t c)
{
    for (int i = 0; i < N_GLOW; i++) {
        lv_arc_set_value(a[i], (int16_t)lroundf(frac * 1000));
        lv_obj_set_style_arc_color(a[i], c, LV_PART_INDICATOR);
        /* nothing to show: no arc at all, not even the rounded start cap */
        if (frac < 0.001f) lv_obj_add_flag(a[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(a[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/* a pill button: dark panel, rounded, text in the middle */
static lv_obj_t *pill(lv_obj_t *par, lv_coord_t cx, lv_coord_t y, lv_coord_t w,
                      lv_coord_t h, const char *txt, lv_event_cb_t cb,
                      lv_obj_t **lbl_out)
{
    lv_obj_t *b = lv_obj_create(par);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, cx - w / 2, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, h / 2, 0);
    lv_obj_set_style_bg_color(b, C_PANEL, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, C_EDGE, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_bg_color(b, C_EDGE, LV_STATE_PRESSED);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &t5_t14, 0);
    lv_obj_set_style_text_color(l, C_W, 0);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    if (lbl_out) *lbl_out = l;
    return b;
}

/* a pill handing its callback a number */
static lv_obj_t *pill_u(lv_obj_t *par, lv_coord_t cx, lv_coord_t y, lv_coord_t w,
                        lv_coord_t h, lv_event_cb_t cb, intptr_t ud,
                        lv_obj_t **lbl_out)
{
    lv_obj_t *b = pill(par, cx, y, w, h, "", cb, lbl_out);
    lv_obj_remove_event_cb(b, cb);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)ud);
    return b;
}

static void pill_mark(lv_obj_t *lbl, bool on)
{
    lv_obj_t *b = lv_obj_get_parent(lbl);
    lv_obj_set_style_border_color(b, on ? C_W : C_EDGE, 0);
    lv_obj_set_style_border_width(b, on ? 2 : 1, 0);
    lv_obj_set_style_text_color(lbl, on ? C_W : C_DIM, 0);
}

static lv_obj_t *title_mk(lv_obj_t *s, const char *t, lv_coord_t y)
{
    lv_obj_t *l = label_c(s, &t5_t11, C_TITLE, T5_W / 2, y, 300);
    lv_obj_set_style_text_letter_space(l, 3, 0);
    lv_label_set_text(l, t);
    return l;
}

/* --------------------------------------------------------- screens, taps */
static lv_obj_t *s_scr[T5_SCR_N];
static int s_cur = T5_SCR_GAUGES;
static bool s_night;
static uint32_t s_last_tap;
static t5_settings_t s_set_at_open;     /* to save only what changed */
static void build_all(void);

static void show(int scr)
{
    s_cur = scr;
    lv_scr_load(s_scr[scr]);
    /* the finger of a long press is still down: it counts for nothing on
     * the new screen until it is lifted */
    lv_indev_t *in = lv_indev_get_act();
    if (in) lv_indev_wait_release(in);
}

void ui_t5_open(int screen)
{
    if (screen >= 0 && screen < T5_SCR_N) show(screen);
}

static void open_cb(lv_event_t *e)
{
    show((int)(intptr_t)lv_event_get_user_data(e));
}

/* double tap: night and back */
static void tap_cb(lv_event_t *e)
{
    (void)e;
    uint32_t now = lv_tick_get();
    if (now - s_last_tap < 400) {
        s_night = !s_night;
        t5_backlight(s_night ? NIGHT_PCT : 100);
        s_last_tap = 0;
    } else {
        s_last_tap = now;
    }
}

/* ------------------------------------------------------------ the gauges */
static struct {
    lv_obj_t *face, *zone, *arc[N_GLOW], *title, *val, *unit, *peak, *regen;
    lv_obj_t *dot[T5_CH_N];
    int ch;                    /* T5_CH_* on the big dial */
    float shown;               /* smoothed position on the scale */
    float max[T5_N];
    int big_font;              /* -1: not set */
} g;

static struct {
    lv_obj_t *zone, *arc, *val, *name;
    float shown;
} sm[T5_SMALL];

#define SM_X    281             /* the small dials' centre line */
#define SM_R    27              /* ring radius (centre of the band) */
#define SM_W    4
#define SM_Y0   34
#define SM_DY   80

static uint8_t big_mask(void)
{
    uint8_t m = g_t5_set.big;
    return m ? m : 1u << T5_CH_BOOST;
}

static void big_set(int c)
{
    uint8_t m = big_mask();
    if (c < 0 || c >= T5_CH_N || !(m >> c & 1)) {
        for (c = 0; !(m >> c & 1); c++) {}
    }
    g.ch = c;
    const chan_t *h = &CH[c];
    lv_img_set_src(g.face, f28_face[h->face]);
    lv_label_set_text(g.title, ch_name(c));
    lv_label_set_text(g.unit, ch_unit(c));
    zone_set(g.zone, F28_SWEEP, (g_t5_set.limit[c] - h->lo) / (h->hi - h->lo));
    g.shown = 0;
    g.big_font = -1;
    /* page dots: one per value shown, in order */
    int n = 0, at = 0;
    for (int i = 0; i < T5_CH_N; i++) {
        if (m >> i & 1) {
            if (i == c) at = n;
            n++;
        }
    }
    for (int i = 0; i < T5_CH_N; i++) {
        if (i < n && n > 1) {
            lv_obj_clear_flag(g.dot[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_x(g.dot[i], BIG_C - n * 5 + i * 10 + 2);
            lv_obj_set_style_bg_color(g.dot[i], i == at ? C_W : C_DOT, 0);
        } else {
            lv_obj_add_flag(g.dot[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_t5_page(int c)
{
    big_set(c);
}

static void gesture_cb(lv_event_t *e)
{
    (void)e;
    lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
    uint8_t m = big_mask();
    int c = g.ch;
    if (d != LV_DIR_LEFT && d != LV_DIR_RIGHT) return;
    for (int k = 0; k < T5_CH_N; k++) {
        c = (c + (d == LV_DIR_LEFT ? 1 : T5_CH_N - 1)) % T5_CH_N;
        if (m >> c & 1) break;
    }
    big_set(c);
}

static void small_set(int i)
{
    int c = g_t5_set.small[i];
    const chan_t *h = &CH[c];
    lv_label_set_text(sm[i].name, ch_name(c));
    zone_set(sm[i].zone, F28_SWEEP, (g_t5_set.limit[c] - h->lo) / (h->hi - h->lo));
    sm[i].shown = 0;
}

static void gauges_create(void)
{
    lv_obj_t *s = s_scr[T5_SCR_GAUGES] = screen_new();
    int c = BIG_C;
    g.face = lv_img_create(s);
    lv_obj_set_pos(g.face, c - F28_SIZE / 2, c - F28_SIZE / 2);
    g.zone = arc_mk(s, c, c, F28_ARC_R, F28_GROOVE_W, F28_START, F28_SWEEP,
                    BAND, C_ZONE, LV_OPA_COVER, true);
    glow_mk(s, c, c, F28_ARC_R, F28_ARC_W, g.arc);
    g.title = label_c(s, &t5_t11, C_TITLE, c, c - F28_TITLE_Y - 6, 140);
    lv_obj_set_style_text_letter_space(g.title, 3, 0);
    g.val = label_c(s, &t5_v54, C_W, c, 0, 190);
    g.unit = label_c(s, &t5_t14, C_GREY, c, c + 30, 120);
    g.peak = label_c(s, &t5_t11, C_DIM, c, c + 50, 150);
    lv_obj_set_style_text_letter_space(g.peak, 1, 0);
    for (int i = 0; i < T5_CH_N; i++) {
        g.dot[i] = box(s, 0, c + 88, 5, 5, C_DOT);
        lv_obj_set_style_radius(g.dot[i], LV_RADIUS_CIRCLE, 0);
    }
    g.regen = label_c(s, &t5_t11, C_REGEN, c, c + 100, 170);
    lv_obj_set_style_text_letter_space(g.regen, 1, 0);

    for (int i = 0; i < T5_SMALL; i++) {
        int cy = SM_Y0 + i * SM_DY;
        arc_mk(s, SM_X, cy, SM_R, SM_W, F28_START, F28_SWEEP, BAND, C_TRACK,
               LV_OPA_COVER, false);
        sm[i].zone = arc_mk(s, SM_X, cy, SM_R, SM_W, F28_START, F28_SWEEP, BAND,
                            C_ZONE, LV_OPA_COVER, false);
        sm[i].arc = arc_mk(s, SM_X, cy, SM_R, SM_W, F28_START, F28_SWEEP, VALUE,
                           C_W, LV_OPA_COVER, false);
        sm[i].val = label_c(s, &t5_v16, C_W, SM_X, cy - 9, 54);
        sm[i].name = label_c(s, &t5_t11, C_GREY, SM_X, cy + 27, 76);
        lv_obj_set_style_text_letter_space(sm[i].name, 1, 0);
        small_set(i);
    }

    lv_obj_add_event_cb(s, gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(s, open_cb, LV_EVENT_LONG_PRESSED, (void *)T5_SCR_MENU);
    lv_obj_add_event_cb(s, tap_cb, LV_EVENT_SHORT_CLICKED, NULL);
    big_set(g.ch);
}

static bool s_regen;

static void gauges_update(const t5_view_t *v)
{
    for (int i = 0; i < T5_N; i++) {
        float x = v->x[i];
        if (!isnan(x) && (isnan(g.max[i]) || x > g.max[i])) g.max[i] = x;
    }
    const chan_t *h = &CH[g.ch];
    float x = v->x[h->x];
    bool warn = !isnan(x) && x >= g_t5_set.limit[g.ch];
    g.shown += (frac01(x, h->lo, h->hi) - g.shown) * 0.35f;
    glow_set(g.arc, g.shown, warn ? C_RED : C_W);

    char buf[32];
    fmt(buf, sizeof buf, h->dec, x);
    int big = digits(buf) <= 3;
    if (big != g.big_font) {
        g.big_font = big;
        const lv_font_t *f = big ? &t5_v54 : &t5_v40;
        lv_obj_set_style_text_font(g.val, f, 0);
        lv_obj_set_y(g.val, BIG_C - lv_font_get_line_height(f) / 2 + 2);
    }
    lv_label_set_text(g.val, buf);
    lv_obj_set_style_text_color(g.val, warn ? C_RED : C_W, 0);

    /* the peak line: MAX, or why there is no value */
    if (!v->link) {
        lv_label_set_text(g.peak, v->state == T5_ST_NO_ECU ? TR("ŽÁDNÁ ECU", "NO ECU")
                                : v->state == T5_ST_SCANNING ? TR("HLEDÁM ECU", "SCANNING")
                                : TR("BEZ DAT", "NO DATA"));
        lv_obj_set_style_text_color(g.peak, C_RED, 0);
    } else if (!isnan(g.max[h->x])) {
        char m[24];
        fmt(m, sizeof m, h->dec, g.max[h->x]);
        snprintf(buf, sizeof buf, "MAX %s", m);
        lv_label_set_text(g.peak, buf);
        lv_obj_set_style_text_color(g.peak, C_DIM, 0);
    } else {
        lv_label_set_text(g.peak, "");
    }
    lv_label_set_text(g.regen, s_regen ? TR("REGENERACE DPF", "DPF REGENERATING") : "");

    for (int i = 0; i < T5_SMALL; i++) {
        int c = g_t5_set.small[i];
        float y = v->x[CH[c].x];
        bool w = !isnan(y) && y >= g_t5_set.limit[c];
        sm[i].shown += (frac01(y, CH[c].lo, CH[c].hi) - sm[i].shown) * 0.35f;
        lv_arc_set_value(sm[i].arc, (int16_t)lroundf(sm[i].shown * 1000));
        lv_obj_set_style_arc_color(sm[i].arc, w ? C_RED : C_W, LV_PART_INDICATOR);
        fmt(buf, sizeof buf, c == T5_CH_BOOST || c == T5_CH_SOOT ? 1 : 0, y);
        lv_label_set_text(sm[i].val, buf);
        lv_obj_set_style_text_color(sm[i].val, w ? C_RED : C_W, 0);
    }
}

/* --------------------------------------------------------------- menu */
static void diag_open(void);
static void diag_cb(lv_event_t *e)
{
    (void)e;
    diag_open();
}

static void set_open_cb(lv_event_t *e)
{
    (void)e;
    s_set_at_open = g_t5_set;
    show(T5_SCR_SET);
}

static void menu_create(void)
{
    lv_obj_t *s = s_scr[T5_SCR_MENU] = screen_new();
    title_mk(s, "MENU", 10);
    lv_obj_t *l;
    pill_u(s, T5_W / 2, 30, 230, 42, open_cb, T5_SCR_DPF, &l);
    lv_label_set_text(l, TR("STAV DPF", "DPF STATUS"));
    pill(s, T5_W / 2, 80, 230, 42, TR("DIAGNOSTIKA", "DIAGNOSTICS"), diag_cb, NULL);
    pill(s, T5_W / 2, 130, 230, 42, TR("NASTAVENÍ", "SETTINGS"), set_open_cb, NULL);
    pill_u(s, T5_W / 2, 180, 230, 42, open_cb, T5_SCR_GAUGES, &l);
    lv_label_set_text(l, TR("ZPĚT", "BACK"));
    lv_obj_add_event_cb(s, open_cb, LV_EVENT_LONG_PRESSED, (void *)T5_SCR_GAUGES);
}

/* ---------------------------------------------------------------- DPF */
static struct {
    lv_obj_t *zone, *arc[N_GLOW], *icon, *val, *status, *status_lbl;
    lv_obj_t *row[4];
    float shown;
} dp;

static void dpf_create(void)
{
    lv_obj_t *s = s_scr[T5_SCR_DPF] = screen_new();
    int c = BIG_C;
    lv_obj_t *dial = box(s, c - F28_SIZE / 2, c - F28_SIZE / 2, F28_SIZE,
                         F28_SIZE, lv_color_hex(0x0E1013));
    lv_obj_set_style_radius(dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(dial, C_EDGE, 0);
    lv_obj_set_style_border_width(dial, 2, 0);
    arc_mk(s, c, c, F28_ARC_R, F28_GROOVE_W, F28_START, F28_SWEEP, BAND,
           lv_color_hex(0x0A0B0D), LV_OPA_COVER, true);
    dp.zone = arc_mk(s, c, c, F28_ARC_R, F28_GROOVE_W, F28_START, F28_SWEEP,
                     BAND, C_ZONE, LV_OPA_COVER, true);
    zone_set(dp.zone, F28_SWEEP, g_t5_set.limit[T5_CH_SOOT] / SOOT_MAX);
    glow_mk(s, c, c, F28_ARC_R, F28_ARC_W, dp.arc);

    lv_obj_t *t = label_c(s, &t5_t11, C_TITLE, c, 24, 150);
    lv_obj_set_style_text_letter_space(t, 3, 0);
    lv_label_set_text(t, TR("STAV DPF", "DPF STATUS"));
    dp.icon = lv_img_create(s);
    lv_img_set_src(dp.icon, &f28_icon_dpf);
    lv_obj_set_pos(dp.icon, c - 32, 40);
    lv_obj_set_style_img_recolor_opa(dp.icon, LV_OPA_COVER, 0);
    dp.val = label_c(s, &t5_v40, C_W, c, 104, 170);
    lv_obj_t *u = label_c(s, &t5_t14, C_GREY, c, 150, 60);
    lv_label_set_text(u, "g");
    dp.status = box(s, c - 72, 176, 144, 24, C_PANEL);
    lv_obj_set_style_radius(dp.status, 12, 0);
    dp.status_lbl = lv_label_create(dp.status);
    lv_obj_set_style_text_font(dp.status_lbl, &t5_t11, 0);
    lv_obj_center(dp.status_lbl);

    const char *const NAME[4] = {
        TR("TLAK mbar", "dP mbar"), TR("FILTR °C", "FILTER °C"),
        TR("REG. km", "REGEN km"), TR("MĚŘ. g", "MEAS. g"),
    };
    for (int i = 0; i < 4; i++) {
        int y = 16 + i * 56;
        lv_obj_t *n = label_c(s, &t5_t11, C_GREY, 281, y, 78);
        lv_label_set_text(n, NAME[i]);
        dp.row[i] = label_c(s, &t5_v20, C_W, 281, y + 16, 78);
    }
    lv_obj_add_event_cb(s, open_cb, LV_EVENT_LONG_PRESSED, (void *)T5_SCR_GAUGES);
    lv_obj_add_event_cb(s, tap_cb, LV_EVENT_SHORT_CLICKED, NULL);
}

static void dpf_update(const t5_view_t *v)
{
    float soot = v->x[T5_SOOT], lim = g_t5_set.limit[T5_CH_SOOT];
    bool full = !isnan(soot) && soot >= lim;
    lv_color_t col = s_regen ? C_REGEN : full ? C_RED : C_W;
    dp.shown += (frac01(soot, 0, SOOT_MAX) - dp.shown) * 0.35f;
    glow_set(dp.arc, dp.shown, col);
    lv_obj_set_style_img_recolor(dp.icon, s_regen ? C_REGEN : full ? C_RED
                                                           : lv_color_hex(0xB8BEC6), 0);
    char buf[40];
    fmt(buf, sizeof buf, 1, soot);
    lv_label_set_text(dp.val, buf);

    if (s_regen) {
        lv_label_set_text(dp.status_lbl, TR("REGENERACE", "REGENERATING"));
        lv_obj_set_style_bg_color(dp.status, C_REGEN, 0);
        lv_obj_set_style_text_color(dp.status_lbl, lv_color_hex(0x000000), 0);
    } else if (isnan(soot)) {
        lv_label_set_text(dp.status_lbl, v->link ? TR("NENAČTENO", "NOT READ")
                                                 : TR("BEZ DAT", "NO DATA"));
        lv_obj_set_style_bg_color(dp.status, C_PANEL, 0);
        lv_obj_set_style_text_color(dp.status_lbl, C_GREY, 0);
    } else {
        snprintf(buf, sizeof buf, TR("%.0f %% Z %.0f g", "%.0f %% OF %.0f g"),
                 soot / lim * 100, lim);
        lv_label_set_text(dp.status_lbl, full ? TR("PLNÝ", "FULL") : buf);
        lv_obj_set_style_bg_color(dp.status, full ? C_RED : C_PANEL, 0);
        lv_obj_set_style_text_color(dp.status_lbl, full ? C_W : C_GREY, 0);
    }
    static const int ROW[4] = { T5_DP, T5_DPF_T, T5_KM, T5_SOOT_MEAS };
    static const int DEC[4] = { 0, 0, 0, 1 };
    for (int i = 0; i < 4; i++) {
        fmt(buf, sizeof buf, DEC[i], v->x[ROW[i]]);
        lv_label_set_text(dp.row[i], buf);
        lv_obj_set_style_text_color(dp.row[i], i == 1 && s_regen ? C_REGEN : C_W, 0);
    }
}

/* ---------------------------------------------------------- diagnostics */
static struct {
    lv_obj_t *count, *code, *kind, *text, *dot[8], *clear_lbl;
    int i;                    /* the code shown */
    uint16_t seq;
    uint32_t arm_until;       /* SMAZAT pressed once: confirm until then */
    t5_dtc_t last;
} dg;

static void diag_read_cb(lv_event_t *e)
{
    (void)e;
    t5_dtc_read();
}

static void diag_clear_cb(lv_event_t *e)
{
    (void)e;
    uint32_t now = lv_tick_get();
    if (dg.arm_until && (int32_t)(dg.arm_until - now) > 0) {
        dg.arm_until = 0;
        t5_dtc_clear();
    } else {
        dg.arm_until = now + 3000;
    }
}

static void diag_gesture_cb(lv_event_t *e)
{
    (void)e;
    int n = dg.last.n;
    if (n < 2) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT) dg.i = (dg.i + 1) % n;
    if (dir == LV_DIR_RIGHT) dg.i = (dg.i + n - 1) % n;
}

static void diag_open(void)
{
    show(T5_SCR_DIAG);
    if (dg.last.result == T5_DTC_NOT_READ && !dg.last.busy) t5_dtc_read();
}

static void diag_create(void)
{
    lv_obj_t *s = s_scr[T5_SCR_DIAG] = screen_new();
    title_mk(s, TR("DIAGNOSTIKA", "DIAGNOSTICS"), 10);
    dg.count = label_c(s, &t5_t14, C_REGEN, T5_W / 2, 28, 300);
    dg.code = label_c(s, &t5_code_36, C_REGEN, T5_W / 2, 52, 300);
    dg.kind = label_c(s, &t5_t11, C_GREY, T5_W / 2, 100, 300);
    lv_obj_set_style_text_letter_space(dg.kind, 1, 0);
    dg.text = label_c(s, &t5_t14, C_W, T5_W / 2, 120, 304);
    lv_label_set_long_mode(dg.text, LV_LABEL_LONG_WRAP);
    for (int i = 0; i < 8; i++) {
        dg.dot[i] = box(s, 0, 162, 6, 4, C_DOT);
        lv_obj_set_style_radius(dg.dot[i], 2, 0);
    }
    pill(s, T5_W / 2 - 70, 176, 128, 38, TR("ČÍST", "READ"), diag_read_cb, NULL);
    pill(s, T5_W / 2 + 70, 176, 128, 38, "", diag_clear_cb, &dg.clear_lbl);
    lv_obj_t *h = label_c(s, &t5_t11, C_DIM, T5_W / 2, 222, 300);
    lv_label_set_text(h, TR("PODRŽ: ZPĚT", "LONG PRESS: BACK"));
    lv_obj_add_event_cb(s, open_cb, LV_EVENT_LONG_PRESSED, (void *)T5_SCR_MENU);
    lv_obj_add_event_cb(s, diag_gesture_cb, LV_EVENT_GESTURE, NULL);
}

/* "1 CHYBA", "3 CHYBY", "5 CHYB"; "1 CODE", "3 CODES" */
static const char *codes_word(int n)
{
    if (g_t5_set.lang == T5_LANG_EN) return n == 1 ? "CODE" : "CODES";
    return n == 1 ? "CHYBA" : n >= 2 && n <= 4 ? "CHYBY" : "CHYB";
}

static void diag_update(const t5_view_t *v)
{
    const t5_dtc_t *t = &v->dtc;
    dg.last = *t;
    if (t->seq != dg.seq) {
        dg.seq = t->seq;
        dg.i = 0;
    }
    if (dg.i >= t->n) dg.i = 0;
    char buf[64];
    bool armed = dg.arm_until && (int32_t)(dg.arm_until - lv_tick_get()) > 0;
    if (!armed) dg.arm_until = 0;
    lv_label_set_text(dg.clear_lbl, armed ? TR("POTVRDIT", "CONFIRM")
                                          : TR("SMAZAT", "CLEAR"));
    int cs = g_t5_set.lang != T5_LANG_EN;

    const char *count = "", *code = "", *kind = "", *text = "";
    lv_color_t cc = C_REGEN;
    if (t->busy == T5_DTC_READING) {
        count = TR("ČTU...", "READING...");
    } else if (t->busy == T5_DTC_CLEARING) {
        count = TR("MAŽU...", "CLEARING...");
    } else if (!v->link && t->result == T5_DTC_NOT_READ) {
        count = TR("BEZ SPOJENÍ", "NO LINK");
        cc = C_RED;
    } else switch (t->result) {
    case T5_DTC_NOT_READ:
        count = TR("NENAČTENO", "NOT READ");
        cc = C_GREY;
        break;
    case T5_DTC_NO_ANSWER:
        count = TR("ŽÁDNÁ ODPOVĚĎ", "NO ANSWER");
        cc = C_RED;
        break;
    case T5_DTC_REFUSED:
        count = TR("SMAZÁNÍ ODMÍTNUTO", "CLEARING REFUSED");
        cc = C_RED;
        text = dtc_nrc_text(t->nrc, cs);
        break;
    default:                                    /* READ, CLEARED */
        if (t->n == 0) {
            count = t->result == T5_DTC_CLEARED ? TR("SMAZÁNO", "CLEARED")
                                                : TR("BEZ CHYB", "NO CODES");
            cc = C_OK;
        } else {
            snprintf(buf, sizeof buf, "%d%s %s", t->n, t->more ? "+" : "",
                     codes_word(t->n));
            count = buf;
        }
        break;
    }
    lv_label_set_text(dg.count, count);
    lv_obj_set_style_text_color(dg.count, cc, 0);

    static char name[8], k[40];
    if (!t->busy && t->n > 0) {
        uint16_t c = t->list[dg.i].code;
        static const char L[4] = { 'P', 'C', 'B', 'U' };
        snprintf(name, sizeof name, "%c%04X", L[c >> 14], c & 0x3FFF);
        code = name;
        uint8_t kd = t->list[dg.i].kind;
        snprintf(k, sizeof k, "%s   7E%X",
                 kd & T5_DTC_STORED ? TR("ULOŽENÁ", "STORED")
                                    : TR("ČEKAJÍCÍ", "PENDING"),
                 8 + t->list[dg.i].ecu);
        kind = k;
        text = dtc_text(c, cs);
        if (!text) text = dtc_group(c, cs);
        if (!text) text = "";
    }
    lv_label_set_text(dg.code, code);
    lv_label_set_text(dg.kind, kind);
    lv_label_set_text(dg.text, text ? text : "");

    int nd = t->busy ? 0 : t->n > 8 ? 8 : t->n;
    if (nd < 2) nd = 0;
    for (int i = 0; i < 8; i++) {
        if (i < nd) {
            lv_obj_clear_flag(dg.dot[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_x(dg.dot[i], T5_W / 2 - nd * 5 + i * 10 + 2);
            lv_obj_set_width(dg.dot[i], i == dg.i ? 10 : 5);
            lv_obj_set_style_bg_color(dg.dot[i], i == dg.i ? C_W : C_DOT, 0);
        } else {
            lv_obj_add_flag(dg.dot[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* ------------------------------------------------------------- settings */
static void set_back_cb(lv_event_t *e)
{
    (void)e;
    if (memcmp(&s_set_at_open, &g_t5_set, sizeof g_t5_set)) {
        t5_settings_save();
        s_set_at_open = g_t5_set;
    }
    show(T5_SCR_MENU);
}

static void set_create(void)
{
    lv_obj_t *s = s_scr[T5_SCR_SET] = screen_new();
    title_mk(s, TR("NASTAVENÍ", "SETTINGS"), 6);
    static const int TO[4] = { T5_SCR_LIMITS, T5_SCR_VIEW, T5_SCR_SOUND, T5_SCR_LANG };
    const char *const NAME[4] = {
        TR("LIMITY", "LIMITS"), TR("ZOBRAZENÍ", "DISPLAY"),
        TR("ZVUK", "SOUND"), TR("JAZYK", "LANGUAGE"),
    };
    lv_obj_t *l;
    for (int i = 0; i < 4; i++) {
        pill_u(s, T5_W / 2, 24 + i * 42, 230, 36, open_cb, TO[i], &l);
        lv_label_set_text(l, NAME[i]);
    }
    pill(s, T5_W / 2, 194, 230, 36, TR("ZPĚT", "BACK"), set_back_cb, NULL);
    lv_obj_add_event_cb(s, set_back_cb, LV_EVENT_LONG_PRESSED, NULL);
}

/* LIMITY: one value at a time, < > for the next, - + for its limit */
static struct {
    int c;
    lv_obj_t *name, *val, *unit;
    uint32_t rep_at;           /* - / + held: repeat from then */
} lm;

static void limits_show(void)
{
    char buf[24];
    const chan_t *h = &CH[lm.c];
    lv_label_set_text(lm.name, ch_name(lm.c));
    fmt(buf, sizeof buf, h->dec, g_t5_set.limit[lm.c]);
    lv_label_set_text(lm.val, buf);
    lv_label_set_text(lm.unit, ch_unit(lm.c));
}

static void limits_step(int dir)
{
    const chan_t *h = &CH[lm.c];
    float x = g_t5_set.limit[lm.c] + dir * h->step;
    x = roundf(x / h->step) * h->step;
    if (x < h->lim_lo) x = h->lim_lo;
    if (x > h->lim_hi) x = h->lim_hi;
    g_t5_set.limit[lm.c] = x;
    limits_show();
}

static void limits_pm_cb(lv_event_t *e)
{
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        limits_step(dir);
        lm.rep_at = lv_tick_get() + 450;
    } else if (code == LV_EVENT_PRESSING) {
        if ((int32_t)(lv_tick_get() - lm.rep_at) >= 0) {
            limits_step(dir);
            lm.rep_at = lv_tick_get() + 70;
        }
    }
}

static void limits_lr_cb(lv_event_t *e)
{
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    lm.c = (lm.c + dir + T5_CH_N) % T5_CH_N;
    limits_show();
}

static lv_obj_t *round_btn(lv_obj_t *s, lv_coord_t cx, lv_coord_t cy, int r,
                           const char *t, const lv_font_t *f)
{
    lv_obj_t *b = lv_obj_create(s);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 2 * r, 2 * r);
    lv_obj_set_pos(b, cx - r, cy - r);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, C_PANEL, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, C_EDGE, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(b, C_EDGE, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, C_W, 0);
    lv_label_set_text(l, t);
    lv_obj_center(l);
    return b;
}

static void back_to_set_cb(lv_event_t *e)
{
    (void)e;
    show(T5_SCR_SET);
}

static void limits_create(void)
{
    lv_obj_t *s = s_scr[T5_SCR_LIMITS] = screen_new();
    title_mk(s, TR("LIMITY", "LIMITS"), 6);
    lm.name = label_c(s, &t5_t14, C_W, T5_W / 2, 34, 200);
    lv_obj_set_style_text_letter_space(lm.name, 2, 0);
    lm.val = label_c(s, &t5_v40, C_RED, T5_W / 2, 70, 160);
    lm.unit = label_c(s, &t5_t14, C_GREY, T5_W / 2, 118, 120);
    lv_obj_t *b;
    b = round_btn(s, 36, 92, 24, LV_SYMBOL_LEFT, &lv_font_montserrat_20);
    lv_obj_add_event_cb(b, limits_lr_cb, LV_EVENT_CLICKED, (void *)-1);
    b = round_btn(s, T5_W - 36, 92, 24, LV_SYMBOL_RIGHT, &lv_font_montserrat_20);
    lv_obj_add_event_cb(b, limits_lr_cb, LV_EVENT_CLICKED, (void *)1);
    b = round_btn(s, T5_W / 2 - 60, 170, 28, LV_SYMBOL_MINUS, &lv_font_montserrat_20);
    lv_obj_add_event_cb(b, limits_pm_cb, LV_EVENT_PRESSED, (void *)-1);
    lv_obj_add_event_cb(b, limits_pm_cb, LV_EVENT_PRESSING, (void *)-1);
    b = round_btn(s, T5_W / 2 + 60, 170, 28, LV_SYMBOL_PLUS, &lv_font_montserrat_20);
    lv_obj_add_event_cb(b, limits_pm_cb, LV_EVENT_PRESSED, (void *)1);
    lv_obj_add_event_cb(b, limits_pm_cb, LV_EVENT_PRESSING, (void *)1);
    pill(s, T5_W / 2, 206, 120, 30, TR("ZPĚT", "BACK"), back_to_set_cb, NULL);
    limits_show();
}

/* ZOBRAZENÍ: the big dial's values (on / off each), the small dials'
 * (tap: the next value) */
static struct {
    lv_obj_t *big[F28_N], *small[T5_SMALL];
} vw;

static int big_ch(int k)                /* the k-th value that has a face */
{
    for (int c = 0; c < T5_CH_N; c++) if (CH[c].face >= 0 && k-- == 0) return c;
    return -1;
}

static void view_show(void)
{
    for (int k = 0; k < F28_N; k++) {
        int c = big_ch(k);
        lv_label_set_text(vw.big[k], ch_name(c));
        pill_mark(vw.big[k], g_t5_set.big >> c & 1);
    }
    for (int i = 0; i < T5_SMALL; i++) {
        char buf[32];
        snprintf(buf, sizeof buf, "%d: %s", i + 1, ch_name(g_t5_set.small[i]));
        lv_label_set_text(vw.small[i], buf);
        pill_mark(vw.small[i], true);
    }
}

static void view_big_cb(lv_event_t *e)
{
    int c = big_ch((int)(intptr_t)lv_event_get_user_data(e));
    uint8_t m = g_t5_set.big ^ (1u << c);
    if (m) g_t5_set.big = m;              /* one at least */
    view_show();
    big_set(g.ch);
}

static void view_small_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    g_t5_set.small[i] = (uint8_t)((g_t5_set.small[i] + 1) % T5_CH_N);
    view_show();
    small_set(i);
}

static void view_create(void)
{
    lv_obj_t *s = s_scr[T5_SCR_VIEW] = screen_new();
    title_mk(s, TR("ZOBRAZENÍ", "DISPLAY"), 6);
    lv_obj_t *t = label_c(s, &t5_t11, C_GREY, 80, 22, 150);
    lv_label_set_text(t, TR("VELKÝ BUDÍK", "BIG DIAL"));
    t = label_c(s, &t5_t11, C_GREY, 240, 22, 150);
    lv_label_set_text(t, TR("MALÉ BUDÍKY", "SMALL DIALS"));
    for (int k = 0; k < F28_N; k++) {
        pill_u(s, 80, 38 + k * 28, 144, 25, view_big_cb, k, &vw.big[k]);
    }
    for (int i = 0; i < T5_SMALL; i++) {
        pill_u(s, 240, 38 + i * 28, 144, 25, view_small_cb, i, &vw.small[i]);
    }
    lv_obj_t *h = label_c(s, &t5_t11, C_DIM, 240, 128, 150);
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP);
    lv_label_set_text(h, TR("KLEPNUTÍ: DALŠÍ HODNOTA", "TAP: NEXT VALUE"));
    pill(s, T5_W / 2, 206, 120, 30, TR("ZPĚT", "BACK"), back_to_set_cb, NULL);
    view_show();
}

/* ZVUK: on / off, volume, a test */
static lv_obj_t *s_snd_on, *s_snd_vol;

static void sound_show(void)
{
    lv_label_set_text(s_snd_on, g_t5_set.sound == T5_SND_BEEP
                      ? TR("PÍPÁNÍ: ZAP", "BEEPS: ON") : TR("PÍPÁNÍ: VYP", "BEEPS: OFF"));
    pill_mark(s_snd_on, g_t5_set.sound == T5_SND_BEEP);
    static const char *const CS[3] = { "TICHO", "STŘEDNĚ", "NAHLAS" };
    static const char *const EN[3] = { "LOW", "MEDIUM", "HIGH" };
    char buf[40];
    snprintf(buf, sizeof buf, "%s: %s", TR("HLASITOST", "VOLUME"),
             TR(CS[g_t5_set.volume - 1], EN[g_t5_set.volume - 1]));
    lv_label_set_text(s_snd_vol, buf);
    pill_mark(s_snd_vol, g_t5_set.sound == T5_SND_BEEP);
}

static void sound_cb(lv_event_t *e)
{
    int what = (int)(intptr_t)lv_event_get_user_data(e);
    if (what == 0) g_t5_set.sound = g_t5_set.sound == T5_SND_BEEP ? T5_SND_OFF
                                                                  : T5_SND_BEEP;
    if (what == 1) g_t5_set.volume = (uint8_t)(g_t5_set.volume % 3 + 1);
    sound_show();
    if (what == 1 || what == 2) t5_beep(what == 2 ? 3 : 1);
}

static void sound_create(void)
{
    lv_obj_t *s = s_scr[T5_SCR_SOUND] = screen_new();
    title_mk(s, TR("ZVUK", "SOUND"), 6);
    pill_u(s, T5_W / 2, 30, 250, 40, sound_cb, 0, &s_snd_on);
    pill_u(s, T5_W / 2, 80, 250, 40, sound_cb, 1, &s_snd_vol);
    lv_obj_t *l;
    pill_u(s, T5_W / 2, 130, 250, 40, sound_cb, 2, &l);
    lv_label_set_text(l, TR("ZKOUŠKA", "TEST"));
    lv_obj_t *h = label_c(s, &t5_t11, C_DIM, T5_W / 2, 176, 300);
    lv_label_set_text(h, TR("REGENERACE: 3x NA ZAČÁTKU, 1x NA KONCI",
                            "REGENERATION: 3x AT THE START, 1x AT THE END"));
    pill(s, T5_W / 2, 206, 120, 30, TR("ZPĚT", "BACK"), back_to_set_cb, NULL);
    sound_show();
}

/* JAZYK: everything is rebuilt in the other language */
static void lang_cb(lv_event_t *e)
{
    int l = (int)(intptr_t)lv_event_get_user_data(e);
    if (l == g_t5_set.lang) return;
    g_t5_set.lang = (uint8_t)l;
    build_all();
    show(T5_SCR_LANG);
}

static void lang_create(void)
{
    lv_obj_t *s = s_scr[T5_SCR_LANG] = screen_new();
    title_mk(s, TR("JAZYK", "LANGUAGE"), 6);
    lv_obj_t *l;
    pill_u(s, T5_W / 2, 50, 230, 44, lang_cb, T5_LANG_CS, &l);
    lv_label_set_text(l, "ČEŠTINA");
    pill_mark(l, g_t5_set.lang == T5_LANG_CS);
    pill_u(s, T5_W / 2, 106, 230, 44, lang_cb, T5_LANG_EN, &l);
    lv_label_set_text(l, "ENGLISH");
    pill_mark(l, g_t5_set.lang == T5_LANG_EN);
    pill(s, T5_W / 2, 206, 120, 30, TR("ZPĚT", "BACK"), back_to_set_cb, NULL);
}

/* ---------------------------------------------------------- regeneration */
static int s_before_regen = -1;     /* the screen a regeneration covered */
static uint32_t s_back_at;

static void regen_watch(const t5_view_t *v)
{
    float t = v->x[T5_DPF_T];
    if (!isnan(t)) {                          /* not read: no change */
        if (!s_regen && t >= REGEN_ON) {
            s_regen = true;
            s_back_at = 0;
            t5_beep(3);
            if (s_cur != T5_SCR_DPF) {
                s_before_regen = s_cur == T5_SCR_GAUGES ? T5_SCR_GAUGES : -1;
                show(T5_SCR_DPF);
            }
        } else if (s_regen && t < REGEN_OFF) {
            s_regen = false;
            t5_beep(1);
            if (s_before_regen >= 0) s_back_at = lv_tick_get() + BACK_MS;
        }
    }
    if (s_back_at && (int32_t)(lv_tick_get() - s_back_at) >= 0) {
        s_back_at = 0;
        if (s_cur == T5_SCR_DPF) show(s_before_regen);
        s_before_regen = -1;
    }
}

/* ---------------------------------------------------------------- all */
static void build_all(void)
{
    lv_obj_t *old[T5_SCR_N];
    memcpy(old, s_scr, sizeof old);
    gauges_create();
    menu_create();
    dpf_create();
    diag_create();
    set_create();
    limits_create();
    view_create();
    sound_create();
    lang_create();
    lv_scr_load(s_scr[s_cur]);
    for (int i = 0; i < T5_SCR_N; i++) if (old[i]) lv_obj_del(old[i]);
}

void ui_t5_create(void)
{
    for (int i = 0; i < T5_N; i++) g.max[i] = NAN;
    g.ch = -1;
    s_cur = T5_SCR_GAUGES;
    build_all();
}

void ui_t5_update(const t5_view_t *v)
{
    regen_watch(v);
    switch (s_cur) {
    case T5_SCR_GAUGES: gauges_update(v); break;
    case T5_SCR_DPF:    dpf_update(v); break;
    case T5_SCR_DIAG:   diag_update(v); break;
    default: break;
    }
}
