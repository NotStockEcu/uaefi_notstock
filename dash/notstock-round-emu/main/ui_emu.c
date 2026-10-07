/* EMU pages, see ui_emu.h. Inside the round panel everything sits in the
 * inscribed square (70..410): bar column on the left, a divider, the
 * channel column on the right, page dots under it all. */
#include "ui_emu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

LV_FONT_DECLARE(emu_num_80);
LV_FONT_DECLARE(emu_num_64);
LV_FONT_DECLARE(emu_txt_28);
LV_FONT_DECLARE(emu_txt_18);
LV_IMG_DECLARE(emu_bg);

#define C_TEXT    lv_color_hex(0xF2F2F2)
#define C_SMALL   lv_color_hex(0x9AA0A6)
#define C_LINE    lv_color_hex(0x8C9196)
#define C_FILL    lv_color_hex(0xC6DB2A)     /* yellow-green */
#define C_WARN    lv_color_hex(0xFF8A00)
#define C_RED     lv_color_hex(0xFF2A2A)
#define C_COLD    lv_color_hex(0x3AA0FF)
#define C_LED_ON  lv_color_hex(0x2BFF5A)
#define C_DOT     lv_color_hex(0x3A3F45)
#define C_MARK    lv_color_hex(0x5A5F66)

/* --------------------------------------------------------------- data */
static float fresh(float x, int64_t at, int64_t now)
{
    return at != 0 && now - at < EMU_STALE_US ? x : NAN;
}

void emu_view_from(const emu_values_t *v, int64_t now_us, emu_view_t *o)
{
    int64_t f0 = v->frame_us[0], f2 = v->frame_us[2], f3 = v->frame_us[3];
    float map = fresh(v->map_kpa, f0, now_us);
    /* boost over the EMU's barometer; before that comes: sea level */
    float baro = fresh(v->baro_kpa, f2, now_us);
    if (!(baro > 50 && baro < 130)) baro = 101.3f;
    o->boost = (map - baro) / 100.0f;           /* NAN stays NAN */
    float lam = fresh(v->lambda, f3, now_us);
    o->afr = lam > 0 ? lam * EMU_STOICH : NAN;
    o->clt = fresh(v->clt, f2, now_us);
    o->iat = fresh(v->iat, f0, now_us);
    o->tps = fresh(v->tps, f0, now_us);
    bool ef = v->frame_us[4] && now_us - v->frame_us[4] < EMU_STALE_US;
    o->err_map = ef && (v->err & EMU_ERR_MAP);
    o->err_wbo = ef && (v->err & EMU_ERR_WBO);
    o->err_clt = ef && (v->err & EMU_ERR_CLT);
    o->err_iat = ef && (v->err & EMU_ERR_IAT);
    int64_t f6 = v->frame_us[6];
    o->fan = f6 && now_us - f6 < EMU_STALE_US
           ? (v->outflags[3] & EMU_OUT4_FAN) != 0 : -1;
    o->link = false;
    for (int i = 0; i < EMU_FRAMES; i++) {
        if (v->frame_us[i] && now_us - v->frame_us[i] < EMU_STALE_US) {
            o->link = true;
        }
    }
}

/* ----------------------------------------------------------- channels */
typedef struct {
    const char *name, *unit, *fmt;
    float lo, hi;                 /* the bar */
    float ticks[4];               /* scale lines on the bar, NAN: end */
    const char *tick_fmt;
    float lo_alarm, lo_warn;      /* NAN: none */
    float hi_warn, hi_alarm;
    bool  lo_cold;                /* under lo_warn is cold (blue), not bad */
} chan_t;

enum { CH_BOOST, CH_AFR, CH_TPS, CH_CLT, CH_IAT, CH_N };

static const chan_t CH[CH_N] = {
    [CH_BOOST] = { "BOOST", "bar", "%.2f", -1.0f, 2.5f, { 0, 1, 2, NAN }, "%.0f",
                   NAN, NAN, 1.8f, 2.2f, false },
    [CH_AFR]   = { "AFR", "", "%.1f", 10, 20, { NAN }, "",
                   10.5f, 11.2f, 15.2f, 16.0f, false },
    [CH_TPS]   = { "Throttle", "%", "%.0f", 0, 100, { NAN }, "",
                   NAN, NAN, NAN, NAN, false },
    [CH_CLT]   = { "CLT", "\xC2\xB0" "C", "%.0f", 20, 120, { 50, 80, 110, NAN }, "%.0f",
                   NAN, 60, 100, 108, true },
    [CH_IAT]   = { "IAT", "\xC2\xB0" "C", "%.0f", -20, 80, { NAN }, "",
                   NAN, NAN, 50, 65, false },
};

