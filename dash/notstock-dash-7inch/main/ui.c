/* NOT STOCK dash layout, 800x480, hand-built LVGL 8.x (no SquareLine).
 *
 * Classic analogue cluster: a big rev counter in the middle with the speed
 * as large text under it, water and intake air temperature on the left,
 * boost and AFR on the right. Every gauge has a needle and a digital
 * readout. No warning lamps: a value past
 * its limit turns its own readout red, and only revs flash the screen.
 *
 * All positions live in the LY_* block. The artwork itself (scales, zones,
 * needles, hubs) is pre-rendered by tools/gen_dials.py, which also writes the
 * matching geometry into dials.h, so nothing here has to be kept in step with
 * the images by hand. tools/preview.py renders this exact file on the PC.
 */
#include "ui.h"
#include "rusefi_can.h"
#include "settings.h"
#include "ui_menu.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_timer.h"

/* ------------------------------------------------------------------ colour */
#define C_Y        lv_color_hex(0xF5C518)
#define C_RED      lv_color_hex(0xE22424)
#define C_W        lv_color_hex(0xFFFFFF)
#define C_LBL      lv_color_hex(0xC8CBCE)
#define C_GREY     lv_color_hex(0x8A9096)
#define C_LINE     lv_color_hex(0x3A3D42)
#define C_SUBTLE   lv_color_hex(0x505357)

/* ---------------------------------------------------------------- geometry */
/* Rev counter. The image is BIG_SIZE square with its pivot in the centre. */
#define LY_RPM_CX     400
#define LY_RPM_CY     236
#define LY_RPM_VAL_Y  (LY_RPM_CY + 48)     /* rpm readout, under the hub */
#define LY_RPM_VAL_W  220

/* Speed, as text in the open bottom of the rev counter, between the 0 and 8
 * labels. */
#define LY_SPEED_Y    (LY_RPM_CY + 132)
#define LY_SPEED_W    240

/* Side columns: pivot of each small gauge. */
#define LY_LEFT_CX    60
#define LY_RIGHT_CX   740
#define LY_CLT_CY     112
#define LY_IAT_CY     352
#define LY_BOOST_CY   100
#define LY_AFR_CY     340
#define LY_SIDE_W     120

/* readouts relative to their pivot */
#define LY_TEMP_VAL_DY   26
#define LY_TEMP_ICON_DX  38                /* icon centre, right of the hub */
#define LY_TURBO_LBL_DY  18
#define LY_TURBO_VAL_DY  38
#define LY_SUB_DY        78                /* lambda line under the AFR */

#define LY_LINK_Y     4                    /* NO CAN / DEMO, top left */

/* ------------------------------------------------------------------ config */
#define NEEDLE_SMOOTH 0.25f                 /* 1.0 is instant, lower is lazier */

/* alarm flash */
#define FLASH_PERIOD_MS 420
/* boot screen: how long the logo stays, and the fade into the dash */
#define SPLASH_MS       2000
#define SPLASH_FADE_MS  700

/* hidden menu trigger, bottom right corner */
#define MENU_HIT_W    130
#define MENU_HIT_H    64

/* ------------------------------------------------------------------- state */
typedef struct {
    const lv_img_dsc_t *face;       /* pre-rendered scale */
    const lv_img_dsc_t *needle;
    lv_coord_t pivot_x, pivot_y;    /* needle pivot inside its image */
    const lv_img_dsc_t *hub;
    int sweep_start, sweep;         /* must match the face, from dials.h */
    float vmin, vmax;
    float scale_div;                /* raw value * scale_div = integer */
} dial_cfg_t;

typedef struct {
    lv_obj_t *meter;
    lv_meter_scale_t *scale;
    lv_meter_indicator_t *needle;
    lv_obj_t *value;
    lv_obj_t *icon;                 /* NULL when the gauge has none */
    lv_obj_t *sub;                  /* lambda line, or NULL */
    int32_t imin, imax;
    float scale_div;
    int val_dec;                    /* decimals on the readout */
    int val_round;                  /* readout rounded to this, 0 = off */
    float warn_above;               /* NAN when the gauge has no warn level */
    bool warn;
    float shown;                    /* smoothed value */
} gauge_t;

