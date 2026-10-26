/* HILL look: yellow round rev counter in the middle with a red needle, three
 * value tiles either side with a yellow bargraph on their outer edge, rpm and
 * speed boxes under the dial.
 *
 * Face, ticks, numbers, tile frames, empty bargraph slots and labels are one
 * baked image (tools/gen_themes.py). Live parts: the needle, the lit
 * bargraph segments and the numbers.
 */
#include "ui.h"
#include "ui_theme.h"
#include "settings.h"

#include <math.h>
#include <stdio.h>

LV_FONT_DECLARE(hill_66);
LV_FONT_DECLARE(hill_38);
LV_FONT_DECLARE(hill_20);

#define C_YEL   lv_color_hex(0xFFD21A)
#define C_RED   lv_color_hex(0xFF3B30)
#define C_W     lv_color_hex(0xFFFFFF)
#define C_GREY  lv_color_hex(0x9A9DA1)
#define C_BOX   lv_color_hex(0x111112)
#define C_EDGE  lv_color_hex(0x3A3B3E)

#define NEEDLE_SMOOTH 0.3f

enum { T_BOOST, T_AFR, T_LAMBDA, T_CLT, T_IAT, T_MAP, T_COUNT };

static const struct {
    lv_coord_t x, y;
    bool left;             /* bargraph on the left edge */
    float lo, hi;
    int dec, lim;
} TILE[T_COUNT] = {
    { HILL_T0_X, HILL_T0_Y, true,  -1,   2,  2, LIM_BOOST },
    { HILL_T1_X, HILL_T1_Y, true,  10,  18,  1, LIM_AFR },
    { HILL_T2_X, HILL_T2_Y, true,  0.7f, 1.3f, 2, -1 },
    { HILL_T3_X, HILL_T3_Y, false, 40, 120,  0, LIM_CLT },
    { HILL_T4_X, HILL_T4_Y, false,  0,  80,  0, LIM_IAT },
    { HILL_T5_X, HILL_T5_Y, false,  0, 300,  0, -1 },
};

static lv_obj_t *meter, *rpm, *speed, *link_lbl;
static lv_meter_indicator_t *needle;
static lv_obj_t *val[T_COUNT], *seg[T_COUNT][HILL_SEGS];
static int lit[T_COUNT], warn[T_COUNT], rpm_warn, link_shown;
static float shown_rpm;

static void build_tile(lv_obj_t *scr, int i)
{
    lv_coord_t x = TILE[i].x, y = TILE[i].y;
    lv_coord_t bx = TILE[i].left ? x + 10 : x + HILL_TILE_W - 34;
    float seg_h = (HILL_TILE_H - 24) / (float)HILL_SEGS;
    for (int s = 0; s < HILL_SEGS; s++) {
        lv_coord_t sy = (lv_coord_t)(y + 12 + s * seg_h + 2);
        seg[i][s] = ui_rect(scr, bx, sy, 24, (lv_coord_t)(seg_h - 3), C_YEL);
        lv_obj_add_flag(seg[i][s], LV_OBJ_FLAG_HIDDEN);
    }
    lit[i] = 0;
    warn[i] = -1;

    lv_coord_t rx = TILE[i].left ? x + 44 : x + 10;
    lv_obj_t *row = ui_box(scr, rx, y + 14, HILL_TILE_W - 54, 70);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row, 5, 0);
    val[i] = ui_label(row, &hill_66, C_YEL, "-", 0, 0, 0, LV_TEXT_ALIGN_RIGHT);
    if (HILL_UNIT[i][0]) {
        lv_obj_t *u = ui_label(row, &hill_20, C_GREY, HILL_UNIT[i], 0, 0, 0,
                               LV_TEXT_ALIGN_LEFT);
        ui_unit_on_baseline(u);
    }
}

