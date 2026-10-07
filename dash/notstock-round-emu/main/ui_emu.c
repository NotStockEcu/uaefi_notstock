/* EMU page, see ui_emu.h. One screen: three channel panels on black, the
 * NOT STOCK mark at the bottom, the link state at the top. */
#include "ui_emu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

LV_FONT_DECLARE(emu_num_120);
LV_FONT_DECLARE(emu_num_80);
LV_FONT_DECLARE(emu_txt_28);
LV_FONT_DECLARE(emu_txt_22);

#define C_BG      lv_color_hex(0x000000)
#define C_PANEL   lv_color_hex(0x0E1013)
#define C_EDGE    lv_color_hex(0x30353A)
#define C_LABEL   lv_color_hex(0xE6EAEE)
#define C_UNIT    lv_color_hex(0x8A9096)
#define C_TRACK   lv_color_hex(0x1D2125)
#define C_GREEN   lv_color_hex(0x2FD05A)
#define C_AMBER   lv_color_hex(0xFFB000)
#define C_RED     lv_color_hex(0xFF2A2A)
#define C_COLD    lv_color_hex(0x3AA0FF)
#define C_ALARM   lv_color_hex(0xB0121A)
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
    /* before the barometer comes (or if it reads nonsense): sea level */
    float baro = fresh(v->baro_kpa, f2, now_us);
    if (!(baro > 50 && baro < 130)) baro = 101.3f;
    o->boost = (map - baro) / 100.0f;           /* NAN stays NAN */
    float lam = fresh(v->lambda, f3, now_us);
    o->afr = lam > 0 ? lam * EMU_STOICH : NAN;
    o->clt = fresh(v->clt, f2, now_us);
    bool ef = v->frame_us[4] && now_us - v->frame_us[4] < EMU_STALE_US;
    o->err_map = ef && (v->err & EMU_ERR_MAP);
    o->err_wbo = ef && (v->err & EMU_ERR_WBO);
    o->err_clt = ef && (v->err & EMU_ERR_CLT);
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
    float zero;                   /* a tick here on the bar, NAN: none */
    float lo_alarm, lo_warn;      /* NAN: none */
    float hi_warn, hi_alarm;
    bool  lo_cold;                /* under lo_warn is cold (blue), not bad */
} chan_t;

enum { CH_BOOST, CH_AFR, CH_CLT, CH_N };

static const chan_t CH[CH_N] = {
    [CH_BOOST] = { "BOOST", "BAR", "%.2f", -1.0f, 2.5f, 0.0f,
                   NAN, NAN, 1.8f, 2.2f, false },
    [CH_AFR]   = { "AFR", "", "%.1f", 10.0f, 20.0f, EMU_STOICH,
                   10.5f, 11.2f, 15.2f, 16.0f, false },
    [CH_CLT]   = { "CLT", "\xC2\xB0" "C", "%.0f", 40.0f, 120.0f, NAN,
                   NAN, 60.0f, 100.0f, 108.0f, true },
};

typedef struct {
    lv_obj_t *panel, *name, *unit, *val, *fill, *tick;
    const lv_font_t *num;
    lv_coord_t bar_w;
    float shown;                  /* the bar, smoothed */
} panel_t;

static panel_t s_p[CH_N];
static lv_obj_t *s_scr, *s_link, *s_link_dot;
static uint32_t s_frame;

static lv_obj_t *rect(lv_obj_t *par, lv_coord_t x, lv_coord_t y,
                      lv_coord_t w, lv_coord_t h, lv_color_t c, int radius)
{
    lv_obj_t *o = lv_obj_create(par);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static lv_obj_t *text(lv_obj_t *par, const lv_font_t *f, lv_color_t c)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    return l;
}

static void panel(int ch, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                  lv_coord_t h, const lv_font_t *num, lv_coord_t val_y)
{
    panel_t *p = &s_p[ch];
    const chan_t *c = &CH[ch];
    p->panel = rect(s_scr, x, y, w, h, C_PANEL, 10);
    lv_obj_set_style_border_color(p->panel, C_EDGE, 0);
    lv_obj_set_style_border_width(p->panel, 2, 0);

    p->name = text(p->panel, &emu_txt_28, C_LABEL);
    lv_label_set_text(p->name, c->name);
    lv_obj_set_pos(p->name, 14, 6);
    p->unit = text(p->panel, &emu_txt_22, C_UNIT);
    lv_label_set_text(p->unit, c->unit);
    lv_obj_align(p->unit, LV_ALIGN_TOP_RIGHT, -14, 11);

    p->num = num;
    p->val = text(p->panel, num, C_LABEL);
    lv_label_set_text(p->val, "--");
    lv_obj_align(p->val, LV_ALIGN_TOP_RIGHT, -14, val_y);

    p->bar_w = w - 28;
    lv_obj_t *track = rect(p->panel, 14, h - 26, p->bar_w, 12, C_TRACK, 2);
    p->fill = rect(track, 0, 0, 0, 12, C_GREEN, 2);
    p->tick = NULL;
    if (!isnan(c->zero)) {
        lv_coord_t tx = (lv_coord_t)((c->zero - c->lo) / (c->hi - c->lo) *
                                     p->bar_w);
        p->tick = rect(p->panel, 14 + tx - 1, h - 31, 2, 22, C_UNIT, 0);
    }
    p->shown = NAN;
}

