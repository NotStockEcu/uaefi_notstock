/* LONK look: light steel-blue dash with a rev band running along the top,
 * big speed and rpm numbers, a boost box and a row of value tiles.
 *
 * The rev band is two baked images: the background carries it unlit, and
 * lonk_lit is the same strip of the screen with the band lit (white, red
 * past 7000). The lit image sits on top, cropped to a width that follows
 * the rpm, so lighting the band costs one narrow redraw per change.
 */
#include "ui.h"
#include "ui_theme.h"
#include "settings.h"

#include <math.h>
#include <stdio.h>

LV_FONT_DECLARE(lonk_64);
LV_FONT_DECLARE(lonk_42);

#define C_NAVY  lv_color_hex(0x1C3354)
#define C_BLUE  lv_color_hex(0x2F6DB5)
#define C_RED   lv_color_hex(0xD62828)
#define C_W     lv_color_hex(0xFFFFFF)

enum { T_AFR, T_MAP, T_IAT, T_CLT, T_LAMBDA, T_COUNT };

static const struct {
    lv_coord_t x0, y0, x1, y1;
    const char *unit;
    int dec, lim;
} TILE[T_COUNT] = {
    { LONK_T0_X0, LONK_T0_Y0, LONK_T0_X1, LONK_T0_Y1, "",    1, LIM_AFR },
    { LONK_T1_X0, LONK_T1_Y0, LONK_T1_X1, LONK_T1_Y1, "kPa", 0, -1 },
    { LONK_T2_X0, LONK_T2_Y0, LONK_T2_X1, LONK_T2_Y1, "\xC2\xB0" "C", 0, LIM_IAT },
    { LONK_T3_X0, LONK_T3_Y0, LONK_T3_X1, LONK_T3_Y1, "\xC2\xB0" "C", 0, LIM_CLT },
    { LONK_T4_X0, LONK_T4_Y0, LONK_T4_X1, LONK_T4_Y1, "",    2, -1 },
};

static lv_obj_t *band, *speed, *rpm, *boost, *info, *val[T_COUNT];
static int band_w, warn[T_COUNT], rpm_warn, boost_warn, link_shown;

/* number + small unit, right-aligned inside a tile, on one baseline */
static lv_obj_t *mk_value(lv_obj_t *par, lv_coord_t x0, lv_coord_t y0,
                          lv_coord_t x1, lv_coord_t y1, const lv_font_t *f,
                          const char *unit)
{
    lv_obj_t *row = ui_box(par, x0 + 8, y0, x1 - x0 - 22, y1 - y0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);
    lv_obj_t *v = ui_label(row, f, C_NAVY, "-", 0, 0, 0, LV_TEXT_ALIGN_RIGHT);
    if (unit && unit[0]) {
        lv_obj_t *u = ui_label(row, &dash_orb_14, C_BLUE, unit, 0, 0, 0,
                               LV_TEXT_ALIGN_LEFT);
        lv_obj_set_style_translate_y(u, 8, 0);
    }
    return v;
}

static lv_obj_t *build(void)
{
    lv_obj_t *scr = ui_screen(lv_color_black());
    ui_img_at(scr, &lonk_bg, 0, 0);

    /* The lit band: an lv_img narrower than its source is clipped to the
     * object, which is exactly the crop wanted here. */
    band = ui_img_at(scr, &lonk_lit, 0, 0);
    lv_obj_set_size(band, 1, lonk_lit.header.h);
    band_w = 1;

    speed = ui_label(scr, &lonk_64, C_NAVY, "0", LONK_SPEED_X1 - 220,
                     LONK_BIG_Y + 16, 220, LV_TEXT_ALIGN_RIGHT);
    rpm = ui_label(scr, &lonk_64, C_NAVY, "0", LONK_RPM_X1 - 260,
                   LONK_BIG_Y + 16, 260, LV_TEXT_ALIGN_RIGHT);
    boost = ui_label(scr, &lonk_42, C_NAVY, "0.00", LONK_BOX_X0,
                     (LONK_BOX_Y0 + LONK_BOX_Y1) / 2 - 16,
                     LONK_BOX_X1 - LONK_BOX_X0, LV_TEXT_ALIGN_CENTER);

    for (int i = 0; i < T_COUNT; i++) {
        val[i] = mk_value(scr, TILE[i].x0, TILE[i].y0, TILE[i].x1, TILE[i].y1,
                          &lonk_42, TILE[i].unit);
        warn[i] = -1;
    }

    info = ui_label(scr, &dash_orb_14, C_W, "", 0, 459, 800,
                    LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_letter_space(info, 3, 0);

    rpm_warn = boost_warn = link_shown = -1;
    ui_corners(scr);
    return scr;
}

static void set_warn(lv_obj_t *l, int *state, int w)
{
    if (w == *state) return;
    *state = w;
    lv_obj_set_style_text_color(l, w ? C_RED : C_NAVY, 0);
}

static void update(const dash_data_t *d, int link)
{
    char b[16];

    int w = (int)(LONK_X0 + (LONK_X1 - LONK_X0) *
                  ui_clampf(d->rpm, 0, RPM_MAX) / RPM_MAX);
    if (w < 1) w = 1;
    if (w != band_w) {
        lv_obj_set_width(band, w);
        band_w = w;
    }

    snprintf(b, sizeof b, "%d", (int)lroundf(ui_clampf(d->speed, 0, 999)));
    ui_text(speed, b);
    snprintf(b, sizeof b, "%d", (int)lroundf(d->rpm / 10) * 10);
    ui_text(rpm, b);
    set_warn(rpm, &rpm_warn, ui_over(LIM_RPM, d->rpm));
    snprintf(b, sizeof b, "%.2f", d->boost);
    ui_text(boost, b);
    set_warn(boost, &boost_warn, ui_over(LIM_BOOST, d->boost));

    const float v[T_COUNT] = { d->afr, d->map, d->iat, d->clt,
                               d->afr / (g_set.stoich / 10.0f) };
    for (int i = 0; i < T_COUNT; i++) {
        if (TILE[i].dec == 2)      snprintf(b, sizeof b, "%.2f", v[i]);
        else if (TILE[i].dec == 1) snprintf(b, sizeof b, "%.1f", v[i]);
        else                       snprintf(b, sizeof b, "%d", (int)lroundf(v[i]));
        ui_text(val[i], b);
        set_warn(val[i], &warn[i], TILE[i].lim >= 0 && ui_over(TILE[i].lim, v[i]));
    }

    if (link != link_shown) {
        link_shown = link;
        ui_text(info, link == LINK_NONE ? "NO CAN"
                    : link == LINK_DEMO ? "DEMO" : "CAN OK");
        lv_obj_set_style_text_color(info, link == LINK_NONE
                                    ? lv_color_hex(0xFF6B6B) : C_W, 0);
    }
}

const theme_t theme_lonk = {
    .build = build, .update = update,
    .flash_r = 0,           /* no round rev counter: always the whole screen */
};
