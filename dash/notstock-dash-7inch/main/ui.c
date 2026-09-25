/* NOT STOCK dash layout, 800x480, hand-built LVGL 8.x (no SquareLine).
 *
 * All geometry lives in the LY_* block. Everything else derives from it, so
 * moving a block is a one-line change. tools/preview.py mirrors these same
 * constants and renders a PNG, which is how the layout gets checked without
 * flashing.
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
#define C_YDIM     lv_color_hex(0x7A630E)
#define C_RED      lv_color_hex(0xE22424)
#define C_REDDIM   lv_color_hex(0x3D0D0D)
#define C_W        lv_color_hex(0xFFFFFF)
#define C_LBL      lv_color_hex(0xC8CBCE)
#define C_GREY     lv_color_hex(0x8A9096)
#define C_DIM      lv_color_hex(0x242629)
#define C_BANDDIM  lv_color_hex(0x33363A)
#define C_LINE     lv_color_hex(0x3A3D42)
#define C_CARDBG   lv_color_hex(0x0D0E10)
#define C_HUB      lv_color_hex(0x0A0A0A)
#define C_HUBED    lv_color_hex(0x444444)
#define C_TICK     lv_color_hex(0xECECEC)
#define C_TICKMIN  lv_color_hex(0x6E7276)
#define C_TICKLBL  lv_color_hex(0xA8ADB2)
#define C_OFF      lv_color_hex(0x404347)
#define C_GREEN    lv_color_hex(0x25C25A)
#define C_SUBTLE   lv_color_hex(0x505357)

/* ---------------------------------------------------------------- geometry */
#define LY_M         6                       /* outer margin */

#define LY_GAUGE_W   250
#define LY_GX_L      LY_M
#define LY_GX_R      (800 - LY_M - LY_GAUGE_W)
#define LY_TITLE_Y   4
#define LY_METER     202
#define LY_METER_Y   26
#define LY_VAL_Y     222                     /* readout, below the dial */
#define LY_SUB_Y     274

/* The dial face, its band, ticks and scale labels are pre-rendered by
 * tools/gen_dials.py and blitted as one image. Only the needle and the live
 * fill arc are drawn by LVGL. These two constants have to agree with the
 * R_BAND_OUT and BAND_W in that script. */
#define LY_BAND_W    7
#define LY_BAND_MOD  (-3)      /* band outer edge = LY_METER/2 + this */

#define LY_MID_X     (LY_GX_L + LY_GAUGE_W + 10)
#define LY_MID_W     (LY_GX_R - LY_MID_X - 10)
#define LY_RPM_SCALE_Y 36
#define LY_RPM_BAR_Y   54
#define LY_RPM_BAR_H   30
#define LY_RPM_NUM_Y   92
#define LY_RULE_Y      178
#define LY_SPEED_LBL_Y 188
#define LY_SPEED_ROW_Y 214

#define LY_CARD_Y    302
#define LY_CARD_H    104
#define LY_CARD_STEP ((800 - 2 * LY_M + 8) / 5)
#define LY_CARD_W    (LY_CARD_STEP - 8)
/* Icon slot. The supplied artwork is up to 44x30, so the label starts clear
 * of the widest one and every icon is centred on the same point regardless of
 * its own size. */
#define LY_ICON_CX   25
#define LY_ICON_CY   23
#define LY_ICON_LBL  51

#define LY_BAR_Y     418
#define LY_BAR_H     46

/* ------------------------------------------------------------------ config */
/* The strip must sit on an integer grid or the pitch alternates between two
 * values and the row reads as uneven. 33 segments at a pitch of 8 fills the
 * middle column exactly, and the nine scale numbers land every 33 px. */
#define RPM_SEGS      33
#define RPM_PITCH     8
#define RPM_SEG_W     6
#define RPM_X0        (LY_MID_X + 2)
#define RPM_LBL_STEP  33
#define NEEDLE_SMOOTH 0.25f

/* alarm flash */
#define FLASH_PERIOD_MS 420
/* hidden menu trigger, bottom right corner of the bar */
#define MENU_HIT_W    130
#define MENU_HIT_H    64