void ui_emu_create(void)
{
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_scr);
    lv_obj_set_style_bg_color(s_scr, C_BG, 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* link state, top */
    s_link_dot = rect(s_scr, 0, 0, 10, 10, C_GREEN, 5);
    s_link = text(s_scr, &emu_txt_22, C_UNIT);
    lv_label_set_text(s_link, "CAN");
    lv_obj_align(s_link, LV_ALIGN_TOP_MID, 8, 38);
    lv_obj_align_to(s_link_dot, s_link, LV_ALIGN_OUT_LEFT_MID, -8, 0);

    panel(CH_BOOST, 88, 72, 304, 178, &emu_num_120, 34);
    panel(CH_AFR, 70, 260, 166, 138, &emu_num_80, 38);
    panel(CH_CLT, 244, 260, 166, 138, &emu_num_80, 38);

    lv_obj_t *mark = text(s_scr, &emu_txt_22, C_MARK);
    lv_label_set_text(mark, "NOT STOCK");
    lv_obj_align(mark, LV_ALIGN_TOP_MID, 0, 412);

    lv_scr_load(s_scr);
}

/* --------------------------------------------------------------- frame */
typedef enum { LV_OK_, LV_COLD, LV_WARN, LV_ALARM } level_t;

static level_t level(const chan_t *c, float x)
{
    if (x >= c->hi_alarm || x <= c->lo_alarm) return LV_ALARM;
    if (x >= c->hi_warn) return LV_WARN;
    if (x <= c->lo_warn) return c->lo_cold ? LV_COLD : LV_WARN;
    return LV_OK_;
}

static void draw(int ch, float x, bool err, bool blink_on)
{
    panel_t *p = &s_p[ch];
    const chan_t *c = &CH[ch];
    /* the ECU says the sensor failed: ERR in amber, no bar */
    lv_obj_set_style_text_font(p->val, err ? &emu_txt_28 : p->num, 0);
    if (err) {
        lv_label_set_text(p->val, "SENSOR ERR");
        lv_obj_set_width(p->fill, 0);
        lv_obj_set_style_bg_color(p->panel, C_PANEL, 0);
        lv_obj_set_style_text_color(p->val, C_AMBER, 0);
        p->shown = NAN;
        return;
    }
    if (isnan(x)) {
        lv_label_set_text(p->val, "--");
        lv_obj_set_width(p->fill, 0);
        lv_obj_set_style_bg_color(p->panel, C_PANEL, 0);
        lv_obj_set_style_text_color(p->val, C_UNIT, 0);
        p->shown = NAN;
        return;
    }
    char buf[16];
    snprintf(buf, sizeof buf, c->fmt, x);
    if (buf[0] == '-' && atof(buf) == 0) {   /* no "-0.00" */
        snprintf(buf, sizeof buf, c->fmt, 0.0);
    }
    lv_label_set_text(p->val, buf);

    float f = (x - c->lo) / (c->hi - c->lo);
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    p->shown = isnan(p->shown) ? f : p->shown + (f - p->shown) * 0.35f;
    lv_obj_set_width(p->fill, (lv_coord_t)(p->shown * p->bar_w + 0.5f));

    level_t lv = level(c, x);
    lv_color_t bar = lv == LV_ALARM ? C_RED : lv == LV_WARN ? C_AMBER
                   : lv == LV_COLD ? C_COLD : C_GREEN;
    lv_obj_set_style_bg_color(p->fill, bar, 0);
    bool flash = lv == LV_ALARM && blink_on;
    lv_obj_set_style_bg_color(p->panel, flash ? C_ALARM : C_PANEL, 0);
    lv_obj_set_style_text_color(p->val,
        lv == LV_ALARM && !flash ? C_RED : C_LABEL, 0);
}

void ui_emu_update(const emu_view_t *v)
{
    s_frame++;
    bool blink_on = (s_frame / 8) % 2 == 0;       /* ~2 Hz at 30 Hz */
    draw(CH_BOOST, v->link ? v->boost : NAN, v->link && v->err_map, blink_on);
    draw(CH_AFR, v->link ? v->afr : NAN, v->link && v->err_wbo, blink_on);
    draw(CH_CLT, v->link ? v->clt : NAN, v->link && v->err_clt, blink_on);

    lv_label_set_text(s_link, v->link ? "CAN" : "NO DATA");
    lv_obj_set_style_text_color(s_link, v->link ? C_UNIT : C_RED, 0);
    lv_obj_set_style_bg_color(s_link_dot, v->link ? C_GREEN : C_RED, 0);
    lv_obj_align(s_link, LV_ALIGN_TOP_MID, 8, 38);
    lv_obj_align_to(s_link_dot, s_link, LV_ALIGN_OUT_LEFT_MID, -8, 0);
}
