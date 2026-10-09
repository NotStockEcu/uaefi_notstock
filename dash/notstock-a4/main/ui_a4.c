/* The A4 gauge, see ui_a4.h. The dials are images (tools/gen_faces.py:
 * the face, white ticks, name and unit); live are the numerals, what is
 * red past the limit (ticks drawn over, the band), the needle (an image
 * LVGL turns about its pivot), its hub, the value, the DPF lamp and the
 * page dots. The DPF page has no scale: its icon and digits. */
#include "ui_a4.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "faces/a4_scales.h"

LV_FONT_DECLARE(a4_fis_54);
LV_FONT_DECLARE(a4_txt_22);
LV_FONT_DECLARE(a4_big_96);
LV_FONT_DECLARE(a4_num_52);
LV_FONT_DECLARE(a4_num_40);
LV_IMG_DECLARE(a4_face_oil);
LV_IMG_DECLARE(a4_face_iat);
LV_IMG_DECLARE(a4_face_clt);
LV_IMG_DECLARE(a4_face_egt);
LV_IMG_DECLARE(a4_face_boost);
LV_IMG_DECLARE(a4_face_fuel);
LV_IMG_DECLARE(a4_face_dpf);
LV_IMG_DECLARE(a4_needle);
LV_IMG_DECLARE(a4_cap);
LV_IMG_DECLARE(a4_regen);
LV_IMG_DECLARE(a4_dpf_icon);

#define C          (A4_SIZE / 2)
#define NEEDLE_PX  0                 /* the pivot in a4_needle */
#define NEEDLE_PY  15
/* the text in the dial's free lower right, centred on TX: the small line,
 * the value, then (printed on the face) name and unit */
#define TX         (C + 104)
#define TW         150

#define REGEN_TEMP 400.0f            /* filter hotter: regenerating (as the round gauge) */
#define REGEN_HYST 50.0f
#define SWEEP_MS   1400
#define DOUBLE_MS  400
#define NIGHT_PCT  30

#define C_VAL      lv_color_hex(0xEEEEEC)    /* as the printed scale */
#define C_RED      lv_color_hex(0xF02A26)    /* past the limit */
#define C_SMALL    lv_color_hex(0xA0A0A0)
#define C_AMBER    lv_color_hex(0xFFA800)
#define C_ICON     lv_color_hex(0xD8D8D6)
#define C_DOT      lv_color_hex(0x3A3A3E)
#define C_DOT_ON   lv_color_hex(0xD8D8D8)

/* ------------------------------------------------------------ settings */
a4_settings_t g_a4_set;

const a4_limit_t A4_LIMIT[A4_PAGES] = {
    [A4_OIL]   = { "OLEJ",  "\xC2\xB0" "C", 90,   150,  1,    130,  0 },
    [A4_IAT]   = { "S\xC3\x81N\xC3\x8D", "\xC2\xB0" "C", 20, 80, 1, 60, 0 },
    [A4_CLT]   = { "VODA",  "\xC2\xB0" "C", 90,   130,  1,    105,  0 },
    [A4_EGT]   = { "V\xC3\x9D" "FUK", "\xC2\xB0" "C", 400, 1000, 10, 750, 0 },
    [A4_BOOST] = { "TURBO", "bar",  0.5f, 2.5f, 0.05f, 2.2f, 2 },
    [A4_FUEL]  = { "PALIVO", "\xC2\xB0" "C", 40,  100,  1,    80,   0 },
    [A4_DPF]   = { "DPF",   "g",    5,    40,   0.1f, 22.29f, 2 },
};

void a4_settings_defaults(void)
{
    for (int i = 0; i < A4_PAGES; i++) {
        g_a4_set.order[i] = (uint8_t)i;
        g_a4_set.warn[i] = A4_LIMIT[i].def;
    }
    g_a4_set.hidden = 1u << A4_FUEL;   /* fuel: there to be chosen */
}