/* ------------------------------------------------------------------- state */
typedef struct {
    lv_obj_t *meter;
    lv_meter_scale_t *scale;
    lv_meter_indicator_t *needle;
    lv_meter_indicator_t *fill;      /* NULL when the gauge has no fill arc */
    lv_obj_t *row;                   /* holds value + inline unit */
    lv_obj_t *value;
    lv_obj_t *sub;                   /* lambda line, or nothing */
    int32_t imin, imax;              /* scaled integer range */
    float scale_div;                 /* raw value * scale_div = integer */
    int val_dec;                     /* decimals on the big readout */
    bool show_lambda;
    float warn_above;                /* NAN when the gauge has no warn level */
    bool warn;
    float shown;                     /* smoothed value */
} gauge_t;

typedef struct {
    const char *label;
    const char *unit;
    const lv_img_dsc_t *icon;
    float lo, hi;
    int decimals;
    /* Thresholds are not stored here: they live in g_set so the driver can
     * change them from the menu. These two say which way the limit applies. */
    bool warn_high;
    bool warn_low;
} card_cfg_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *icon;
    lv_obj_t *bar;
    lv_obj_t *value;
    bool warn;
    bool armed;                      /* low-limit tiles, see update_cards */
    int32_t shown;
} card_t;

static gauge_t g_boost, g_afr;
static lv_obj_t *lbl_rpm, *lbl_speed;
static lv_obj_t *rpm_seg[RPM_SEGS];
static bool rpm_on[RPM_SEGS];
static int rpm_lit = -1;
static card_t cards[5];
static lv_obj_t *st_val[2], *st_icon[2];
static int st_state[2] = { -1, -1 };
static lv_obj_t *link_dot, *link_txt;
static int link_state = -1;
static lv_obj_t *scr_dash;
static lv_obj_t *flash_layer;      /* red wash, on the top layer */
static lv_obj_t *dim_layer;        /* software brightness, on the sys layer */
static lv_obj_t *rpm_scale_lbl[9];
static bool alarm_on = false;
static bool alarm_shown = false;
static int64_t alarm_t0 = 0;
static lv_obj_t *knock_layer;   /* darkens the dash under the red wash */

static const card_cfg_t card_cfg[5] = {
    { "WATER",      "\xC2\xB0" "C", &ic_water,    0, 120, 0, true,  false },
    { "OIL TEMP",   "\xC2\xB0" "C", &ic_oiltemp,  0, 150, 0, true,  false },
    { "OIL PRESS",  "bar",          &ic_oilpress, 0,   8, 1, false, true  },
    { "IAT",        "\xC2\xB0" "C", &ic_iat,      0,  80, 0, true,  false },
    { "FUEL PRESS", "bar",          &ic_fuel,     0,   6, 1, false, true  },
};

/* current limit for a card, read live from settings */
static float card_limit(int i)
{
    switch (i) {
    case 0: return g_set.clt_warn;
    case 1: return g_set.oilt_warn;
    case 2: return set_oilp_warn();
    case 3: return g_set.iat_warn;
    default: return set_fuelp_warn();
    }
}

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

