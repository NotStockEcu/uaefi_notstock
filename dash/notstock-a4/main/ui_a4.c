/* The A4 gauge, see ui_a4.h. The dials are images (tools/gen_faces.py);
 * live are the needle (an image LVGL turns about its pivot), its cap, the
 * FIS window's text, the DPF lamp and the page dots. */
#include "ui_a4.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

LV_FONT_DECLARE(a4_fis_54);
LV_FONT_DECLARE(a4_txt_22);
LV_FONT_DECLARE(a4_big_96);
LV_IMG_DECLARE(a4_face_oil);
LV_IMG_DECLARE(a4_face_iat);
LV_IMG_DECLARE(a4_face_clt);
LV_IMG_DECLARE(a4_face_egt);
LV_IMG_DECLARE(a4_face_boost);
LV_IMG_DECLARE(a4_face_dpf);
LV_IMG_DECLARE(a4_needle);
LV_IMG_DECLARE(a4_cap);
LV_IMG_DECLARE(a4_regen);
LV_IMG_DECLARE(a4_dpf_icon);

#define C          (A4_SIZE / 2)
#define START_DEG  95.0f             /* as in gen_faces.py */
#define SWEEP_DEG  265.0f
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

#define C_VAL      lv_color_hex(0xEEEEEC)    /* as the printed numerals */
#define C_WARN     lv_color_hex(0xFF7618)    /* as the numerals past the limit */
#define C_SMALL    lv_color_hex(0xA0A0A0)
#define C_AMBER    lv_color_hex(0xFFA800)
#define C_ICON     lv_color_hex(0xD8D8D6)
#define C_RED      lv_color_hex(0xFF2A1E)
#define C_DOT      lv_color_hex(0x3A3A3E)
#define C_DOT_ON   lv_color_hex(0xD8D8D8)

typedef struct {
    const lv_img_dsc_t *face;
    int   src;              /* RND_* in rnd_data_t.v, -1: the DPF soot */
    float lo, hi, warn;     /* scale and red band, as gen_faces.py */
    const char *fmt;
} dial_t;

static const dial_t DIAL[A4_PAGES] = {
    [A4_OIL]   = { &a4_face_oil,   RND_OIL,     50, 150, 130,  "%.0f" },
    [A4_IAT]   = { &a4_face_iat,   RND_INTAKE, -20,  80,  60,  "%.0f" },
    [A4_CLT]   = { &a4_face_clt,   RND_WATER,   50, 130, 105,  "%.0f" },
    [A4_EGT]   = { &a4_face_egt,   RND_EXHAUST,  0, 1000, 750, "%.0f" },
    [A4_BOOST] = { &a4_face_boost, RND_BOOST,    0, 2.5f, 2.2f, "%.2f" },
    [A4_DPF]   = { &a4_face_dpf,   -1,           0,  40,  24,  "%.1f" },
};

static lv_obj_t *s_scr, *s_face, *s_needle, *s_cap, *s_lamp;
static lv_obj_t *s_val, *s_line, *s_dots[A4_PAGES];
/* the DPF page: no scale, the filter icon and the values */
static lv_obj_t *s_dpf, *s_dpf_icon, *s_dpf_soot, *s_dpf_dp, *s_dpf_state;
static int s_page;
static float s_angle = NAN;        /* shown, degrees */
static bool s_regen, s_night;
static uint32_t s_frames, s_sweep_t0, s_last_click;
static bool s_sweeping;

static float value_of(const rnd_data_t *d, int page)
{
    if (!d->link) return NAN;
    int src = DIAL[page].src;
    return src < 0 ? d->dpf.soot_g : d->v[src];
}

static void fmt(char *buf, size_t n, const char *f, float x)
{
    snprintf(buf, n, f, x);
    if (buf[0] == '-' && atof(buf) == 0) snprintf(buf, n, f, 0.0);
}

/* ------------------------------------------------------------- events */
static void gesture_cb(lv_event_t *e)
{
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT) ui_a4_page((s_page + 1) % A4_PAGES);
    if (dir == LV_DIR_RIGHT) ui_a4_page((s_page + A4_PAGES - 1) % A4_PAGES);
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