static lv_obj_t *lbl_speed;
static gauge_t g_rpm, g_clt, g_iat, g_boost, g_afr;
static lv_obj_t *scr_dash;
static lv_obj_t *link_txt;
static int link_state = -1;
static lv_obj_t *flash_layer;      /* red wash, on the top layer */
static lv_obj_t *knock_layer;      /* darkens the dash under the red wash */
static lv_obj_t *dim_layer;        /* software brightness, on the sys layer */
static bool alarm_on = false;
static bool alarm_shown = false;
static int64_t alarm_t0 = 0;

/* ------------------------------------------------------------------ helpers */
static lv_obj_t *mk_label(lv_obj_t *par, const lv_font_t *font, lv_color_t col,
                          const char *txt, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, lv_text_align_t align)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col, 0);
    lv_label_set_text(l, txt);
    if (w > 0) {
        lv_obj_set_width(l, w);
        lv_obj_set_style_text_align(l, align, 0);
    }
    lv_obj_set_pos(l, x, y);
    return l;
}

static lv_obj_t *mk_box(lv_obj_t *par, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *o = lv_obj_create(par);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *mk_img(lv_obj_t *par, const lv_img_dsc_t *src,
                        lv_coord_t cx, lv_coord_t cy)
{
    lv_obj_t *i = lv_img_create(par);
    lv_img_set_src(i, src);
    lv_obj_set_pos(i, cx - src->header.w / 2, cy - src->header.h / 2);
    return i;
}

/* A horizontal row that packs a number and a small unit on one baseline.
 * Flex handles the widths, so the pair stays centred whatever the digits. */
static lv_obj_t *mk_value_row(lv_obj_t *par, lv_coord_t x, lv_coord_t y,
                              lv_coord_t w, lv_coord_t h,
                              const lv_font_t *big, lv_color_t col,
                              const char *unit, lv_obj_t **out_value)
{
    lv_obj_t *row = mk_box(par, x, y, w, h);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 4, 0);
    *out_value = mk_label(row, big, col, "--", 0, 0, 0, LV_TEXT_ALIGN_LEFT);
    if (unit && unit[0]) {
        lv_obj_t *u = mk_label(row, &dash_lbl_13, C_GREY, unit, 0, 0, 0,
                               LV_TEXT_ALIGN_LEFT);
        lv_obj_set_style_pad_bottom(u, 4, 0);
    }
    return row;
}

/* Only touch a label when the text actually changed: LVGL invalidates the
 * area on every set_text, and at 25 Hz that is a lot of pointless redraw. */
