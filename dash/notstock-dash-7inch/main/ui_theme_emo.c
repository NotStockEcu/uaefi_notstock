/* EMO look: segmented rev arc with a big italic speed in the middle, bar
 * gauges in the corners, yellow tabs along the bottom.
 *
 * Background, scales, grey segments, frames and labels are one baked image
 * (tools/gen_themes.py). Live parts: one lv_arc per lit segment, the bar
 * fills and the numbers.
 */
#include "ui.h"
#include "ui_theme.h"
#include "settings.h"

#include <math.h>
#include <stdio.h>

LV_FONT_DECLARE(emo_150);
LV_FONT_DECLARE(emo_64);
LV_FONT_DECLARE(emo_24);
LV_FONT_DECLARE(emo_36);
LV_FONT_DECLARE(emo_15);

#define C_YEL   lv_color_hex(0xFFCC00)
#define C_RED   lv_color_hex(0xE22424)
#define C_W     lv_color_hex(0xFFFFFF)
#define C_BLK   lv_color_hex(0x101010)
#define C_GREY  lv_color_hex(0xA8ACB0)

#define RPM_PER_SEG (RPM_MAX / EMO_SEGS)

typedef struct {
    lv_coord_t x, y;
    float lo, hi;
    int lim;               /* LIM_* */
    int dec;
    const char *unit;
} bar_cfg_t;

static const bar_cfg_t BARS[4] = {
    { EMO_BAR0_X, EMO_BAR0_Y, 40, 120,  LIM_CLT,   0, "\xC2\xB0" "C" },
    { EMO_BAR1_X, EMO_BAR1_Y,  0,  80,  LIM_IAT,   0, "\xC2\xB0" "C" },
    { EMO_BAR2_X, EMO_BAR2_Y, -1,   2,  LIM_BOOST, 2, " bar" },
    { EMO_BAR3_X, EMO_BAR3_Y, 10,  18,  LIM_AFR,   1, "" },
};

static lv_obj_t *seg[EMO_SEGS];
static int lit = -1;
static lv_obj_t *speed, *rpm, *top, *fill[4], *val[4], *lam, *map;
static int fill_w[4], fill_warn[4], rpm_warn, link_shown;

static lv_obj_t *mk_seg(lv_obj_t *par, int i)
{
    int a0 = EMO_START + i * (EMO_SWEEP / EMO_SEGS) + EMO_GAP / 2;
    int a1 = EMO_START + (i + 1) * (EMO_SWEEP / EMO_SEGS) - EMO_GAP / 2;
    lv_obj_t *a = lv_arc_create(par);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, 2 * EMO_R_OUT, 2 * EMO_R_OUT);
    lv_obj_set_pos(a, EMO_CX - EMO_R_OUT, EMO_CY - EMO_R_OUT);
    lv_arc_set_bg_angles(a, a0 % 360, a1 % 360);
    lv_obj_set_style_arc_width(a, EMO_R_OUT - EMO_R_IN, LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, i >= 14 ? C_RED : C_YEL, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(a, LV_OBJ_FLAG_HIDDEN);
    return a;
}