static lv_obj_t *mk_box_val(lv_obj_t *scr, lv_coord_t x, const char *unit)
{
    lv_obj_t *b = ui_rect(scr, x, 436, 118, 40, C_BOX);
    lv_obj_set_style_border_color(b, C_EDGE, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(b, 5, 0);
    lv_obj_set_style_pad_bottom(b, 4, 0);
    lv_obj_t *v = ui_label(b, &hill_38, C_W, "0", 0, 0, 0, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *u = ui_label(b, &hill_20, C_GREY, unit, 0, 0, 0,
                           LV_TEXT_ALIGN_LEFT);
    ui_unit_on_baseline(u);
    return v;
}

static lv_obj_t *build(void)
{
    lv_obj_t *scr = ui_screen(lv_color_black());
    ui_img_at(scr, &hill_bg, 0, 0);

    /* needle only: the face is in the background image. Square meter centred
     * on the pivot, same trick as the NOTSTOCK dials. */
    lv_coord_t side = 2 * hill_needle.header.w;
    meter = lv_meter_create(scr);
    lv_obj_remove_style_all(meter);
    lv_obj_set_size(meter, side, side);
    lv_obj_set_pos(meter, HILL_CX - side / 2, HILL_CY - side / 2);
    lv_obj_clear_flag(meter, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(meter, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_width(meter, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(meter, 0, LV_PART_INDICATOR);
    lv_meter_scale_t *sc = lv_meter_add_scale(meter);
    lv_meter_set_scale_range(meter, sc, 0, RPM_MAX, HILL_SWEEP, HILL_START);
    lv_meter_set_scale_ticks(meter, sc, 0, 0, 0, C_EDGE);
    needle = lv_meter_add_needle_img(meter, sc, &hill_needle,
                                     HILL_NEEDLE_PIVOT_X, HILL_NEEDLE_PIVOT_Y);
    lv_meter_set_indicator_value(meter, needle, 0);
    shown_rpm = 0;
    lv_obj_t *hub = lv_img_create(scr);
    lv_img_set_src(hub, &hill_hub);
    lv_obj_set_pos(hub, HILL_CX - hill_hub.header.w / 2,
                   HILL_CY - hill_hub.header.h / 2);

    for (int i = 0; i < T_COUNT; i++) build_tile(scr, i);

    rpm = mk_box_val(scr, 400 - 124, "RPM");
    speed = mk_box_val(scr, 400 + 6, "KM/H");

    link_lbl = ui_label(scr, &hill_20, C_RED, "", 300, 18, 200,
                        LV_TEXT_ALIGN_CENTER);
    rpm_warn = link_shown = -1;
    ui_corners(scr);
    return scr;
}

static void update(const dash_data_t *d, int link)
{
    char b[16];

    shown_rpm += (ui_clampf(d->rpm, 0, RPM_MAX) - shown_rpm) * NEEDLE_SMOOTH;
    lv_meter_set_indicator_value(meter, needle, (int32_t)shown_rpm);

    snprintf(b, sizeof b, "%d", (int)lroundf(d->rpm / 10) * 10);
    ui_text(rpm, b);
    int w = ui_over(LIM_RPM, d->rpm);
    if (w != rpm_warn) {
        rpm_warn = w;
        lv_obj_set_style_text_color(rpm, w ? C_RED : C_W, 0);
    }
    snprintf(b, sizeof b, "%d", (int)lroundf(ui_clampf(d->speed, 0, 999)));
    ui_text(speed, b);

    const float v[T_COUNT] = { d->boost, d->afr,
                               d->afr / (g_set.stoich / 10.0f),
                               d->clt, d->iat, d->map };
    for (int i = 0; i < T_COUNT; i++) {
        float f = ui_clampf((v[i] - TILE[i].lo) / (TILE[i].hi - TILE[i].lo),
                            0, 1);
        int n = (int)(f * HILL_SEGS + 0.5f);
        if (n != lit[i]) {
            /* segment 0 is the top one, the bar grows from the bottom */
            for (int s = 0; s < HILL_SEGS; s++) {
                bool on = (HILL_SEGS - 1 - s) < n;
                bool was = (HILL_SEGS - 1 - s) < lit[i];
                if (on == was) continue;
                if (on) lv_obj_clear_flag(seg[i][s], LV_OBJ_FLAG_HIDDEN);
                else    lv_obj_add_flag(seg[i][s], LV_OBJ_FLAG_HIDDEN);
            }
            lit[i] = n;
        }
        if (TILE[i].dec == 2)      snprintf(b, sizeof b, "%.2f", v[i]);
        else if (TILE[i].dec == 1) snprintf(b, sizeof b, "%.1f", v[i]);
        else                       snprintf(b, sizeof b, "%d", (int)lroundf(v[i]));
        ui_text(val[i], b);
        int wn = TILE[i].lim >= 0 && ui_over(TILE[i].lim, v[i]);
        if (wn != warn[i]) {
            warn[i] = wn;
            lv_obj_set_style_text_color(val[i], wn ? C_RED : C_YEL, 0);
        }
    }

    if (link != link_shown) {
        link_shown = link;
        ui_text(link_lbl, link == LINK_NONE ? "NO CAN"
                        : link == LINK_DEMO ? "DEMO" : "");
        lv_obj_set_style_text_color(link_lbl,
                                    link == LINK_NONE ? C_RED : C_YEL, 0);
    }
}

const theme_t theme_hill = {
    .build = build, .update = update,
    .flash_cx = HILL_CX, .flash_cy = HILL_CY, .flash_r = HILL_R,
};