static lv_obj_t *mk_panel(lv_obj_t *par, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *o = mk_box(par, x, y, w, h);
    lv_obj_set_style_border_color(o, C_LINE, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_radius(o, 7, 0);
    return o;
}

static lv_obj_t *mk_seg(lv_obj_t *par, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h, lv_color_t col)
{
    lv_obj_t *o = mk_box(par, x, y, w, h);
    lv_obj_set_style_bg_color(o, col, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 1, 0);
    return o;
}

static lv_obj_t *mk_icon(lv_obj_t *par, const lv_img_dsc_t *src,
                         lv_color_t col)
{
    lv_obj_t *i = lv_img_create(par);
    lv_img_set_src(i, src);
    lv_obj_set_style_img_recolor(i, col, 0);
    lv_obj_set_style_img_recolor_opa(i, LV_OPA_COVER, 0);
    return i;
}

/* A horizontal row that packs a big number and a small unit on one baseline.
 * Flex handles the widths, so the pair stays centred whatever the digits. */
static lv_obj_t *mk_value_row(lv_obj_t *par, lv_coord_t x, lv_coord_t y,
                              lv_coord_t w, lv_coord_t h,
                              const lv_font_t *big, const char *unit,
                              lv_flex_align_t main_align,
                              lv_obj_t **out_value)
{
    lv_obj_t *row = mk_box(par, x, y, w, h);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, main_align, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);
    *out_value = mk_label(row, big, C_W, "--", 0, 0, 0, LV_TEXT_ALIGN_LEFT);
    if (unit && unit[0]) {
        lv_obj_t *u = mk_label(row, &dash_lbl_13, C_GREY, unit, 0, 0, 0,
                               LV_TEXT_ALIGN_LEFT);
        lv_obj_set_style_pad_bottom(u, 5, 0);
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

/* ----------------------------------------------------------- gauge builder */
typedef struct {
    const char *title;
    const char *unit;
    const lv_img_dsc_t *face;   /* pre-rendered dial, see tools/gen_dials.py */
    float vmin, vmax;
    float scale_div;
    int val_dec;
    bool has_fill;
    bool show_lambda;
    float warn_above;
} gauge_cfg_t;

static void build_gauge(gauge_t *g, lv_obj_t *par, lv_coord_t bx,
                        const gauge_cfg_t *c)
{
    g->scale_div   = c->scale_div;
    g->val_dec     = c->val_dec;
    g->show_lambda = c->show_lambda;
    g->warn_above  = c->warn_above;
    g->warn        = false;
    g->imin = (int32_t)lroundf(c->vmin * c->scale_div);
    g->imax = (int32_t)lroundf(c->vmax * c->scale_div);
    g->shown = c->vmin;

    mk_label(par, &dash_lbl_18, C_Y, c->title,
             bx, LY_TITLE_Y, LY_GAUGE_W, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *m = lv_meter_create(par);
    lv_obj_remove_style_all(m);
    lv_obj_set_size(m, LY_METER, LY_METER);
    lv_obj_set_pos(m, bx + (LY_GAUGE_W - LY_METER) / 2, LY_METER_Y);
    lv_obj_clear_flag(m, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(m, 0, LV_PART_MAIN);

    /* the whole static dial in one blit */
    lv_obj_set_style_bg_img_src(m, c->face, LV_PART_MAIN);

    /* lv_meter would otherwise paint its own hub disc over the needle */
    lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_width(m, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(m, 0, LV_PART_INDICATOR);

    g->meter = m;
    g->scale = lv_meter_add_scale(m);

    /* 250 degree sweep starting 125 degrees before 12 o'clock. LVGL measures
     * from 3 o'clock, so 270 - 125 = 145. Must match gen_dials.py. */
    lv_meter_set_scale_range(m, g->scale, g->imin, g->imax, 250, 145);
    /* zero ticks: they are already in the baked image */
    lv_meter_set_scale_ticks(m, g->scale, 0, 0, 0, C_DIM);

    if (c->has_fill) {
        g->fill = lv_meter_add_arc(m, g->scale, LY_BAND_W, C_Y, LY_BAND_MOD);
        lv_meter_set_indicator_start_value(m, g->fill, 0);
        lv_meter_set_indicator_end_value(m, g->fill, 0);
    } else {
        g->fill = NULL;
    }

    g->needle = lv_meter_add_needle_img(m, g->scale, &needle_y,
                                        NEEDLE_PIVOT_X, NEEDLE_PIVOT_Y);
    lv_meter_set_indicator_value(m, g->needle, g->imin);

    lv_obj_t *hub = lv_img_create(par);
    lv_img_set_src(hub, &hub_cap);
    lv_obj_set_pos(hub,
                   bx + LY_GAUGE_W / 2 - hub_cap.header.w / 2,
                   LY_METER_Y + LY_METER / 2 - hub_cap.header.h / 2);

    g->row = mk_value_row(par, bx, LY_VAL_Y, LY_GAUGE_W, 50,
                          &dash_num_46, c->unit, LV_FLEX_ALIGN_CENTER,
                          &g->value);

    g->sub = c->show_lambda
             ? mk_label(par, &dash_lbl_13, C_YDIM, "",
                        bx, LY_SUB_Y, LY_GAUGE_W, LV_TEXT_ALIGN_CENTER)
             : NULL;
}

static void update_gauge(gauge_t *g, float raw)
{
    g->shown += (raw - g->shown) * NEEDLE_SMOOTH;

    int32_t iv = (int32_t)lroundf(g->shown * g->scale_div);
    if (iv < g->imin) iv = g->imin;
    if (iv > g->imax) iv = g->imax;
    lv_meter_set_indicator_value(g->meter, g->needle, iv);

    bool warn = !isnan(g->warn_above) && g->shown >= g->warn_above;
    if (warn != g->warn) {
        g->warn = warn;
        lv_obj_set_style_text_color(g->value, warn ? C_RED : C_W, 0);
        g->needle->type_data.needle_img.src = warn ? &needle_r : &needle_y;
        if (g->fill) {
            g->fill->type_data.arc.color = warn ? C_RED : C_Y;
        }
        lv_obj_invalidate(g->meter);
    }

    if (g->fill) {
        int32_t lo = iv < 0 ? iv : 0;
        int32_t hi = iv < 0 ? 0 : iv;
        lv_meter_set_indicator_start_value(g->meter, g->fill, lo);
        lv_meter_set_indicator_end_value(g->meter, g->fill, hi);
    }

    char buf[16];
    if (g->val_dec == 2)      snprintf(buf, sizeof buf, "%.2f", g->shown);
    else if (g->val_dec == 1) snprintf(buf, sizeof buf, "%.1f", g->shown);
    else                      snprintf(buf, sizeof buf, "%d",
                                       (int)lroundf(g->shown));
    set_text_if_changed(g->value, buf);

    if (g->sub) {
        /* UTF-8 lambda, the glyph is in dash_lbl_13 */
        snprintf(buf, sizeof buf, "\xCE\xBB %.2f", g->shown / set_stoich());
        set_text_if_changed(g->sub, buf);
    }
}

/* ------------------------------------------------------------- middle block */
static void build_middle(lv_obj_t *par)
{
    mk_label(par, &dash_lbl_18, C_Y, "RPM",
             LY_MID_X, LY_TITLE_Y, LY_MID_W - 48, LV_TEXT_ALIGN_CENTER);
    mk_label(par, &dash_lbl_13, C_YDIM, "x1000",
             LY_MID_X + LY_MID_W - 46, LY_TITLE_Y + 6, 0, LV_TEXT_ALIGN_LEFT);

    for (int i = 0; i <= 8; i++) {
        rpm_scale_lbl[i] = mk_label(par, &dash_num_15, C_GREY, "",
                                    RPM_X0 + i * RPM_LBL_STEP - 10,
                                    LY_RPM_SCALE_Y, 20, LV_TEXT_ALIGN_CENTER);
    }

    for (int i = 0; i < RPM_SEGS; i++) {
        rpm_seg[i] = mk_seg(par, RPM_X0 + i * RPM_PITCH, LY_RPM_BAR_Y,
                            RPM_SEG_W, LY_RPM_BAR_H, C_DIM);
        rpm_on[i] = false;
    }

    lbl_rpm = mk_label(par, &dash_num_64, C_W, "0",
                       LY_MID_X, LY_RPM_NUM_Y, LY_MID_W, LV_TEXT_ALIGN_CENTER);

    mk_seg(par, LY_MID_X + 40, LY_RULE_Y, LY_MID_W - 80, 1, C_LINE);

    mk_label(par, &dash_lbl_18, C_Y, "SPEED",
             LY_MID_X, LY_SPEED_LBL_Y, LY_MID_W, LV_TEXT_ALIGN_CENTER);

    mk_value_row(par, LY_MID_X, LY_SPEED_ROW_Y, LY_MID_W, 50,
                 &dash_num_46, "km/h", LV_FLEX_ALIGN_CENTER, &lbl_speed);
}

static bool seg_is_redline(int i)
{
    return ((i + 1) * (int)g_set.rpm_max / RPM_SEGS) > g_set.rpm_redline;
}

static void update_middle(float rpm, float speed)
{
    int lit = (int)lroundf(rpm / g_set.rpm_max * RPM_SEGS);
    if (lit < 0) lit = 0;
    if (lit > RPM_SEGS) lit = RPM_SEGS;

    if (lit != rpm_lit) {
        for (int i = 0; i < RPM_SEGS; i++) {
            bool on = i < lit;
            if (on == rpm_on[i]) continue;
            rpm_on[i] = on;
            bool rl = seg_is_redline(i);
            lv_obj_set_style_bg_color(rpm_seg[i],
                on ? (rl ? C_RED : C_Y) : (rl ? C_REDDIM : C_DIM), 0);
        }
        rpm_lit = lit;
    }

    char buf[12];
    snprintf(buf, sizeof buf, "%d", (int)lroundf(rpm));
    set_text_if_changed(lbl_rpm, buf);
    snprintf(buf, sizeof buf, "%d", (int)lroundf(speed));
    set_text_if_changed(lbl_speed, buf);
}

/* -------------------------------------------------------------- card block */
static void build_cards(lv_obj_t *par)
{
    for (int i = 0; i < 5; i++) {
        const card_cfg_t *c = &card_cfg[i];
        card_t *k = &cards[i];

        k->root = mk_panel(par, LY_M + i * LY_CARD_STEP, LY_CARD_Y,
                           LY_CARD_W, LY_CARD_H);
        lv_obj_set_style_bg_color(k->root, C_CARDBG, 0);
        lv_obj_set_style_bg_opa(k->root, LV_OPA_COVER, 0);

        /* Icons are placed by their centre, read from the image header, so
         * swapping in a different pixel size needs no coordinate edits. */
        k->icon = mk_icon(k->root, c->icon, C_Y);
        lv_obj_set_pos(k->icon, LY_ICON_CX - c->icon->header.w / 2,
                       LY_ICON_CY - c->icon->header.h / 2);

        mk_label(k->root, &dash_lbl_13, C_LBL, c->label,
                 LY_ICON_LBL, 14, 0, LV_TEXT_ALIGN_LEFT);

        /* One continuous bar reads cleaner at this size than a segment row. */
        k->bar = lv_bar_create(k->root);
        lv_obj_remove_style_all(k->bar);
        lv_obj_set_pos(k->bar, 12, 40);
        lv_obj_set_size(k->bar, LY_CARD_W - 24, 6);
        lv_obj_set_style_bg_color(k->bar, C_DIM, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(k->bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(k->bar, 3, LV_PART_MAIN);
        lv_obj_set_style_bg_color(k->bar, C_Y, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(k->bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(k->bar, 3, LV_PART_INDICATOR);
        lv_bar_set_range(k->bar, 0, 1000);
        lv_bar_set_value(k->bar, 0, LV_ANIM_OFF);

        mk_value_row(k->root, 12, 54, LY_CARD_W - 24, 40,
                     &dash_num_28, c->unit, LV_FLEX_ALIGN_START, &k->value);

        k->warn = false;
        k->armed = false;
        k->shown = -1;
    }
}

static void update_cards(const dash_data_t *d)
{
    const float vals[5] = { d->clt, d->oilt, d->oilp, d->iat, d->fuelp };

    for (int i = 0; i < 5; i++) {
        const card_cfg_t *c = &card_cfg[i];
        card_t *k = &cards[i];
        float v = vals[i];

        /* Same rule as the flash: a limit of zero is off, and a low limit
         * only bites after the channel has read healthy once. */
        float lim = card_limit(i);
        bool warn = false;
        if (lim > 0.0f) {
            if (c->warn_high) {
                warn = v >= lim;
            } else if (c->warn_low) {
                if (v > lim) k->armed = true;
                warn = k->armed && v <= lim && d->rpm > 400;
            }
        }

        if (warn != k->warn) {
            k->warn = warn;
            lv_obj_set_style_border_color(k->root, warn ? C_RED : C_LINE, 0);
            lv_obj_set_style_text_color(k->value, warn ? C_RED : C_W, 0);
            lv_obj_set_style_img_recolor(k->icon, warn ? C_RED : C_Y, 0);
            lv_obj_set_style_bg_color(k->bar, warn ? C_RED : C_Y,
                                      LV_PART_INDICATOR);
        }

        int32_t f = (int32_t)lroundf(
            clampf((v - c->lo) / (c->hi - c->lo), 0.0f, 1.0f) * 1000.0f);
        if (f != k->shown) {
            lv_bar_set_value(k->bar, f, LV_ANIM_OFF);
            k->shown = f;
        }

        char buf[12];
        if (c->decimals == 1) snprintf(buf, sizeof buf, "%.1f", v);
        else                  snprintf(buf, sizeof buf, "%d", (int)lroundf(v));
        set_text_if_changed(k->value, buf);
    }
}

/* --------------------------------------------------------- bottom bar block */
/* Flags, wordmark and link indicator share one strip. Two separate rows for
 * this little information wasted a fifth of the panel. */
static void build_bar(lv_obj_t *par)
{
    lv_obj_t *p = mk_panel(par, LY_M, LY_BAR_Y, 800 - 2 * LY_M, LY_BAR_H);

    const char *names[2] = { "FAN", "ALS" };
    const lv_img_dsc_t *ics[2] = { &ic_fan, &ic_flame };
    const lv_coord_t xs[2] = { 30, 122 };   /* icon centres */

    mk_seg(p, 92, 10, 1, LY_BAR_H - 22, C_LINE);
    mk_seg(p, 190, 10, 1, LY_BAR_H - 22, C_LINE);

    for (int i = 0; i < 2; i++) {
        st_icon[i] = mk_icon(p, ics[i], C_OFF);
        lv_obj_set_pos(st_icon[i], xs[i] - ics[i]->header.w / 2,
                       LY_BAR_H / 2 - ics[i]->header.h / 2);
        mk_label(p, &dash_lbl_13, C_GREY, names[i], xs[i] + 20, 7, 0,
                 LV_TEXT_ALIGN_LEFT);
        st_val[i] = mk_label(p, &dash_lbl_13, C_OFF, "OFF",
                             xs[i] + 20, 22, 0, LV_TEXT_ALIGN_LEFT);
    }

    /* The wordmark is a traced bitmap rather than two text labels: it is
     * italic, tightly kerned and two-coloured, none of which a single LVGL
     * font can do. */
    lv_obj_t *logo = lv_img_create(p);
    lv_img_set_src(logo, &logo_notstock);
    lv_obj_set_pos(logo, (800 - 2 * LY_M - logo_notstock.header.w) / 2, 3);

    lv_obj_t *sub = mk_label(p, &dash_lbl_13, C_SUBTLE, "NOT STABLE",
                             (800 - 2 * LY_M) / 2 - 110, 28, 220,
                             LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_letter_space(sub, 5, 0);

    const lv_coord_t rx = 800 - 2 * LY_M;
    mk_seg(p, rx - 132, 10, 1, LY_BAR_H - 22, C_LINE);
    link_dot = mk_seg(p, rx - 116, LY_BAR_H / 2 - 4, 8, 8, C_OFF);
    lv_obj_set_style_radius(link_dot, LV_RADIUS_CIRCLE, 0);
    link_txt = mk_label(p, &dash_lbl_13, C_GREY, "NO LINK",
                        rx - 98, LY_BAR_H / 2 - 9, 0, LV_TEXT_ALIGN_LEFT);
}

static void update_flags(bool fan, bool als)
{
    const bool on[2] = { fan, als };
    for (int i = 0; i < 2; i++) {
        if (st_state[i] == (int)on[i]) continue;
        st_state[i] = (int)on[i];
        lv_obj_set_style_img_recolor(st_icon[i], on[i] ? C_Y : C_OFF, 0);
        lv_obj_set_style_text_color(st_val[i], on[i] ? C_Y : C_OFF, 0);
        lv_label_set_text(st_val[i], on[i] ? "ON" : "OFF");
    }
}

static void update_link(int state)
{
    if (state == link_state) return;
    link_state = state;
    switch (state) {
    case 0:
        lv_obj_set_style_bg_color(link_dot, C_RED, 0);
        lv_obj_set_style_text_color(link_txt, C_RED, 0);
        lv_label_set_text(link_txt, "NO LINK");
        break;
    case 1:
        lv_obj_set_style_bg_color(link_dot, C_GREEN, 0);
        lv_obj_set_style_text_color(link_txt, C_GREY, 0);
        lv_label_set_text(link_txt, "CAN");
        break;
    default:
        lv_obj_set_style_bg_color(link_dot, C_Y, 0);
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
    float rpm = 900.0f + 6200.0f * powf(sinf(cyc * (float)M_PI), 1.4f);
    float boost = -0.6f + 2.0f *
                  powf(clampf((rpm - 1800.0f) / 4200.0f, 0.0f, 1.0f), 1.2f);

    d->rpm   = rpm;
    d->boost = boost;
    d->speed = clampf(rpm / 8000.0f * 180.0f, 0, 255);
    d->afr   = boost > 0.4f ? 11.6f + 0.5f * sinf(t * 3.0f)
                            : 14.4f + 0.8f * sinf(t * 1.7f);
    d->clt   = 88.0f + 6.0f * sinf(t / 9.0f);
    d->oilt  = 94.0f + 8.0f * sinf(t / 11.0f);
    d->oilp  = clampf(0.9f + rpm / 8000.0f * 4.4f, 0, 8);
    d->iat   = 30.0f + 18.0f * clampf(boost, 0, 2);
    d->fuelp = 3.5f + clampf(boost, 0, 2) * 0.9f;
    d->fan   = d->clt > 90.0f;
    d->als   = rpm > 5800.0f && boost > 1.2f;
}

/* ------------------------------------------------------------- alarm flash */
/* A red wash over everything, on LVGL's top layer so it covers the dash but
 * sits under the brightness dim. It is not clickable, so the menu still
 * works while an alarm is going off. */
static void build_overlays(void)
{
    /* Drawn first, so it sits under the red. A plain translucent red over a
     * yellow-on-black dash turns muddy orange; knocking the picture back
     * first keeps the red saturated at high intensities. */
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
 * still turns its own tile, needle or readout red, which is enough for a
 * temperature or a pressure: those creep, and a strobing screen while you are
 * trying to read the number that caused it is worse than useless. Revs are
 * the one case where the driver has to react inside a second and cannot be
 * looking at the panel. */
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
        lv_obj_t *dot = mk_seg(hit, MENU_HIT_W - 30 + i * 8,
                               MENU_HIT_H - 16, 3, 3, C_LINE);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    }
}

void ui_show_dash(void)
{
    lv_scr_load(scr_dash);
}

void ui_apply_settings(void)
{
    /* rev counter scale */
    for (int i = 0; i <= 8; i++) {
        char t[8];
        snprintf(t, sizeof t, "%d", i * g_set.rpm_max / 8000);
        set_text_if_changed(rpm_scale_lbl[i], t);
    }
    rpm_lit = -1;
    for (int i = 0; i < RPM_SEGS; i++) {
        rpm_on[i] = false;
        lv_obj_set_style_bg_color(rpm_seg[i],
            seg_is_redline(i) ? C_REDDIM : C_DIM, 0);
    }

    g_boost.warn_above = set_boost_warn();
    g_afr.warn_above   = g_set.afr_lean_warn ? g_set.afr_lean_warn / 10.0f
                                             : NAN;

    /* force the next update to repaint every tile, and re-arm the low-limit
     * channels so a changed limit is judged from fresh readings */
    for (int i = 0; i < 5; i++) {
        cards[i].warn = !cards[i].warn;
        cards[i].armed = false;
    }

    lv_obj_set_style_bg_opa(dim_layer,
        (lv_opa_t)((100 - g_set.brightness) * 255 / 100), 0);

    if (!g_set.flash_enable && flash_layer) {
        lv_obj_add_flag(flash_layer, LV_OBJ_FLAG_HIDDEN);
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

    update_gauge(&g_boost, clampf(d.boost, -1.2f, 2.5f));
    update_gauge(&g_afr,   clampf(d.afr, 9.0f, 19.0f));
    update_middle(d.rpm, d.speed);
    update_cards(&d);
    update_flags(d.fan, d.als);
    update_alarm(&d);
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

    gauge_cfg_t bc = {
        .title = "BOOST", .unit = "bar", .face = &dial_boost,
        .vmin = -1.0f, .vmax = 2.0f, .scale_div = 100.0f,
        .val_dec = 2, .has_fill = true, .show_lambda = false,
        .warn_above = set_boost_warn(),
    };
    gauge_cfg_t ac = {
        .title = "AFR", .unit = "", .face = &dial_afr,
        .vmin = 10.0f, .vmax = 18.0f, .scale_div = 100.0f,
        .val_dec = 1, .has_fill = false, .show_lambda = true,
        .warn_above = NAN,
    };

    build_gauge(&g_boost, scr, LY_GX_L, &bc);
    build_gauge(&g_afr,   scr, LY_GX_R, &ac);
    build_middle(scr);
    build_cards(scr);
    build_bar(scr);
    build_menu_hit(scr);
    build_overlays();

    ui_menu_create();
    ui_apply_settings();

    lv_timer_create(ui_timer_cb, 40, NULL);
}
