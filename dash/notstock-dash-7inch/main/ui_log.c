/* LOG screen, see ui_log.h.
 *
 * Every channel is stored already scaled to 0..1000 of its own range, so all
 * of them share one chart axis and each fills the height. The buttons along
 * the top carry the live value in real units and switch their line on and
 * off; the selection is saved.
 */
#include "ui_log.h"
#include "settings.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define LOG_HZ        10
#define LOG_POINTS    300                   /* 30 s at 10 Hz */
#define LOG_REFRESH_MS 200                  /* chart redraw while visible */
#define SCALE         1000

#define C_BG       lv_color_hex(0x000000)
#define C_PANEL    lv_color_hex(0x0B0C0E)
#define C_GRID     lv_color_hex(0x1E2124)
#define C_LINE     lv_color_hex(0x3A3D42)
#define C_OFF      lv_color_hex(0x5A5E63)
#define C_W        lv_color_hex(0xFFFFFF)
#define C_Y        lv_color_hex(0xF5C518)
#define C_BTN      lv_color_hex(0x16181B)

typedef struct {
    const char *name;
    float lo, hi;              /* chart range, real units */
    int decimals;
    uint32_t colour;
} channel_t;

enum { CH_RPM, CH_MAP, CH_CLT, CH_IAT, CH_AFR, CH_DUTY, CH_IGN, CH_TPS,
       CH_COUNT };

/* Order is the button order and the bit order of g_set.log_mask. */
static const channel_t CH[CH_COUNT] = {
    [CH_RPM]  = { "RPM",  0, 8000, 0, 0xFFFFFF },
    [CH_MAP]  = { "MAP",  0,  300, 0, 0xF5C518 },   /* kPa */
    [CH_CLT]  = { "CLT",  0,  130, 0, 0xFF5A36 },   /* degC */
    [CH_IAT]  = { "IAT",  0,   80, 0, 0x3DA5FF },   /* degC */
    [CH_AFR]  = { "AFR", 10,   20, 1, 0x3DDC84 },
    [CH_DUTY] = { "DUTY", 0,  100, 0, 0xC77DFF },   /* injector duty % */
    [CH_IGN]  = { "IGN", -10,  50, 1, 0xFF9A00 },   /* timing, deg BTDC */
    [CH_TPS]  = { "TPS",  0,  100, 0, 0x7FDBDA },   /* % */
};

static float channel_value(const dash_data_t *d, int ch)
{
    switch (ch) {
    case CH_RPM:  return d->rpm;
    case CH_MAP:  return d->map;
    case CH_CLT:  return d->clt;
    case CH_IAT:  return d->iat;
    case CH_AFR:  return d->afr;
    case CH_DUTY: return d->injduty;
    case CH_IGN:  return d->timing;
    default:      return d->tps;
    }
}

/* ring buffer, oldest at s_head once full */
static lv_coord_t s_ring[CH_COUNT][LOG_POINTS];
static int s_head, s_count;
static float s_last[CH_COUNT];
static int64_t s_next_us;

static lv_obj_t *scr_log, *chart, *hold_btn, *hold_lbl, *time_lbl;
static lv_chart_series_t *ser[CH_COUNT];
static lv_coord_t ser_buf[CH_COUNT][LOG_POINTS];
static lv_obj_t *btn[CH_COUNT], *btn_name[CH_COUNT], *btn_val[CH_COUNT];
static bool s_hold;

static lv_coord_t scale_to_chart(int ch, float v)
{
    float f = (v - CH[ch].lo) / (CH[ch].hi - CH[ch].lo);
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    return (lv_coord_t)(f * SCALE + 0.5f);
}

void ui_log_sample(const dash_data_t *d, int64_t now_us)
{
    for (int c = 0; c < CH_COUNT; c++) s_last[c] = channel_value(d, c);
    if (now_us < s_next_us) return;
    s_next_us = (s_next_us == 0 ? now_us : s_next_us) + 1000000 / LOG_HZ;
    if (s_next_us < now_us) s_next_us = now_us;     /* fell behind, resync */

    for (int c = 0; c < CH_COUNT; c++) {
        s_ring[c][s_head] = scale_to_chart(c, s_last[c]);
    }
    s_head = (s_head + 1) % LOG_POINTS;
    if (s_count < LOG_POINTS) s_count++;
}

/* ------------------------------------------------------------------ view */
static bool channel_on(int c)
{
    return (g_set.log_mask >> c) & 1;
}

static void paint_button(int c)
{
    bool on = channel_on(c);
    lv_color_t col = lv_color_hex(CH[c].colour);
    lv_obj_set_style_border_color(btn[c], on ? col : C_LINE, 0);
    lv_obj_set_style_text_color(btn_name[c], on ? col : C_OFF, 0);
    lv_obj_set_style_text_color(btn_val[c], on ? C_W : C_OFF, 0);
}

static void refresh_chart(void)
{
    /* newest sample on the right edge, empty points to the left of it */
    int missing = LOG_POINTS - s_count;
    int start = (s_head - s_count + LOG_POINTS) % LOG_POINTS;
    for (int c = 0; c < CH_COUNT; c++) {
        if (!channel_on(c)) continue;
        for (int i = 0; i < LOG_POINTS; i++) {
            ser_buf[c][i] = i < missing
                ? LV_CHART_POINT_NONE
                : s_ring[c][(start + i - missing) % LOG_POINTS];
        }
    }
    lv_chart_refresh(chart);
}