void a4_settings_normalize(void)
{
    int k = 0;
    uint8_t o[A4_PAGES];
    for (int i = 0; i < A4_PAGES; i++) {
        if (g_a4_set.order[i] != A4_DPF) o[k++] = g_a4_set.order[i];
    }
    o[k] = A4_DPF;
    for (int i = 0; i < A4_PAGES; i++) g_a4_set.order[i] = o[i];
    g_a4_set.hidden &= (uint8_t)~(1u << A4_DPF);
    if ((g_a4_set.hidden & ((1u << A4_SWIPE) - 1)) == (1u << A4_SWIPE) - 1) {
        g_a4_set.hidden = 0;          /* every dial left out: all back */
    }
}

/* --------------------------------------------------------------- dials */
static const a4_scale_t SC_OIL = A4_SCALE_OIL, SC_IAT = A4_SCALE_IAT,
    SC_CLT = A4_SCALE_CLT, SC_EGT = A4_SCALE_EGT, SC_BOOST = A4_SCALE_BOOST,
    SC_FUEL = A4_SCALE_FUEL;

typedef struct {
    const lv_img_dsc_t *face;
    int   src;                  /* RND_* in rnd_data_t.v, -1: DPF soot, -2: fuel */
    const a4_scale_t *sc;       /* NULL: no scale (DPF) */
    const char *fmt;
} dial_t;

static const dial_t DIAL[A4_PAGES] = {
    [A4_OIL]   = { &a4_face_oil,   RND_OIL,     &SC_OIL,   "%.0f" },
    [A4_IAT]   = { &a4_face_iat,   RND_INTAKE,  &SC_IAT,   "%.0f" },
    [A4_CLT]   = { &a4_face_clt,   RND_WATER,   &SC_CLT,   "%.0f" },
    [A4_EGT]   = { &a4_face_egt,   RND_EXHAUST, &SC_EGT,   "%.0f" },
    [A4_BOOST] = { &a4_face_boost, RND_BOOST,   &SC_BOOST, "%.2f" },
    [A4_FUEL]  = { &a4_face_fuel,  -2,          &SC_FUEL,  "%.0f" },
    [A4_DPF]   = { &a4_face_dpf,   -1,          NULL,      "%.1f" },
};

#define MAX_NUMS   8
#define MAX_TICKS  32
#define MAX_BAND   96                /* chords of the broken part */

static lv_obj_t *s_scr, *s_face, *s_needle, *s_cap, *s_lamp;
static lv_obj_t *s_val, *s_line, *s_dots[A4_PAGES];
static lv_obj_t *s_num[MAX_NUMS];
static lv_obj_t *s_tick[MAX_TICKS];
static lv_point_t s_tick_pt[MAX_TICKS][2];
static lv_obj_t *s_band[MAX_BAND], *s_band_end;
static lv_point_t s_band_pt[MAX_BAND][2];
/* the DPF page */
static lv_obj_t *s_dpf, *s_dpf_icon, *s_dpf_soot, *s_dpf_dp, *s_dpf_temp, *s_dpf_after, *s_dpf_km, *s_dpf_state;

static int s_page;
static int s_before_regen = -1;    /* the dial the regeneration covered */
static float s_angle = NAN;        /* shown, degrees */
static bool s_regen, s_night, s_sweeping;
static uint32_t s_frames, s_sweep_t0, s_last_click;

static float value_of(const rnd_data_t *d, int page)
{
    if (!d->link) return NAN;
    int src = DIAL[page].src;
    return src == -1 ? d->dpf.soot_g : src == -2 ? d->diag.fuel_c : d->v[src];
}

static void fmt(char *buf, size_t n, const char *f, float x)
{
    snprintf(buf, n, f, x);
    if (buf[0] == '-' && atof(buf) == 0) snprintf(buf, n, f, 0.0);
}

static float deg_of(const a4_scale_t *sc, float v)
{
    return A4_START_DEG + A4_SWEEP_DEG * (v - sc->lo) / (sc->hi - sc->lo);
}

/* ---------------------------------------------- what is red past the limit */
/* one piece of the band from a0 to a1 deg: straight chords of at most
 * CHORD_DEG along its middle (a thick polyline in LVGL 8 is jagged at the
 * joints; at this radius a 4 deg chord is off the arc by 0.1 px) */