/* ------------------------------------------------------------- screen */
void ui_a4_create(int page)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_black(), 0);

    s_face = lv_img_create(s_scr);
    lv_obj_set_pos(s_face, 0, 0);

    /* the DPF lamp, between the hub and the window */
    s_lamp = lv_img_create(s_scr);
    lv_img_set_src(s_lamp, &a4_regen);
    lv_obj_set_pos(s_lamp, C - 100 - a4_regen.header.w / 2, C - a4_regen.header.h / 2);
    lv_obj_add_flag(s_lamp, LV_OBJ_FLAG_HIDDEN);

    /* lower right: the small line (REGENERACE, the DPF's pressure), the
     * value under it */
    s_line = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_line, &a4_txt_22, 0);
    lv_obj_set_style_text_color(s_line, C_SMALL, 0);
    lv_obj_set_width(s_line, TW);
    lv_obj_set_style_text_align(s_line, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_line, TX - 22 - TW / 2, C + 8);   /* clear of the last numeral */
    lv_label_set_text(s_line, "");

    s_val = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_val, &a4_fis_54, 0);
    lv_obj_set_style_text_color(s_val, C_VAL, 0);
    lv_obj_set_width(s_val, TW);
    lv_obj_set_style_text_align(s_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_val, TX - TW / 2, C + 42);
    lv_label_set_text(s_val, "");

    for (int i = 0; i < A4_PAGES; i++) {
        lv_obj_t *o = lv_obj_create(s_scr);
        lv_obj_remove_style_all(o);
        lv_obj_set_size(o, 7, 7);
        lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_pos(o, TX - (A4_PAGES * 13) / 2 + i * 13 + 3, C + 168);
        lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
        s_dots[i] = o;
    }

    /* the DPF page */
    s_dpf = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_dpf);
    lv_obj_set_size(s_dpf, A4_SIZE, A4_SIZE);
    lv_obj_clear_flag(s_dpf, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    s_dpf_icon = lv_img_create(s_dpf);
    lv_img_set_src(s_dpf_icon, &a4_dpf_icon);
    lv_obj_set_style_img_recolor_opa(s_dpf_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(s_dpf_icon, C_ICON, 0);
    lv_obj_align(s_dpf_icon, LV_ALIGN_CENTER, 0, -112);
    lv_obj_t *t = lv_label_create(s_dpf);
    lv_obj_set_style_text_font(t, &a4_txt_22, 0);
    lv_obj_set_style_text_color(t, C_SMALL, 0);
    lv_label_set_text(t, "SAZE g");
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -36);
    s_dpf_soot = lv_label_create(s_dpf);
    lv_obj_set_style_text_font(s_dpf_soot, &a4_big_96, 0);
    lv_obj_set_style_text_color(s_dpf_soot, C_VAL, 0);
    lv_label_set_text(s_dpf_soot, "");
    lv_obj_align(s_dpf_soot, LV_ALIGN_CENTER, 0, 22);
    s_dpf_dp = lv_label_create(s_dpf);
    lv_obj_set_style_text_font(s_dpf_dp, &a4_fis_54, 0);
    lv_obj_set_style_text_color(s_dpf_dp, C_VAL, 0);
    lv_label_set_text(s_dpf_dp, "");
    /* the pressure right-aligned up to the middle, "mbar" after it */
    lv_obj_set_width(s_dpf_dp, 120);
    lv_obj_set_style_text_align(s_dpf_dp, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_dpf_dp, C + 14 - 120, C + 76);
    t = lv_label_create(s_dpf);
    lv_obj_set_style_text_font(t, &a4_txt_22, 0);
    lv_obj_set_style_text_color(t, C_SMALL, 0);
    lv_label_set_text(t, "mbar dp");
    lv_obj_set_pos(t, C + 22, C + 106);
    s_dpf_state = lv_label_create(s_dpf);
    lv_obj_set_style_text_font(s_dpf_state, &a4_txt_22, 0);
    lv_obj_set_style_text_color(s_dpf_state, C_AMBER, 0);
    lv_label_set_text(s_dpf_state, "");
    lv_obj_align(s_dpf_state, LV_ALIGN_CENTER, 0, 158);

    s_needle = lv_img_create(s_scr);
    lv_img_set_src(s_needle, &a4_needle);
    lv_obj_set_pos(s_needle, C - NEEDLE_PX, C - NEEDLE_PY);
    lv_img_set_pivot(s_needle, NEEDLE_PX, NEEDLE_PY);
    lv_img_set_antialias(s_needle, true);
    lv_img_set_angle(s_needle, (int16_t)(START_DEG * 10));

    s_cap = lv_img_create(s_scr);
    lv_img_set_src(s_cap, &a4_cap);
    lv_obj_set_pos(s_cap, C - a4_cap.header.w / 2, C - a4_cap.header.h / 2);

    lv_obj_add_event_cb(s_scr, gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(s_scr, click_cb, LV_EVENT_SHORT_CLICKED, NULL);
    ui_a4_page(page);
    lv_scr_load(s_scr);
}

void ui_a4_page(int page)
{
    if (page < 0 || page >= A4_PAGES) page = 0;
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
    for (int i = 0; i < A4_PAGES; i++) {
        /* under the values on the DPF page, under the text on the dials */
        lv_obj_set_x(s_dots[i], (dpf ? C : TX) - (A4_PAGES * 13) / 2 + i * 13 + 3);
        lv_obj_set_y(s_dots[i], dpf ? C + 190 : C + 168);
    }
    for (int i = 0; i < A4_PAGES; i++) {
        lv_obj_set_style_bg_color(s_dots[i], i == page ? C_DOT_ON : C_DOT, 0);
    }
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
}

static void dpf_update(const rnd_data_t *d, bool blink_on)
{
    char buf[24];
    float soot = d->link ? d->dpf.soot_g : NAN;
    float dp = d->link ? d->dpf.dp_hpa : NAN;
    bool full = !isnan(soot) && soot >= DIAL[A4_DPF].warn;
    if (isnan(soot)) {
        lv_label_set_text(s_dpf_soot, d->link ? "--" : "- - -");
    } else {
        fmt(buf, sizeof buf, "%.1f", soot);
        lv_label_set_text(s_dpf_soot, buf);
    }
    lv_obj_set_style_text_color(s_dpf_soot, full ? C_WARN : C_VAL, 0);
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
        lv_label_set_text(s_dpf_state, "PLNY");
        lv_obj_set_style_text_color(s_dpf_state, C_WARN, 0);
    } else {
        lv_label_set_text(s_dpf_state, "");
    }
}