static void refresh_values(void)
{
    char b[16];
    for (int c = 0; c < CH_COUNT; c++) {
        if (CH[c].decimals) snprintf(b, sizeof b, "%.1f", s_last[c]);
        else                snprintf(b, sizeof b, "%d", (int)(s_last[c] + (s_last[c] < 0 ? -0.5f : 0.5f)));
        const char *cur = lv_label_get_text(btn_val[c]);
        if (!cur || strcmp(cur, b) != 0) lv_label_set_text(btn_val[c], b);
    }
}

static void log_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (lv_scr_act() != scr_log) return;
    refresh_values();
    if (!s_hold) refresh_chart();
}

static void channel_cb(lv_event_t *e)
{
    int c = (int)(intptr_t)lv_event_get_user_data(e);
    g_set.log_mask ^= (uint8_t)(1u << c);
    lv_chart_hide_series(chart, ser[c], !channel_on(c));
    paint_button(c);
    refresh_chart();
    settings_save();
}

static void hold_cb(lv_event_t *e)
{
    (void)e;
    s_hold = !s_hold;
    lv_label_set_text(hold_lbl, s_hold ? "LIVE" : "HOLD");
    lv_obj_set_style_border_color(hold_btn, s_hold ? C_Y : C_LINE, 0);
    lv_label_set_text(time_lbl, s_hold ? "30 s  -  HELD" : "30 s");
    if (!s_hold) refresh_chart();
}

static void dash_cb(lv_event_t *e)
{
    (void)e;
    ui_show_dash();
}

static lv_obj_t *mk_btn(lv_obj_t *par, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *b = lv_obj_create(par);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, C_BTN, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, C_LINE, 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    return b;
}

static lv_obj_t *mk_text(lv_obj_t *par, const lv_font_t *f, lv_color_t col,
                         const char *txt)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_label_set_text(l, txt);
    return l;
}

void ui_log_create(void)
{
    scr_log = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr_log);
    lv_obj_set_style_bg_color(scr_log, C_BG, 0);
    lv_obj_set_style_bg_opa(scr_log, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_log, LV_OBJ_FLAG_SCROLLABLE);

    /* top bar */
    lv_obj_t *title = mk_text(scr_log, &dash_orb_18, C_Y, "LOG");
    lv_obj_set_style_text_letter_space(title, 3, 0);
    lv_obj_set_pos(title, 12, 14);

    time_lbl = mk_text(scr_log, &dash_orb_14, C_OFF, "30 s");
    lv_obj_set_pos(time_lbl, 80, 17);

    hold_btn = mk_btn(scr_log, 520, 6, 120, 38);
    hold_lbl = mk_text(hold_btn, &dash_orb_14, C_W, "HOLD");
    lv_obj_center(hold_lbl);
    lv_obj_add_event_cb(hold_btn, hold_cb, LV_EVENT_CLICKED, NULL);

    /* same corner that opened the log */
    lv_obj_t *back = mk_btn(scr_log, 652, 6, 140, 38);
    lv_obj_t *bl = mk_text(back, &dash_orb_14, C_W, "DASH");
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, dash_cb, LV_EVENT_CLICKED, NULL);

    /* channel buttons: name in the channel colour, live value under it */
    const lv_coord_t bw = 94, bh = 56, gap = 4, x0 = 8, y0 = 52;
    for (int c = 0; c < CH_COUNT; c++) {
        btn[c] = mk_btn(scr_log, x0 + c * (bw + gap), y0, bw, bh);
        btn_name[c] = mk_text(btn[c], &dash_orb_14, C_OFF, CH[c].name);
        lv_obj_align(btn_name[c], LV_ALIGN_TOP_MID, 0, 6);
        btn_val[c] = mk_text(btn[c], &dash_orb_18, C_OFF, "-");
        lv_obj_align(btn_val[c], LV_ALIGN_BOTTOM_MID, 0, -6);
        lv_obj_add_event_cb(btn[c], channel_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)c);
    }

    /* chart */
    chart = lv_chart_create(scr_log);
    lv_obj_set_pos(chart, 8, 118);
    lv_obj_set_size(chart, 784, 354);
    lv_obj_clear_flag(chart, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, LOG_POINTS);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, SCALE);
    lv_chart_set_div_line_count(chart, 5, 7);      /* a line every 5 s */
    lv_obj_set_style_bg_color(chart, C_PANEL, 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chart, C_LINE, 0);
    lv_obj_set_style_border_width(chart, 1, 0);
    lv_obj_set_style_radius(chart, 6, 0);
    lv_obj_set_style_pad_all(chart, 6, 0);
    lv_obj_set_style_line_color(chart, C_GRID, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);    /* no dots */

    for (int c = 0; c < CH_COUNT; c++) {
        for (int i = 0; i < LOG_POINTS; i++) ser_buf[c][i] = LV_CHART_POINT_NONE;
        ser[c] = lv_chart_add_series(chart, lv_color_hex(CH[c].colour),
                                     LV_CHART_AXIS_PRIMARY_Y);
        lv_chart_set_ext_y_array(chart, ser[c], ser_buf[c]);
        lv_chart_hide_series(chart, ser[c], !channel_on(c));
        paint_button(c);
    }

    lv_timer_create(log_timer_cb, LOG_REFRESH_MS, NULL);
}

lv_obj_t *ui_log_screen(void)
{
    refresh_values();
    if (!s_hold) refresh_chart();
    return scr_log;
}