typedef enum { L_OK, L_COLD, L_WARN, L_ALARM } level_t;

static level_t level(const chan_t *c, float x)
{
    if (x >= c->hi_alarm || x <= c->lo_alarm) return L_ALARM;
    if (x >= c->hi_warn) return L_WARN;
    if (x <= c->lo_warn) return c->lo_cold ? L_COLD : L_WARN;
    return L_OK;
}

static void fmt(char *buf, size_t n, const char *f, float x)
{
    snprintf(buf, n, f, x);
    if (buf[0] == '-' && atof(buf) == 0) snprintf(buf, n, f, 0.0);
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

static lv_obj_t *page_obj(lv_obj_t *scr)
{
    lv_obj_t *o = lv_obj_create(scr);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, EMU_W, EMU_H);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

/* the bar column: name, a framed vertical bar with scale lines to its
 * right, the orange warn line across, the value and max under it */
typedef struct {
    int ch;
    lv_obj_t *fill, *val, *max;
    lv_coord_t inner_h;
    float shown, peak;
} bar_t;

#define BX 92           /* bar frame */
#define BY 112
#define BW 56
#define BH 206
#define TICK_X (BX + BW)
#define TICK_W 62

static lv_coord_t bar_y(const chan_t *c, float x)
{
    float f = (x - c->lo) / (c->hi - c->lo);
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    return (lv_coord_t)(BY + BH - 3 - f * (BH - 6));
}

static void bar_build(lv_obj_t *pg, bar_t *b, int ch)
{
    const chan_t *c = &CH[ch];
    b->ch = ch;
    b->shown = b->peak = NAN;
    lv_obj_t *n = text(pg, &emu_txt_28, C_TEXT, c->name);
    lv_obj_set_pos(n, BX - 6, 76);
    lv_obj_t *u = text(pg, &emu_txt_18, C_SMALL, c->unit);
    lv_obj_align_to(u, n, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);

    lv_obj_t *frame = rect(pg, BX, BY, BW, BH, lv_color_black());
    lv_obj_set_style_bg_opa(frame, LV_OPA_50, 0);
    lv_obj_set_style_border_color(frame, C_TEXT, 0);
    lv_obj_set_style_border_width(frame, 2, 0);
    b->inner_h = BH - 6;
    b->fill = rect(frame, 1, BH - 3 - 1, BW - 6, 0, C_FILL);
    lv_obj_set_align(b->fill, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_pos(b->fill, 0, -3);

    for (int i = 0; i < 4 && !isnan(c->ticks[i]); i++) {
        lv_coord_t y = bar_y(c, c->ticks[i]);
        rect(pg, TICK_X, y, TICK_W, 2, C_LINE);
        char t[12];
        snprintf(t, sizeof t, c->tick_fmt, c->ticks[i]);
        lv_obj_t *l = text(pg, &emu_txt_18, C_TEXT, t);
        lv_obj_set_pos(l, TICK_X + 26, y - 22);
    }
    if (!isnan(c->hi_warn)) {
        rect(pg, BX - 4, bar_y(c, c->hi_warn) - 1, BW + 8 + TICK_W, 3, C_WARN);
    }
    b->val = text(pg, &emu_num_64, C_TEXT, "--");
    lv_obj_set_pos(b->val, BX - 6, BY + BH + 2);
    /* the max: right of the bar's foot, under the lowest scale line */
    b->max = text(pg, &emu_txt_18, C_SMALL, "");
    lv_obj_set_pos(b->max, TICK_X + 8, BY + BH - 24);
}

/* a channel on the right: name top left, value right, max under it */
typedef struct {
    int ch;
    lv_obj_t *val, *max;
    const lv_font_t *num;
    float peak;
} slot_t;

#define RX 246          /* right column */
#define RW 160

static void slot_build(lv_obj_t *pg, slot_t *s, int ch, lv_coord_t y,
                       lv_coord_t h, const lv_font_t *num)
{
    const chan_t *c = &CH[ch];
    s->ch = ch;
    s->num = num;
    s->peak = NAN;
    lv_obj_t *n = text(pg, &emu_txt_28, C_TEXT, c->name);
    lv_obj_set_pos(n, RX, y + 4);
    lv_obj_t *u = text(pg, &emu_txt_18, C_SMALL, c->unit);
    lv_obj_align_to(u, n, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);
    s->val = text(pg, num, C_TEXT, "--");
    lv_obj_set_width(s->val, RW);
    lv_obj_set_style_text_align(s->val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s->val, RX, y + h / 2 - lv_font_get_line_height(num) / 2 + 4);
    s->max = text(pg, &emu_txt_18, C_SMALL, "");
    lv_obj_set_pos(s->max, RX, y + h - 26);
}

/* ------------------------------------------------------------- screen */
static lv_obj_t *s_scr, *s_pg[EMU_PAGES], *s_led, *s_dot[EMU_PAGES];
static lv_obj_t *s_fan, *s_fan_txt;
static bar_t s_boost_bar, s_clt_bar;
static slot_t s_afr, s_tps, s_iat;
static int s_page;
static uint32_t s_frame;

static void gesture_cb(lv_event_t *e)
{
    (void)e;
    lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (d == LV_DIR_LEFT) ui_emu_page((s_page + 1) % EMU_PAGES);
    if (d == LV_DIR_RIGHT) ui_emu_page((s_page + EMU_PAGES - 1) % EMU_PAGES);
}

void ui_emu_page(int page)
{
    s_page = page;
    for (int i = 0; i < EMU_PAGES; i++) {
        if (i == page) lv_obj_clear_flag(s_pg[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_pg[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_dot[i], i == page ? C_TEXT : C_DOT, 0);
    }
}

static void divider(lv_obj_t *pg)
{
    rect(pg, 232, 80, 2, 326, C_LINE);
}

void ui_emu_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_scr);
    lv_obj_set_style_bg_color(s_scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_scr, gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_t *bg = lv_img_create(s_scr);
    lv_img_set_src(bg, &emu_bg);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_CLICKABLE);

    /* page 1: BOOST bar, AFR and throttle */
    s_pg[0] = page_obj(s_scr);
    bar_build(s_pg[0], &s_boost_bar, CH_BOOST);
    divider(s_pg[0]);
    slot_build(s_pg[0], &s_afr, CH_AFR, 76, 160, &emu_num_80);
    rect(s_pg[0], 240, 240, 150, 2, C_LINE);
    slot_build(s_pg[0], &s_tps, CH_TPS, 246, 160, &emu_num_80);

    /* page 2: CLT bar, IAT */
    s_pg[1] = page_obj(s_scr);
    bar_build(s_pg[1], &s_clt_bar, CH_CLT);
    divider(s_pg[1]);
    slot_build(s_pg[1], &s_iat, CH_IAT, 76, 330, &emu_num_80);

    /* the link "LED" at the top, page dots and the mark at the bottom */
    s_led = rect(s_scr, EMU_W / 2 - 41, 32, 14, 14, C_RED);
    /* FAN: an outline while off, lit while the coolant fan runs */
    s_fan = rect(s_scr, EMU_W / 2 - 9, 28, 50, 22, lv_color_black());
    lv_obj_set_style_bg_opa(s_fan, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(s_fan, 11, 0);
    lv_obj_set_style_border_width(s_fan, 2, 0);
    lv_obj_set_style_border_color(s_fan, C_DOT, 0);
    s_fan_txt = text(s_fan, &emu_txt_18, C_DOT, "FAN");
    lv_obj_center(s_fan_txt);
    lv_obj_add_flag(s_fan, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_radius(s_led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(s_led, 16, 0);
    lv_obj_set_style_shadow_spread(s_led, 2, 0);
    for (int i = 0; i < EMU_PAGES; i++) {
        s_dot[i] = rect(s_scr, EMU_W / 2 - 14 + 20 * i, 420, 8, 8, C_DOT);
        lv_obj_set_style_radius(s_dot[i], LV_RADIUS_CIRCLE, 0);
    }
    lv_obj_t *mark = text(s_scr, &emu_txt_18, C_MARK, "NOT STOCK");
    lv_obj_align(mark, LV_ALIGN_TOP_MID, 0, 436);

    ui_emu_page(0);
    lv_scr_load(s_scr);
}

/* --------------------------------------------------------------- frame */
static lv_color_t level_color(level_t l, lv_color_t ok)
{
    return l == L_ALARM ? C_RED : l == L_WARN ? C_WARN
         : l == L_COLD ? C_COLD : ok;
}

static void show_max(lv_obj_t *l, const chan_t *c, float peak)
{
    char b[24], v[12];
    if (isnan(peak)) {
        lv_label_set_text(l, "");
        return;
    }
    fmt(v, sizeof v, c->fmt, peak);
    snprintf(b, sizeof b, "MAX %s", v);
    lv_label_set_text(l, b);
}

static void bar_draw(bar_t *b, float x, bool err, bool blink)
{
    const chan_t *c = &CH[b->ch];
    lv_obj_set_style_text_font(b->val, err ? &emu_txt_28 : &emu_num_64, 0);
    if (err || isnan(x)) {
        lv_label_set_text(b->val, err ? "SENSOR ERR" : "--");
        lv_obj_set_style_text_color(b->val, err ? C_WARN : C_SMALL, 0);
        lv_obj_set_height(b->fill, 0);
        b->shown = NAN;
        show_max(b->max, c, b->peak);
        return;
    }
    char buf[16];
    fmt(buf, sizeof buf, c->fmt, x);
    lv_label_set_text(b->val, buf);
    if (isnan(b->peak) || x > b->peak) b->peak = x;
    show_max(b->max, c, b->peak);

    float f = (x - c->lo) / (c->hi - c->lo);
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    b->shown = isnan(b->shown) ? f : b->shown + (f - b->shown) * 0.35f;
    lv_obj_set_height(b->fill, (lv_coord_t)(b->shown * b->inner_h + 0.5f));
    level_t lv = level(c, x);
    lv_obj_set_style_bg_color(b->fill, level_color(lv, C_FILL), 0);
    lv_obj_set_style_text_color(b->val,
        lv == L_ALARM ? (blink ? C_RED : C_TEXT) : C_TEXT, 0);
}

static void slot_draw(slot_t *s, float x, bool err, bool blink)
{
    const chan_t *c = &CH[s->ch];
    lv_obj_set_style_text_font(s->val, err ? &emu_txt_28 : s->num, 0);
    if (err || isnan(x)) {
        lv_label_set_text(s->val, err ? "SENSOR ERR" : "--");
        lv_obj_set_style_text_color(s->val, err ? C_WARN : C_SMALL, 0);
        show_max(s->max, c, s->peak);
        return;
    }
    char buf[16];
    fmt(buf, sizeof buf, c->fmt, x);
    lv_label_set_text(s->val, buf);
    if (isnan(s->peak) || x > s->peak) s->peak = x;
    show_max(s->max, c, s->peak);
    level_t lv = level(c, x);
    lv_color_t col = lv == L_OK ? C_TEXT : level_color(lv, C_TEXT);
    if (lv == L_ALARM && !blink) col = C_TEXT;
    lv_obj_set_style_text_color(s->val, col, 0);
}

void ui_emu_update(const emu_view_t *v)
{
    s_frame++;
    bool blink = (s_frame / 8) % 2 == 0;          /* ~2 Hz at 30 Hz */
    bool k = v->link;
    bar_draw(&s_boost_bar, k ? v->boost : NAN, k && v->err_map, blink);
    slot_draw(&s_afr, k ? v->afr : NAN, k && v->err_wbo, blink);
    slot_draw(&s_tps, k ? v->tps : NAN, false, blink);
    bar_draw(&s_clt_bar, k ? v->clt : NAN, k && v->err_clt, blink);
    slot_draw(&s_iat, k ? v->iat : NAN, k && v->err_iat, blink);
    lv_obj_set_style_bg_color(s_led, k ? C_LED_ON : C_RED, 0);
    lv_obj_set_style_shadow_color(s_led, k ? C_LED_ON : C_RED, 0);

    int fan = k ? v->fan : -1;
    if (fan < 0) {
        lv_obj_add_flag(s_fan, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_fan, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_fan, C_COLD, 0);
        lv_obj_set_style_bg_opa(s_fan, fan ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(s_fan, fan ? C_COLD : C_DOT, 0);
        lv_obj_set_style_text_color(s_fan_txt,
                                    fan ? lv_color_black() : C_DOT, 0);
    }
}