static void set_text_if_changed(lv_obj_t *label, const char *txt)
{
    const char *cur = lv_label_get_text(label);
    if (cur == NULL || strcmp(cur, txt) != 0) {
        lv_label_set_text(label, txt);
    }
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ------------------------------------------------------------ dial builder */
/* The face is a plain image centred on the pivot. On top of it sits a
 * transparent lv_meter that only draws the needle, then the hub cap.
 *
 * The meter is not given the face as its background because lv_meter puts
 * its centre at (w/2, w/2) from its top-left corner, not in the middle of
 * the object. That only works for square faces, and the side gauges are not
 * square. A square meter centred on the pivot, just big enough for the
 * needle, works for every face. */
static void build_dial(gauge_t *g, lv_obj_t *par, lv_coord_t cx, lv_coord_t cy,
                       const dial_cfg_t *c)
{
    g->scale_div = c->scale_div;
    g->imin = (int32_t)lroundf(c->vmin * c->scale_div);
    g->imax = (int32_t)lroundf(c->vmax * c->scale_div);
    g->shown = c->vmin;
    g->warn = false;
    g->warn_above = NAN;
    g->icon = NULL;
    g->sub = NULL;

    mk_img(par, c->face, cx, cy);

    lv_coord_t side = 2 * c->needle->header.w;
    lv_obj_t *m = lv_meter_create(par);
    lv_obj_remove_style_all(m);
    lv_obj_set_size(m, side, side);
    lv_obj_set_pos(m, cx - side / 2, cy - side / 2);
    lv_obj_clear_flag(m, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(m, 0, LV_PART_MAIN);

    /* lv_meter would otherwise paint its own hub disc over the needle */
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_width(m, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(m, 0, LV_PART_INDICATOR);

    g->meter = m;
    g->scale = lv_meter_add_scale(m);
    lv_meter_set_scale_range(m, g->scale, g->imin, g->imax,
                             c->sweep, c->sweep_start);
    /* zero ticks: they are already in the baked image */
    lv_meter_set_scale_ticks(m, g->scale, 0, 0, 0, C_LINE);

    g->needle = lv_meter_add_needle_img(m, g->scale, c->needle,
                                        c->pivot_x, c->pivot_y);
    lv_meter_set_indicator_value(m, g->needle, g->imin);

    mk_img(par, c->hub, cx, cy);
}

static void set_warn(gauge_t *g, bool warn, lv_color_t normal)
{
    if (warn == g->warn) return;
    g->warn = warn;
    lv_obj_set_style_text_color(g->value, warn ? C_RED : normal, 0);
    if (g->icon) {
        lv_obj_set_style_img_recolor(g->icon, warn ? C_RED : C_Y, 0);
    }
}

static void update_gauge(gauge_t *g, float raw, lv_color_t normal)
{
    g->shown += (raw - g->shown) * NEEDLE_SMOOTH;

    int32_t iv = (int32_t)lroundf(g->shown * g->scale_div);
    if (iv < g->imin) iv = g->imin;
    if (iv > g->imax) iv = g->imax;
    lv_meter_set_indicator_value(g->meter, g->needle, iv);

    set_warn(g, !isnan(g->warn_above) && g->shown >= g->warn_above, normal);

    char buf[16];
    if (g->val_dec == 2)      snprintf(buf, sizeof buf, "%.2f", g->shown);
    else if (g->val_dec == 1) snprintf(buf, sizeof buf, "%.1f", g->shown);
    else if (g->val_round > 1)
        snprintf(buf, sizeof buf, "%d",
                 (int)lroundf(g->shown / g->val_round) * g->val_round);
    else                      snprintf(buf, sizeof buf, "%d",
                                       (int)lroundf(g->shown));
    set_text_if_changed(g->value, buf);

    if (g->sub) {
        /* UTF-8 lambda, the glyph is in dash_lbl_13 */
        snprintf(buf, sizeof buf, "\xCE\xBB %.2f", g->shown / set_stoich());
        set_text_if_changed(g->sub, buf);
    }
}

/* ------------------------------------------------ rev counter and speed */
static void build_rpm(lv_obj_t *par)
{
    const dial_cfg_t c = {
        .face = &dial_rpm, .needle = &needle_big,
        .pivot_x = NEEDLE_BIG_PIVOT_X, .pivot_y = NEEDLE_BIG_PIVOT_Y,
        .hub = &hub_big,
        .sweep_start = BIG_SWEEP_START, .sweep = BIG_SWEEP,
        .vmin = 0, .vmax = RPM_MAX, .scale_div = 1.0f,
    };
    build_dial(&g_rpm, par, LY_RPM_CX, LY_RPM_CY, &c);
    g_rpm.val_dec = 0;
    g_rpm.val_round = 10;

    /* the readout sits in the open bottom of the scale, between 0 and 8 */
    lv_obj_t *row = mk_box(par, LY_RPM_CX - LY_RPM_VAL_W / 2, LY_RPM_VAL_Y,
                           LY_RPM_VAL_W, 50);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);
    g_rpm.value = mk_label(row, &dash_orb_40, C_W, "0", 0, 0, 0,
                           LV_TEXT_ALIGN_LEFT);
    lv_obj_t *u = mk_label(row, &dash_orb_18, C_GREY, "rpm", 0, 0, 0,
                           LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_pad_bottom(u, 6, 0);
}

/* Speed is text only: a number, no needle. */
static void build_speed(lv_obj_t *par)
{
    lv_obj_t *row = mk_box(par, LY_RPM_CX - LY_SPEED_W / 2, LY_SPEED_Y,
                           LY_SPEED_W, 72);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);
    lbl_speed = mk_label(row, &dash_speed_56, C_W, "0", 0, 0, 0,
                         LV_TEXT_ALIGN_LEFT);
    lv_obj_t *u = mk_label(row, &dash_orb_18, C_GREY, "km/h", 0, 0, 0,
                           LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_pad_bottom(u, 14, 0);
}

static void update_speed(float kmh)
{
    char buf[8];
    snprintf(buf, sizeof buf, "%d", (int)lroundf(clampf(kmh, 0, 999)));
    set_text_if_changed(lbl_speed, buf);
}

/* ---------------------------------------------------------- side gauges */
/* Water and intake air: short arc on top, icon next to the hub, number
 * below. */
static void build_temp(gauge_t *g, lv_obj_t *par, lv_coord_t cy,
                       const lv_img_dsc_t *face, float vmin, float vmax,
                       const lv_img_dsc_t *icon)
{
    const dial_cfg_t c = {
        .face = face, .needle = &needle_temp,
        .pivot_x = NEEDLE_TEMP_PIVOT_X, .pivot_y = NEEDLE_TEMP_PIVOT_Y,
        .hub = &hub_temp,
        .sweep_start = TEMP_SWEEP_START, .sweep = TEMP_SWEEP,
        .vmin = vmin, .vmax = vmax, .scale_div = 10.0f,
    };
    build_dial(g, par, LY_LEFT_CX, cy, &c);
    g->val_dec = 0;
    g->val_round = 0;

    /* Icons are placed by their centre, read from the image header, so
     * swapping in a different pixel size needs no coordinate edits. */
    g->icon = mk_img(par, icon, LY_LEFT_CX + LY_TEMP_ICON_DX, cy);
    lv_obj_set_style_img_recolor(g->icon, C_Y, 0);
    lv_obj_set_style_img_recolor_opa(g->icon, LV_OPA_COVER, 0);

    mk_value_row(par, LY_LEFT_CX - LY_SIDE_W / 2, cy + LY_TEMP_VAL_DY,
                 LY_SIDE_W, 40, &dash_num_28, C_Y, "\xC2\xB0" "C", &g->value);
}

/* Boost and AFR: long sweep with coloured zones, title under the hub, number
 * below that. */
static void build_turbo(gauge_t *g, lv_obj_t *par, lv_coord_t cy,
                        const lv_img_dsc_t *face, float vmin, float vmax,
                        const char *title, const char *unit, int val_dec)
{
    const dial_cfg_t c = {
        .face = face, .needle = &needle_turbo,
        .pivot_x = NEEDLE_TURBO_PIVOT_X, .pivot_y = NEEDLE_TURBO_PIVOT_Y,
        .hub = &hub_turbo,
        .sweep_start = TURBO_SWEEP_START, .sweep = TURBO_SWEEP,
        .vmin = vmin, .vmax = vmax, .scale_div = 100.0f,
    };
    build_dial(g, par, LY_RIGHT_CX, cy, &c);
    g->val_dec = val_dec;
    g->val_round = 0;

    mk_label(par, &dash_lbl_13, C_LBL, title, LY_RIGHT_CX - LY_SIDE_W / 2,
             cy + LY_TURBO_LBL_DY, LY_SIDE_W, LV_TEXT_ALIGN_CENTER);
    mk_value_row(par, LY_RIGHT_CX - LY_SIDE_W / 2, cy + LY_TURBO_VAL_DY,
                 LY_SIDE_W, 40, &dash_num_28, C_Y, unit, &g->value);
}

/* ----------------------------------------------------------- link status */
/* Not a warning lamp: a line of text at the top that is only there while the
 * data is not live, so frozen needles are never mistaken for real ones. */
static void update_link(int state)
{
    if (state == link_state) return;
    link_state = state;
    switch (state) {
    case 0:
        lv_obj_clear_flag(link_txt, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(link_txt, C_RED, 0);
        lv_label_set_text(link_txt, "NO CAN");
        break;
    case 1:
        lv_obj_add_flag(link_txt, LV_OBJ_FLAG_HIDDEN);
        break;
    default:
        lv_obj_clear_flag(link_txt, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(link_txt, C_Y, 0);
        lv_label_set_text(link_txt, "DEMO");
        break;
    }
}

/* --------------------------------------------------------------- demo feed */
static void demo_fill(dash_data_t *d)
{
    float t = (float)esp_timer_get_time() / 1000000.0f;
    float cyc = fmodf(t, 7.0f) / 7.0f;
    float rpm = 900.0f + 6600.0f * powf(sinf(cyc * (float)M_PI), 1.4f);
    float boost = -0.6f + 2.0f *
                  powf(clampf((rpm - 1800.0f) / 4200.0f, 0.0f, 1.0f), 1.2f);

    d->rpm   = rpm;
    d->boost = boost;
    d->speed = clampf(rpm / 8000.0f * 200.0f, 0, 255);
    d->afr   = boost > 0.4f ? 11.6f + 0.5f * sinf(t * 3.0f)
                            : 14.4f + 0.8f * sinf(t * 1.7f);
    d->clt   = 88.0f + 6.0f * sinf(t / 9.0f);
    d->iat   = 30.0f + 18.0f * clampf(boost, 0, 2);
}

/* ------------------------------------------------------------- alarm flash */
/* A red wash over everything, on LVGL's top layer so it covers the dash but
 * sits under the brightness dim. It is not clickable, so the menu still
 * works while an alarm is going off. */
static void build_overlays(void)
{
    /* Drawn first, so it sits under the red. A plain translucent red over a
     * white-on-black dash turns pink; knocking the picture back first keeps
     * the red saturated at high intensities. */
    knock_layer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(knock_layer);
    lv_obj_set_size(knock_layer, 800, 480);
    lv_obj_set_pos(knock_layer, 0, 0);
    lv_obj_set_style_bg_color(knock_layer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(knock_layer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(knock_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(knock_layer, LV_OBJ_FLAG_HIDDEN);

    flash_layer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(flash_layer);
    lv_obj_set_size(flash_layer, 800, 480);
    lv_obj_set_pos(flash_layer, 0, 0);
    lv_obj_set_style_bg_color(flash_layer, C_RED, 0);
    lv_obj_set_style_bg_opa(flash_layer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(flash_layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(flash_layer, LV_OBJ_FLAG_HIDDEN);

    /* EXIO2 on this board is a plain display-enable line with no PWM, so
     * brightness has to be faked with a black wash. It never goes fully
     * opaque, hence the 15 % floor on the setting. */
    dim_layer = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(dim_layer);
    lv_obj_set_size(dim_layer, 800, 480);
    lv_obj_set_pos(dim_layer, 0, 0);
    lv_obj_set_style_bg_color(dim_layer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(dim_layer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(dim_layer, LV_OBJ_FLAG_CLICKABLE);
}

/* The full-screen flash is a shift light and nothing else. Every other limit
 * turns its own readout red, which is enough for a temperature or a pressure:
 * those creep, and a strobing screen while you are trying to read the number
 * that caused it is worse than useless. Revs are the one case where the
 * driver has to react inside a second and cannot be looking at the panel. */
static bool alarm_active(const dash_data_t *d)
{
    if (!g_set.flash_enable) return false;
    if (!g_set.rpm_flash) return false;
    return d->rpm >= g_set.rpm_flash;
}

static void update_alarm(const dash_data_t *d)
{
    bool on = alarm_active(d);
    if (on != alarm_on) {
        alarm_on = on;
        alarm_t0 = esp_timer_get_time();
        if (!on) {
            lv_obj_add_flag(flash_layer, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(knock_layer, LV_OBJ_FLAG_HIDDEN);
            alarm_shown = false;
        } else {
            lv_obj_clear_flag(flash_layer, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(knock_layer, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (!on) return;

    /* square wave rather than a fade: it is far more noticeable in daylight
     * and costs one opacity write per half period instead of one per frame */
    int64_t ms = (esp_timer_get_time() - alarm_t0) / 1000;
    bool phase = ((ms / (FLASH_PERIOD_MS / 2)) & 1) == 0;
    if (phase != alarm_shown) {
        alarm_shown = phase;
        int op = g_set.flash_intensity * 255 / 100;
        lv_obj_set_style_bg_opa(flash_layer,
            phase ? (lv_opa_t)op : LV_OPA_TRANSP, 0);
        /* the knock-back scales with intensity, so a low setting still
         * leaves the dash readable through the flash */
        lv_obj_set_style_bg_opa(knock_layer,
            phase ? (lv_opa_t)(op * 3 / 5) : LV_OPA_TRANSP, 0);
    }
}

/* ------------------------------------------------------------- menu access */
static void menu_cb(lv_event_t *e)
{
    (void)e;
    ui_menu_refresh();
    lv_scr_load(ui_menu_screen());
}

static void build_menu_hit(lv_obj_t *par)
{
    /* Deliberately invisible and in the far bottom-right corner, and it needs
     * a long press. Nothing about normal driving should ever open it. */
    lv_obj_t *hit = mk_box(par, 800 - MENU_HIT_W, 480 - MENU_HIT_H,
                           MENU_HIT_W, MENU_HIT_H);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hit, menu_cb, LV_EVENT_LONG_PRESSED, NULL);
    /* nothing above it may swallow the press */
    lv_obj_move_foreground(hit);

    /* the only hint it exists: three dim dots */
    for (int i = 0; i < 3; i++) {
        lv_obj_t *dot = mk_box(hit, MENU_HIT_W - 30 + i * 8,
                               MENU_HIT_H - 16, 3, 3);
        lv_obj_set_style_bg_color(dot, C_LINE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    }
}

void ui_show_dash(void)
{
    lv_scr_load(scr_dash);
}

void ui_apply_settings(void)
{
    g_rpm.warn_above   = RPM_REDLINE;
    g_clt.warn_above   = g_set.clt_warn ? g_set.clt_warn : NAN;
    g_iat.warn_above   = g_set.iat_warn ? g_set.iat_warn : NAN;
    g_boost.warn_above = g_set.boost_warn ? set_boost_warn() : NAN;
    g_afr.warn_above   = g_set.afr_lean_warn ? g_set.afr_lean_warn / 10.0f
                                             : NAN;

    lv_obj_set_style_bg_opa(dim_layer,
        (lv_opa_t)((100 - g_set.brightness) * 255 / 100), 0);

    if (!g_set.flash_enable && flash_layer) {
        lv_obj_add_flag(flash_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(knock_layer, LV_OBJ_FLAG_HIDDEN);
        alarm_on = false;
    }
}

/* ------------------------------------------------------------------- timer */
static void ui_timer_cb(lv_timer_t *t)
{
    (void)t;
    dash_data_t d;

    if (g_set.demo) {
        memset(&d, 0, sizeof d);
        demo_fill(&d);
        update_link(2);
    } else {
        memcpy(&d, (const void *)&g_dash, sizeof d);
        update_link(rusefi_can_link_ok() ? 1 : 0);
    }

    update_speed(d.speed);
    update_gauge(&g_rpm,   clampf(d.rpm, 0, RPM_MAX), C_W);
    update_gauge(&g_clt,   clampf(d.clt, -40, 150), C_Y);
    update_gauge(&g_iat,   clampf(d.iat, -40, 150), C_Y);
    update_gauge(&g_boost, clampf(d.boost, -1.2f, 2.5f), C_Y);
    update_gauge(&g_afr,   clampf(d.afr, 9.0f, 19.0f), C_Y);
    update_alarm(&d);
}

/* ------------------------------------------------------------ boot screen */
static void splash_done_cb(lv_timer_t *t)
{
    lv_timer_del(t);
    /* auto_del frees the splash screen; the image itself lives in flash */
    lv_scr_load_anim(scr_dash, LV_SCR_LOAD_ANIM_FADE_ON, SPLASH_FADE_MS, 0,
                     true);
}

/* The logo on black, shown from the first frame until SPLASH_MS. The dash is
 * already built and updating underneath, so it comes up with live values. */
static void build_splash(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_remove_style_all(s);
    lv_obj_set_style_bg_color(s, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    mk_img(s, &splash_logo, 400, 240);
    lv_scr_load(s);
    lv_timer_create(splash_done_cb, SPLASH_MS, NULL);
}

/* ------------------------------------------------------------------- build */
void ui_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    scr_dash = scr;
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    build_rpm(scr);
    build_speed(scr);

    build_temp(&g_clt, scr, LY_CLT_CY, &dial_clt, CLT_MIN, CLT_MAX, &ic_water);
    build_temp(&g_iat, scr, LY_IAT_CY, &dial_iat, IAT_MIN, IAT_MAX, &ic_iat);

    build_turbo(&g_boost, scr, LY_BOOST_CY, &dial_boost, BOOST_MIN, BOOST_MAX,
                "TURBO", "bar", 2);
    build_turbo(&g_afr, scr, LY_AFR_CY, &dial_afr, AFR_MIN, AFR_MAX,
                "AFR", "", 1);
    g_afr.sub = mk_label(scr, &dash_lbl_13, C_GREY, "",
                         LY_RIGHT_CX - LY_SIDE_W / 2, LY_AFR_CY + LY_SUB_DY,
                         LY_SIDE_W, LV_TEXT_ALIGN_CENTER);

    link_txt = mk_label(scr, &dash_lbl_18, C_RED, "", 0, LY_LINK_Y,
                        LY_SIDE_W, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(link_txt, LV_OBJ_FLAG_HIDDEN);

    build_menu_hit(scr);
    build_overlays();

    ui_menu_create();
    ui_apply_settings();

    lv_timer_create(ui_timer_cb, 40, NULL);
    build_splash();
}