static lv_obj_t *build(void)
{
    lv_obj_t *scr = ui_screen(lv_color_black());
    ui_img_at(scr, &emo_bg, 0, 0);

    for (int i = 0; i < EMO_SEGS; i++) seg[i] = mk_seg(scr, i);
    lit = 0;

    top = ui_label(scr, &emo_15, C_GREY, "KM/H", EMO_CX - 80, EMO_CY - 116,
                   160, LV_TEXT_ALIGN_CENTER);
    speed = ui_label(scr, &emo_150, C_W, "0", EMO_CX - 170, EMO_CY - 92, 340,
                     LV_TEXT_ALIGN_CENTER);
    rpm = ui_label(scr, &emo_64, C_W, "0", EMO_CX - 120, EMO_CY + 46, 240,
                   LV_TEXT_ALIGN_CENTER);

    for (int i = 0; i < 4; i++) {
        fill[i] = ui_rect(scr, BARS[i].x, BARS[i].y, 1, EMO_BAR_H, C_YEL);
        val[i] = ui_label(scr, &emo_24, C_BLK, "", BARS[i].x + 8,
                          BARS[i].y + (EMO_BAR_H - 24) / 2 + 1, 0,
                          LV_TEXT_ALIGN_LEFT);
        fill_w[i] = -1;
        fill_warn[i] = -1;
    }

    const lv_coord_t tab_mid = EMO_TAB_Y + (474 - EMO_TAB_Y) / 2;
    lam = ui_label(scr, &emo_36, C_BLK, "", 92, tab_mid - 12, 110,
                   LV_TEXT_ALIGN_LEFT);
    map = ui_label(scr, &emo_36, C_BLK, "", 800 - 164, tab_mid - 12, 100,
                   LV_TEXT_ALIGN_CENTER);

    rpm_warn = -1;
    link_shown = -1;
    ui_corners(scr);
    return scr;
}

static void update(const dash_data_t *d, int link)
{
    char b[24];

    int n = (int)ui_clampf(d->rpm / RPM_PER_SEG + 0.5f, 0, EMO_SEGS);
    if (n != lit) {
        for (int i = 0; i < EMO_SEGS; i++) {
            bool on = i < n;
            if (on == (i < lit)) continue;
            if (on) lv_obj_clear_flag(seg[i], LV_OBJ_FLAG_HIDDEN);
            else    lv_obj_add_flag(seg[i], LV_OBJ_FLAG_HIDDEN);
        }
        lit = n;
    }

    snprintf(b, sizeof b, "%d", (int)lroundf(ui_clampf(d->speed, 0, 999)));
    ui_text(speed, b);
    snprintf(b, sizeof b, "%d", (int)lroundf(d->rpm / 10) * 10);
    ui_text(rpm, b);
    int w = ui_over(LIM_RPM, d->rpm);
    if (w != rpm_warn) {
        rpm_warn = w;
        lv_obj_set_style_text_color(rpm, w ? C_RED : C_W, 0);
    }

    const float v[4] = { d->clt, d->iat, d->boost, d->afr };
    for (int i = 0; i < 4; i++) {
        const bar_cfg_t *c = &BARS[i];
        float f = ui_clampf((v[i] - c->lo) / (c->hi - c->lo), 0, 1);
        int px = (int)(f * EMO_BAR_W + 0.5f);
        if (px < 1) px = 1;
        if (px != fill_w[i]) {
            lv_obj_set_width(fill[i], px);
            fill_w[i] = px;
        }
        int warn = ui_over(c->lim, v[i]);
        if (warn != fill_warn[i]) {
            lv_obj_set_style_bg_color(fill[i], warn ? C_RED : C_YEL, 0);
            fill_warn[i] = warn;
        }
        if (c->dec == 2)      snprintf(b, sizeof b, "%.2f%s", v[i], c->unit);
        else if (c->dec == 1) snprintf(b, sizeof b, "%.1f%s", v[i], c->unit);
        else                  snprintf(b, sizeof b, "%d%s", (int)lroundf(v[i]),
                                       c->unit);
        ui_text(val[i], b);
    }

    snprintf(b, sizeof b, "%.2f", d->afr / (g_set.stoich / 10.0f));
    ui_text(lam, b);
    snprintf(b, sizeof b, "%d", (int)lroundf(d->map));
    ui_text(map, b);

    if (link != link_shown) {
        link_shown = link;
        ui_text(top, link == LINK_NONE ? "NO CAN"
                   : link == LINK_DEMO ? "DEMO" : "KM/H");
        lv_obj_set_style_text_color(top, link == LINK_NONE ? C_RED
                                       : link == LINK_DEMO ? C_YEL : C_GREY, 0);
    }
}

const theme_t theme_emo = {
    .build = build, .update = update,
    .flash_cx = EMO_CX, .flash_cy = EMO_CY, .flash_r = EMO_R_IN - 10,
};