#define CHORD_DEG 4.0f
static int band_piece(int n, float a0, float a1)
{
    const float r = (A4_R_BAND0 + A4_R_BAND1) / 2.0f;
    int k = (int)ceilf((a1 - a0) / CHORD_DEG);
    if (k < 1) k = 1;
    for (int j = 0; j < k && n < MAX_BAND; j++, n++) {
        float b0 = (a0 + (a1 - a0) * j / k) * 3.14159265f / 180.0f;
        float b1 = (a0 + (a1 - a0) * (j + 1) / k) * 3.14159265f / 180.0f;
        s_band_pt[n][0].x = (lv_coord_t)lroundf(C + r * cosf(b0));
        s_band_pt[n][0].y = (lv_coord_t)lroundf(C + r * sinf(b0));
        s_band_pt[n][1].x = (lv_coord_t)lroundf(C + r * cosf(b1));
        s_band_pt[n][1].y = (lv_coord_t)lroundf(C + r * sinf(b1));
        lv_line_set_points(s_band[n], s_band_pt[n], 2);
        lv_obj_clear_flag(s_band[n], LV_OBJ_FLAG_HIDDEN);
    }
    return n;
}

/* the solid end of the band: an arc (whole degrees are fine at its length) */
static void band_solid(float a0, float a1)
{
    if (a1 - a0 < 1.0f) {
        lv_obj_add_flag(s_band_end, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_arc_set_angles(s_band_end, (uint16_t)lroundf(a0) % 360, (uint16_t)lroundf(a1) % 360);
    lv_obj_clear_flag(s_band_end, LV_OBJ_FLAG_HIDDEN);
}

/* numerals white or red, red ticks over the white ones past the limit, and
 * the band from the limit to the end of the scale: short pieces with gaps
 * at the limit, growing until they close up into a solid band */
static void draw_scale(void)
{
    const a4_scale_t *sc = DIAL[s_page].sc;
    float w = g_a4_set.warn[s_page];
    int nb = 0, nt = 0;
    float solid_from = NAN;
    for (int i = 0; i < MAX_NUMS; i++) lv_obj_add_flag(s_num[i], LV_OBJ_FLAG_HIDDEN);
    if (sc) {
        for (int i = 0; i < sc->n_nums && i < MAX_NUMS; i++) {
            const a4_num_t *nm = &sc->nums[i];
            lv_label_set_text_static(s_num[i], nm->t);
            lv_obj_set_style_text_color(s_num[i], nm->v >= w - 1e-4f ? C_RED : C_VAL, 0);
            lv_obj_align(s_num[i], LV_ALIGN_CENTER, (lv_coord_t)lroundf(nm->x - C),
                         (lv_coord_t)lroundf(nm->y - C));
            lv_obj_clear_flag(s_num[i], LV_OBJ_FLAG_HIDDEN);
        }
        for (int i = 0; i < sc->n_ticks && nt < MAX_TICKS; i++) {
            const a4_tick_t *t = &sc->ticks[i];
            if (t->v < w - 1e-4f) continue;
            s_tick_pt[nt][0].x = (lv_coord_t)lroundf(t->x0);
            s_tick_pt[nt][0].y = (lv_coord_t)lroundf(t->y0);
            s_tick_pt[nt][1].x = (lv_coord_t)lroundf(t->x1);
            s_tick_pt[nt][1].y = (lv_coord_t)lroundf(t->y1);
            lv_line_set_points(s_tick[nt], s_tick_pt[nt], 2);
            /* a little wider than the white one under it: no white fringe */
            lv_obj_set_style_line_width(s_tick[nt], (lv_coord_t)(t->w + 1.5f), 0);
            lv_obj_clear_flag(s_tick[nt], LV_OBJ_FLAG_HIDDEN);
            nt++;
        }
        float a0 = deg_of(sc, w < sc->lo ? sc->lo : w);
        float a1 = A4_START_DEG + A4_SWEEP_DEG;
        float span = a1 - a0;
        for (float pos = a0; span > 0.3f && pos < a1;) {
            float t = (pos - a0) / span;               /* 0 at the limit */
            float dash = 1.0f + 9.0f * powf(t, 1.3f);  /* degrees */
            float gap = 2.4f * (1.0f - t / 0.8f);      /* closes at 80 % */
            if (gap < 0.4f) {                          /* solid to the end */
                solid_from = pos;
                break;
            }
            nb = band_piece(nb, pos, fminf(pos + dash, a1));
            pos += dash + gap;
        }
    }
    for (int i = nt; i < MAX_TICKS; i++) lv_obj_add_flag(s_tick[i], LV_OBJ_FLAG_HIDDEN);
    for (int i = nb; i < MAX_BAND; i++) lv_obj_add_flag(s_band[i], LV_OBJ_FLAG_HIDDEN);
    if (sc && !isnan(solid_from)) band_solid(solid_from, A4_START_DEG + A4_SWEEP_DEG);
    else lv_obj_add_flag(s_band_end, LV_OBJ_FLAG_HIDDEN);
}

/* ------------------------------------------------------------- paging */
/* in the swipe: a dial, not left out */
static bool shown(int p)
{
    return p < A4_SWIPE && !(g_a4_set.hidden >> p & 1);
}

/* the next shown dial in the order, dir +1 / -1 */
static int step(int dir)
{
    int at = 0;
    for (int i = 0; i < A4_PAGES; i++) if (g_a4_set.order[i] == s_page) at = i;
    for (int k = 1; k <= A4_PAGES; k++) {
        int p = g_a4_set.order[(at + dir * k + A4_PAGES * 2) % A4_PAGES];
        if (shown(p)) return p;
    }
    return s_page;
}

static void dots_show(void)
{
    int n = 0, at = 0;
    for (int i = 0; i < A4_PAGES; i++) {
        int p = g_a4_set.order[i];
        if (!shown(p)) continue;
        if (p == s_page) at = n;
        n++;
    }
    bool dpf = s_page == A4_DPF;            /* not in the swipe: no dots */
    lv_coord_t cx = TX, y = C + 168;
    for (int i = 0; i < A4_PAGES; i++) {
        if (i >= n || dpf) {
            lv_obj_add_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(s_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(s_dots[i], cx - (n * 13) / 2 + i * 13 + 3, y);
        lv_obj_set_style_bg_color(s_dots[i], i == at ? C_DOT_ON : C_DOT, 0);
    }
}

/* ------------------------------------------------------------- events */
static void gesture_cb(lv_event_t *e)
{
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT) ui_a4_page(step(+1));
    if (dir == LV_DIR_RIGHT) ui_a4_page(step(-1));
}

static void click_cb(lv_event_t *e)
{
    (void)e;
    uint32_t now = lv_tick_get();
    if (s_last_click && now - s_last_click < DOUBLE_MS) {
        s_night = !s_night;
        a4_backlight(s_night ? NIGHT_PCT : 100);
        s_last_click = 0;
    } else {
        s_last_click = now;
    }
}

static void long_cb(lv_event_t *e)
{
    (void)e;
    a4_menu_open();
}

/* ------------------------------------------------------------- screen */
static lv_obj_t *label(lv_obj_t *par, const lv_font_t *f, lv_color_t c)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_label_set_text(l, "");
    return l;
}

static lv_obj_t *red_line(lv_obj_t *par, lv_coord_t w)
{
    lv_obj_t *l = lv_line_create(par);
    lv_obj_set_style_line_color(l, C_RED, 0);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

static void dpf_create(void)
{
    s_dpf = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_dpf);
    lv_obj_set_size(s_dpf, A4_SIZE, A4_SIZE);
    lv_obj_clear_flag(s_dpf, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    s_dpf_icon = lv_img_create(s_dpf);
    lv_img_set_src(s_dpf_icon, &a4_dpf_icon);
    lv_obj_set_style_img_recolor_opa(s_dpf_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(s_dpf_icon, C_ICON, 0);
    lv_obj_align(s_dpf_icon, LV_ALIGN_CENTER, 0, -156);
    lv_obj_t *t = label(s_dpf, &a4_txt_22, C_SMALL);
    lv_label_set_text(t, "SAZE g");
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -100);
    s_dpf_soot = label(s_dpf, &a4_big_96, C_VAL);
    lv_obj_set_width(s_dpf_soot, 300);
    lv_obj_set_style_text_align(s_dpf_soot, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_dpf_soot, LV_ALIGN_CENTER, 0, -48);
    /* three rows: the number right-aligned up to the middle, what it is
     * after it: differential pressure, the filter's surface, the exhaust
     * after it, the distance since the last regeneration */
    static const char *const ROW[4] = { "mbar dp", "\xC2\xB0" "C povrch",
                                        "\xC2\xB0" "C za DPF", "km od reg." };
    lv_obj_t **row_val[4] = { &s_dpf_dp, &s_dpf_temp, &s_dpf_after, &s_dpf_km };
    for (int i = 0; i < 4; i++) {
        lv_coord_t y = C + 6 + i * 40;
        lv_obj_t *v = label(s_dpf, &a4_num_40, C_VAL);
        lv_obj_set_width(v, 130);
        lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(v, C + 14 - 130, y);
        *row_val[i] = v;
        t = label(s_dpf, &a4_txt_22, C_SMALL);
        lv_label_set_text_static(t, ROW[i]);
        lv_obj_set_pos(t, C + 22, y + 15);
    }
    s_dpf_state = label(s_dpf, &a4_txt_22, C_AMBER);
    lv_obj_set_width(s_dpf_state, 220);
    lv_obj_set_style_text_align(s_dpf_state, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_dpf_state, LV_ALIGN_CENTER, 0, 176);
}

void ui_a4_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_black(), 0);

    s_face = lv_img_create(s_scr);
    lv_obj_set_pos(s_face, 0, 0);

    for (int i = 0; i < MAX_BAND; i++) s_band[i] = red_line(s_scr, A4_R_BAND1 - A4_R_BAND0);
    s_band_end = lv_arc_create(s_scr);
    lv_obj_remove_style_all(s_band_end);
    lv_obj_set_size(s_band_end, 2 * A4_R_BAND1, 2 * A4_R_BAND1);
    lv_obj_center(s_band_end);
    lv_arc_set_bg_angles(s_band_end, 0, 0);
    lv_obj_set_style_arc_width(s_band_end, A4_R_BAND1 - A4_R_BAND0, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_band_end, C_RED, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_band_end, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_band_end, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(s_band_end, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_band_end, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < MAX_TICKS; i++) s_tick[i] = red_line(s_scr, 5);
    for (int i = 0; i < MAX_NUMS; i++) {
        s_num[i] = label(s_scr, &a4_num_52, C_VAL);
        lv_obj_add_flag(s_num[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* the DPF lamp, left of the hub */
    s_lamp = lv_img_create(s_scr);
    lv_img_set_src(s_lamp, &a4_regen);
    lv_obj_set_pos(s_lamp, C - 100 - a4_regen.header.w / 2, C - a4_regen.header.h / 2);
    lv_obj_add_flag(s_lamp, LV_OBJ_FLAG_HIDDEN);

    /* lower right: the small line (REGENERACE), the value under it */
    s_line = label(s_scr, &a4_txt_22, C_SMALL);
    lv_obj_set_width(s_line, TW);
    lv_obj_set_style_text_align(s_line, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_line, TX - 22 - TW / 2, C + 8);   /* clear of the last numeral */
    s_val = label(s_scr, &a4_fis_54, C_VAL);
    lv_obj_set_width(s_val, TW);
    lv_obj_set_style_text_align(s_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_val, TX - TW / 2, C + 42);

    dpf_create();

    for (int i = 0; i < A4_PAGES; i++) {
        lv_obj_t *o = lv_obj_create(s_scr);
        lv_obj_remove_style_all(o);
        lv_obj_set_size(o, 7, 7);
        lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
        s_dots[i] = o;
    }

    s_needle = lv_img_create(s_scr);
    lv_img_set_src(s_needle, &a4_needle);
    lv_obj_set_pos(s_needle, C - NEEDLE_PX, C - NEEDLE_PY);
    lv_img_set_pivot(s_needle, NEEDLE_PX, NEEDLE_PY);
    lv_img_set_antialias(s_needle, true);
    lv_img_set_angle(s_needle, (int16_t)(A4_START_DEG * 10));

    s_cap = lv_img_create(s_scr);
    lv_img_set_src(s_cap, &a4_cap);
    lv_obj_set_pos(s_cap, C - a4_cap.header.w / 2, C - a4_cap.header.h / 2);

    lv_obj_add_event_cb(s_scr, gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(s_scr, click_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(s_scr, long_cb, LV_EVENT_LONG_PRESSED, NULL);

    a4_menu_create();
    /* the first shown dial in the order */
    s_page = g_a4_set.order[0];
    ui_a4_page(shown(s_page) ? s_page : step(+1));
    lv_scr_load(s_scr);
}

lv_obj_t *a4_gauge_screen(void)
{
    return s_scr;
}

void ui_a4_page(int page)
{
    if (page < 0 || page >= A4_PAGES) page = 0;
    a4_flip();                      /* the new dial goes out whole */
    if (page != A4_DPF) s_before_regen = -1;
    s_page = page;
    lv_img_set_src(s_face, DIAL[page].face);
    bool dpf = page == A4_DPF;
    lv_obj_t *dial_parts[] = { s_needle, s_cap, s_val, s_line };
    for (unsigned i = 0; i < sizeof dial_parts / sizeof dial_parts[0]; i++) {
        if (dpf) lv_obj_add_flag(dial_parts[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(dial_parts[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (dpf) lv_obj_clear_flag(s_dpf, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_dpf, LV_OBJ_FLAG_HIDDEN);
    draw_scale();
    dots_show();
}

void a4_settings_changed(void)
{
    if (!shown(s_page)) s_page = step(+1);
    ui_a4_page(s_page);
}

int ui_a4_current(void)
{
    return s_page;
}

void ui_a4_sweep(void)
{
    s_sweeping = true;
    s_sweep_t0 = lv_tick_get();
}

/* the regeneration: hotter than REGEN_TEMP, over below it - REGEN_HYST */
static void regen_watch(const rnd_data_t *d)
{
    float t = d->link ? d->dpf.temp_c : NAN;
    if (isnan(t)) return;                  /* no news: keep what we had */
    bool now = s_regen ? t >= REGEN_TEMP - REGEN_HYST : t >= REGEN_TEMP;
    if (now == s_regen) return;
    s_regen = now;
    a4_regen_sound(now);
    /* on the dials: the DPF page comes up for the regeneration, and the
     * dial it covered comes back after it (unless swiped away meanwhile) */
    if (lv_scr_act() != s_scr) return;
    if (now) {
        s_before_regen = s_page;
        if (s_page != A4_DPF) ui_a4_page(A4_DPF);
    } else if (s_page == A4_DPF && s_before_regen >= 0 && s_before_regen != A4_DPF) {
        ui_a4_page(s_before_regen);
        s_before_regen = -1;
    }
}

static void dpf_update(const rnd_data_t *d, bool blink_on)
{
    char buf[24];
    float soot = d->link ? d->dpf.soot_g : NAN;
    float dp = d->link ? d->dpf.dp_hpa : NAN;
    float tc = d->link ? d->dpf.temp_c : NAN;
    if (isnan(tc)) {
        lv_label_set_text(s_dpf_temp, "--");
    } else {
        fmt(buf, sizeof buf, "%.0f", tc);
        lv_label_set_text(s_dpf_temp, buf);
    }
    lv_obj_set_style_text_color(s_dpf_temp, s_regen ? C_AMBER : C_VAL, 0);
    float after = d->link ? d->diag.egt_dpf_c : NAN;
    if (isnan(after)) {
        lv_label_set_text(s_dpf_after, "--");
    } else {
        fmt(buf, sizeof buf, "%.0f", after);
        lv_label_set_text(s_dpf_after, buf);
    }
    float km = d->link ? d->dpf.dist_km : NAN;
    if (isnan(km)) {
        lv_label_set_text(s_dpf_km, "--");
    } else {
        fmt(buf, sizeof buf, "%.0f", km);
        lv_label_set_text(s_dpf_km, buf);
    }
    bool full = !isnan(soot) && soot >= g_a4_set.warn[A4_DPF];
    if (isnan(soot)) {
        lv_label_set_text(s_dpf_soot, d->link ? "--" : "- - -");
    } else {
        fmt(buf, sizeof buf, "%.1f", soot);
        lv_label_set_text(s_dpf_soot, buf);
    }
    lv_obj_set_style_text_color(s_dpf_soot, full ? C_RED : C_VAL, 0);
    if (isnan(dp)) {
        lv_label_set_text(s_dpf_dp, "--");
    } else {
        fmt(buf, sizeof buf, "%.0f", dp);
        lv_label_set_text(s_dpf_dp, buf);
    }
    /* the icon: white, amber while it regenerates, red (blinking) when full */
    lv_color_t ic = s_regen ? C_AMBER : full ? C_RED : C_ICON;
    lv_obj_set_style_img_recolor(s_dpf_icon, ic, 0);
    lv_obj_set_style_img_opa(s_dpf_icon,
        full && !s_regen && !blink_on ? LV_OPA_40 : LV_OPA_COVER, 0);
    if (s_regen) {
        lv_label_set_text(s_dpf_state, "REGENERACE");
        lv_obj_set_style_text_color(s_dpf_state, C_AMBER, 0);
    } else if (!d->link) {
        lv_label_set_text(s_dpf_state, "NO DATA");
        lv_obj_set_style_text_color(s_dpf_state, C_SMALL, 0);
    } else if (full) {
        lv_label_set_text(s_dpf_state, "PLN\xC3\x9D");
        lv_obj_set_style_text_color(s_dpf_state, C_RED, 0);
    } else {
        lv_label_set_text(s_dpf_state, "");
    }
}

void ui_a4_update(const rnd_data_t *d)
{
    bool blink_on = (s_frames++ / 8) % 2;
    regen_watch(d);
    if (lv_scr_act() != s_scr) {           /* in the menu */
        a4_menu_update(d);
        return;
    }
    a4_menu_update(d);                      /* keeps its trouble codes current */
    if (s_page == A4_DPF) {
        lv_obj_add_flag(s_lamp, LV_OBJ_FLAG_HIDDEN);   /* the icon says it */
        dpf_update(d, blink_on);
        return;
    }
    const dial_t *dl = &DIAL[s_page];
    const a4_scale_t *sc = dl->sc;
    float x = value_of(d, s_page);

    /* needle: the value, smoothed as a real one moves; under the scale it
     * rests at the start, over it just past the end */
    float frac = isnan(x) ? 0 : (x - sc->lo) / (sc->hi - sc->lo);
    if (frac < 0) frac = 0;
    if (frac > 1.02f) frac = 1.02f;
    if (s_sweeping) {
        uint32_t t = lv_tick_elaps(s_sweep_t0);
        if (t >= SWEEP_MS) {
            s_sweeping = false;
        } else {
            /* up to full scale in the first half, back to the value */
            float k = (float)t / SWEEP_MS;
            if (k < 0.5f) {
                frac = sinf(k * 3.14159f);
            } else {
                float u = (k - 0.5f) * 2;
                frac = 1 - u + u * frac;
            }
        }
    }
    float target = A4_START_DEG + A4_SWEEP_DEG * frac;
    if (isnan(s_angle) || s_sweeping) s_angle = target;
    else s_angle += (target - s_angle) * 0.25f;
    int16_t a10 = (int16_t)lroundf(s_angle * 10.0f);
    if (lv_img_get_angle(s_needle) != a10) lv_img_set_angle(s_needle, a10);

    char buf[32];
    if (!d->link) {
        lv_label_set_text(s_val, "- - -");
    } else if (isnan(x)) {
        lv_label_set_text(s_val, "--");
    } else {
        fmt(buf, sizeof buf, dl->fmt, x);
        lv_label_set_text(s_val, buf);
    }
    bool alarm = !isnan(x) && x >= g_a4_set.warn[s_page];
    lv_obj_set_style_text_color(s_val, alarm ? C_RED : C_VAL, 0);
    lv_obj_set_style_text_opa(s_val, alarm && !blink_on ? LV_OPA_30 : LV_OPA_COVER, 0);

    if (s_regen) {
        lv_label_set_text(s_line, "REGENERACE");
        lv_obj_set_style_text_color(s_line, C_AMBER, 0);
        lv_obj_clear_flag(s_lamp, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_lamp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(s_line, C_SMALL, 0);
        lv_label_set_text(s_line, d->link ? "" : "NO DATA");
    }
}