void ui_a4_update(const rnd_data_t *d)
{
    const dial_t *dl = &DIAL[s_page];
    bool blink_on = (s_frames++ / 8) % 2;
    regen_watch(d);
    if (s_page == A4_DPF) {
        lv_obj_add_flag(s_lamp, LV_OBJ_FLAG_HIDDEN);   /* the icon says it */
        dpf_update(d, blink_on);
        return;
    }
    float x = value_of(d, s_page);

    /* needle: the value, smoothed as a real one moves; under the scale it
     * rests at the start, over it at the end */
    float frac = isnan(x) ? 0 : (x - dl->lo) / (dl->hi - dl->lo);
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
    float target = START_DEG + SWEEP_DEG * frac;
    if (isnan(s_angle) || s_sweeping) s_angle = target;
    else s_angle += (target - s_angle) * 0.25f;
    int16_t a10 = (int16_t)lroundf(s_angle * 10.0f);
    if (lv_img_get_angle(s_needle) != a10) lv_img_set_angle(s_needle, a10);

    /* the window: the value; the small line: REGENERACE, or on the DPF dial
     * the differential pressure */
    char buf[32];
    if (!d->link) {
        lv_label_set_text(s_val, "- - -");
    } else if (isnan(x)) {
        lv_label_set_text(s_val, "--");
    } else {
        fmt(buf, sizeof buf, dl->fmt, x);
        lv_label_set_text(s_val, buf);
    }
    bool alarm = !isnan(x) && x >= dl->warn;
    lv_obj_set_style_text_color(s_val, alarm ? C_WARN : C_VAL, 0);
    lv_obj_set_style_text_opa(s_val, alarm && !blink_on ? LV_OPA_30 : LV_OPA_COVER, 0);

    if (s_regen) {
        lv_label_set_text(s_line, "REGENERACE");
        lv_obj_set_style_text_color(s_line, C_AMBER, 0);
        lv_obj_clear_flag(s_lamp, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_lamp, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(s_line, C_SMALL, 0);
        if (!d->link) {
            lv_label_set_text(s_line, "NO DATA");
        } else {
            lv_label_set_text(s_line, "");
        }
    }
}
