/* OBD-II test screen: plain blocks, made to prove the link to an OBD ECU
 * (VW T5.1 CAAC) before anything prettier is built on it.
 *
 * Two pages, swipe left / right or tap the tabs at the top: TEST (the
 * blocks below) and DPF (the particulate filter's measuring values, a first
 * go at what the round gauge's DPF page will show).
 *
 * Shown instead of the selected look whenever the menu's ECU protocol is
 * OBD-II. Each block says which PID it reads and whether the ECU supports
 * it; the top line is the protocol state, the bottom line the raw counters
 * and every supported PID, which is what to look at when something is not
 * coming through.
 */
#include "ui.h"
#include "ui_theme.h"
#include "obd2.h"
#include "settings.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(obd_60);

#define C_BG     lv_color_hex(0x000000)
#define C_TILE   lv_color_hex(0x121416)
#define C_EDGE   lv_color_hex(0x2A2D31)
#define C_W      lv_color_hex(0xFFFFFF)
#define C_GREY   lv_color_hex(0x8A9096)
#define C_DIM    lv_color_hex(0x5A5E63)
#define C_Y      lv_color_hex(0xF5C518)
#define C_RED    lv_color_hex(0xE22424)
#define C_GREEN  lv_color_hex(0x3DDC84)

enum { B_CLT, B_OIL, B_BOOST, B_IAT, B_EGT, B_ENGINE, B_COUNT };

static const struct {
    const char *label, *unit;
    uint8_t pid;          /* 0: see pid_of() */
    int dec, lim;
} BLK[B_COUNT] = {
    [B_CLT]    = { "WATER",   "\xC2\xB0" "C", 0x05, 0, LIM_CLT },
    [B_OIL]    = { "OIL",     "\xC2\xB0" "C", 0x5C, 0, -1 },
    [B_BOOST]  = { "BOOST",   "bar",         0,    2, LIM_BOOST },
    [B_IAT]    = { "INTAKE",  "\xC2\xB0" "C", 0x0F, 0, LIM_IAT },
    [B_EGT]    = { "EXHAUST", "\xC2\xB0" "C", 0x78, 0, -1 },
    [B_ENGINE] = { "ENGINE",  "rpm",         0x0C, 0, -1 },
};

static lv_obj_t *val[B_COUNT], *unit_row[B_COUNT], *na[B_COUNT], *tag[B_COUNT];
static lv_obj_t *state_lbl, *stats_lbl, *pids_lbl, *speed_lbl;
static lv_obj_t *pg[2], *tab[2], *title_lbl;
static int page;

/* DPF page */
#define SOOT_MAX     40.0f     /* arc full scale, g */
#define SOOT_WARN    24.0f     /* guess until a regeneration is seen */
#define REGEN_TEMP   400.0f    /* filter hotter than this: regenerating */

enum { T_MEAS, T_DP, T_DIST, T_TEMP, T_COUNT };
static const struct { const char *label, *unit, *fmt; } TILE[T_COUNT] = {
    [T_MEAS] = { "SOOT MEASURED", "g",            "%.2f" },
    [T_DP]   = { "DIFF PRESSURE", "hPa",          "%.0f" },
    [T_DIST] = { "SINCE REGEN",   "km",           "%.1f" },
    [T_TEMP] = { "FILTER TEMP",   "\xC2\xB0" "C", "%.0f" },
};
/* the filter drawing, in the soot tile */
#define C_DPF_EMPTY  lv_color_hex(0x24282D)
#define C_DPF_CELL   lv_color_hex(0x0B0C0E)
#define DPF_X        40
#define DPF_Y        70
#define DPF_W        300
#define DPF_H        170
#define DPF_BORDER   6
#define DPF_IN_W     (DPF_W - 2 * DPF_BORDER)
#define DPF_IN_H     (DPF_H - 2 * DPF_BORDER)
static lv_obj_t *dpf_body, *dpf_fill, *dpf_pipe[2];
static lv_obj_t *soot_val, *soot_state;
static int soot_level = -1;
static lv_obj_t *tile_val[T_COUNT], *regen_box, *regen_lbl, *dpf_foot;
static int regen_on = -1;
static int warn[B_COUNT];
static uint32_t shown_rx = UINT32_MAX;

static uint8_t pid_of(int b)
{
    if (b != B_BOOST) return BLK[b].pid;
    return obd_supported(0x87) ? 0x87 : 0x0B;
}

static void build_block(lv_obj_t *scr, int b, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *t = ui_rect(scr, x, y, w, h, C_TILE);
    lv_obj_set_style_border_color(t, C_EDGE, 0);
    lv_obj_set_style_border_width(t, 2, 0);
    lv_obj_set_style_radius(t, 8, 0);

    ui_label(t, &dash_orb_18, C_GREY, BLK[b].label, 14, 12, 0,
             LV_TEXT_ALIGN_LEFT);
    tag[b] = ui_label(t, &dash_orb_14, C_DIM, "", w - 110, 14, 94,
                      LV_TEXT_ALIGN_RIGHT);

    unit_row[b] = ui_box(t, 0, h / 2 - 34, w, 60);
    lv_obj_set_flex_flow(unit_row[b], LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(unit_row[b], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(unit_row[b], 8, 0);
    val[b] = ui_label(unit_row[b], &obd_60, C_W, "--", 0, 0, 0,
                      LV_TEXT_ALIGN_LEFT);
    lv_obj_t *u = ui_label(unit_row[b], &dash_orb_18, C_GREY, BLK[b].unit, 0, 0,
                           0, LV_TEXT_ALIGN_LEFT);
    ui_unit_on_baseline(u);

    na[b] = ui_label(t, &dash_orb_18, C_DIM, "NOT SUPPORTED", 0, h / 2 - 10,
                     w, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(na[b], LV_OBJ_FLAG_HIDDEN);
    warn[b] = -1;
}

/* ------------------------------------------------------------ pages */
static void show_page(int p)
{
    page = p;
    for (int i = 0; i < 2; i++) {
        if (i == p) lv_obj_clear_flag(pg[i], LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag(pg[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(tab[i], i == p ? C_W : C_DIM, 0);
        lv_obj_set_style_border_width(tab[i], i == p ? 2 : 0, 0);
    }
    lv_label_set_text(title_lbl, p ? "DPF STATUS" : "OBD-II TEST");
}

void ui_obd_page(int p)
{
    if (pg[0]) show_page(p ? 1 : 0);
}

static void gesture_cb(lv_event_t *e)
{
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) show_page(!page);
}

static void tab_cb(lv_event_t *e)
{
    show_page((int)(intptr_t)lv_event_get_user_data(e));
}

static void build_tabs(lv_obj_t *scr)
{
    static const char *const NAME[2] = { "TEST", "DPF" };
    for (int i = 0; i < 2; i++) {
        tab[i] = ui_label(scr, &dash_orb_14, C_DIM, NAME[i], 215 + i * 90,
                          14, 80, LV_TEXT_ALIGN_CENTER);
        lv_obj_set_style_pad_ver(tab[i], 4, 0);
        lv_obj_set_style_border_side(tab[i], LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(tab[i], C_Y, 0);
        lv_obj_add_flag(tab[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(tab[i], 12);
        lv_obj_add_event_cb(tab[i], tab_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }
}

static void build_dpf(lv_obj_t *par)
{
    /* soot mass: a big particulate filter on the left that fills up */
    lv_obj_t *t = ui_rect(par, 8, 50, 380, 382, C_TILE);
    lv_obj_set_style_border_color(t, C_EDGE, 0);
    lv_obj_set_style_border_width(t, 2, 0);
    lv_obj_set_style_radius(t, 8, 0);
    ui_label(t, &dash_orb_18, C_GREY, "SOOT", 14, 12, 0, LV_TEXT_ALIGN_LEFT);
    ui_label(t, &dash_orb_14, C_DIM, "UDS 114F  calculated", 150, 14, 214,
             LV_TEXT_ALIGN_RIGHT);

    for (int i = 0; i < 2; i++) {             /* inlet and outlet pipe */
        dpf_pipe[i] = ui_rect(t, i ? DPF_X + DPF_W - 4 : 10, DPF_Y + 62,
                              DPF_X - 6, 46, C_GREY);
        lv_obj_set_style_radius(dpf_pipe[i], 4, 0);
    }
    dpf_body = ui_rect(t, DPF_X, DPF_Y, DPF_W, DPF_H, C_DPF_EMPTY);
    lv_obj_set_style_radius(dpf_body, 26, 0);
    lv_obj_set_style_border_width(dpf_body, DPF_BORDER, 0);
    lv_obj_set_style_border_color(dpf_body, C_GREY, 0);
    lv_obj_set_style_clip_corner(dpf_body, true, 0);
    lv_obj_set_style_shadow_color(dpf_body, lv_color_hex(0xFF9A1F), 0);
    lv_obj_set_style_shadow_width(dpf_body, 0, 0);
    /* soot, from the inlet side */
    dpf_fill = ui_rect(dpf_body, 0, 0, 0, DPF_IN_H, C_GREY);
    /* the filter's cells, cut out of the fill */
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 7; c++) {
            lv_obj_t *h = ui_rect(dpf_body, 18 + c * 38 - 9, 30 + r * 50 - 9,
                                  18, 18, C_DPF_CELL);
            lv_obj_set_style_radius(h, LV_RADIUS_CIRCLE, 0);
        }
    }

    soot_val = ui_label(t, &obd_60, C_W, "--", 0, DPF_Y + DPF_H + 18, 330,
                        LV_TEXT_ALIGN_CENTER);
    ui_label(t, &dash_orb_18, C_GREY, "g", 250, DPF_Y + DPF_H + 52, 40,
             LV_TEXT_ALIGN_LEFT);
    soot_state = ui_label(t, &dash_orb_14, C_DIM, "", 0, DPF_Y + DPF_H + 92,
                          376, LV_TEXT_ALIGN_CENTER);

    /* four tiles on the right */
    for (int i = 0; i < T_COUNT; i++) {
        lv_coord_t x = 400 + (i % 2) * 198, y = 50 + (i / 2) * 160;
        lv_obj_t *b = ui_rect(par, x, y, 190, 150, C_TILE);
        lv_obj_set_style_border_color(b, C_EDGE, 0);
        lv_obj_set_style_border_width(b, 2, 0);
        lv_obj_set_style_radius(b, 8, 0);
        ui_label(b, &dash_orb_14, C_GREY, TILE[i].label, 12, 12, 0,
                 LV_TEXT_ALIGN_LEFT);
        tile_val[i] = ui_label(b, &dash_orb_40, C_W, "--", 0, 52, 186,
                               LV_TEXT_ALIGN_CENTER);
        ui_label(b, &dash_orb_14, C_GREY, TILE[i].unit, 0, 104, 186,
                 LV_TEXT_ALIGN_CENTER);
    }

    /* regeneration: lit while the filter is hot */
    regen_box = ui_rect(par, 400, 372, 388, 60, C_TILE);
    lv_obj_set_style_border_color(regen_box, C_EDGE, 0);
    lv_obj_set_style_border_width(regen_box, 2, 0);
    lv_obj_set_style_radius(regen_box, 8, 0);
    regen_lbl = ui_label(regen_box, &dash_orb_18, C_DIM, "NO REGENERATION", 0,
                         19, 384, LV_TEXT_ALIGN_CENTER);

    dpf_foot = ui_label(par, &dash_lbl_13, C_GREY, "", 14, 448, 772,
                        LV_TEXT_ALIGN_LEFT);
}

static void update_dpf(const dash_data_t *d, bool live)
{
    char buf[200];
    const volatile float *v[T_COUNT] = {
        [T_MEAS] = &g_obd.dpf.soot_meas_g, [T_DP] = &g_obd.dpf.dp_hpa,
        [T_DIST] = &g_obd.dpf.dist_km,     [T_TEMP] = &g_obd.dpf.temp_c,
    };
    for (int i = 0; i < T_COUNT; i++) {
        float x = *v[i];
        if (!live || isnan(x)) ui_text(tile_val[i], "--");
        else {
            snprintf(buf, sizeof buf, TILE[i].fmt, x);
            ui_text(tile_val[i], buf);
        }
    }

    float s = live ? g_obd.dpf.soot_g : NAN;
    float temp = live ? g_obd.dpf.temp_c : NAN;
    int r = !isnan(temp) && temp >= REGEN_TEMP;

    if (isnan(s)) {
        ui_text(soot_val, "--");
        lv_obj_set_width(dpf_fill, 0);
        bool refused = g_obd.uds[OBD_UDS_DPF_SOOT] == UDS_REFUSED;
        ui_text(soot_state, refused ? "NOT SUPPORTED" : "");
    } else {
        snprintf(buf, sizeof buf, "%.1f", s);
        ui_text(soot_val, buf);
        lv_obj_set_width(dpf_fill, (lv_coord_t)lroundf(
            ui_clampf(s / SOOT_MAX, 0, 1) * DPF_IN_W));
        snprintf(buf, sizeof buf, "%.0f %% OF %.0f g", s / SOOT_WARN * 100,
                 SOOT_WARN);
        ui_text(soot_state, buf);
    }

    /* 0 clean, 1 getting full, 2 over the warn level, 3 regenerating */
    int lvl = r ? 3 : isnan(s) ? 0 : s >= SOOT_WARN ? 2 :
              s >= SOOT_WARN * 0.7f ? 1 : 0;
    if (lvl != soot_level) {
        static const uint32_t FILL[4] = { 0x8A9096, 0xE0A020, 0xE22424,
                                          0xFF9A1F };
        static const uint32_t EDGE[4] = { 0x8A9096, 0x8A9096, 0xE22424,
                                          0xFFB347 };
        soot_level = lvl;
        lv_obj_set_style_bg_color(dpf_fill, lv_color_hex(FILL[lvl]), 0);
        lv_obj_set_style_border_color(dpf_body, lv_color_hex(EDGE[lvl]), 0);
        for (int i = 0; i < 2; i++) {
            lv_obj_set_style_bg_color(dpf_pipe[i], lv_color_hex(EDGE[lvl]), 0);
        }
        /* regenerating: the whole filter glows */
        lv_obj_set_style_shadow_width(dpf_body, r ? 40 : 0, 0);
        lv_obj_set_style_bg_color(dpf_body, r ? lv_color_hex(0x3A2006)
                                              : C_DPF_EMPTY, 0);
        lv_obj_set_style_text_color(soot_val, lvl == 2 ? C_RED : C_W, 0);
    }

    if (r != regen_on) {
        regen_on = r;
        ui_text(regen_lbl, r ? "REGENERATING" : "NO REGENERATION");
        lv_obj_set_style_text_color(regen_lbl, r ? lv_color_hex(0x111111)
                                                 : C_DIM, 0);
        lv_obj_set_style_bg_color(regen_box, r ? lv_color_hex(0xFF9A1F)
                                               : C_TILE, 0);
    }

    char egt[16], rpm[16];
    if (!live || isnan(d->egt)) snprintf(egt, sizeof egt, "--");
    else snprintf(egt, sizeof egt, "%.0f \xC2\xB0" "C", d->egt);
    if (!live) snprintf(rpm, sizeof rpm, "--");
    else snprintf(rpm, sizeof rpm, "%.0f", d->rpm);
    snprintf(buf, sizeof buf,
             "EXHAUST %s   RPM %s   warn at %.0f g (a guess for now)   "
             "REGENERATING above %.0f \xC2\xB0" "C filter",
             egt, rpm, SOOT_WARN, REGEN_TEMP);
    ui_text(dpf_foot, buf);
}

static lv_obj_t *build(void)
{
    lv_obj_t *scr = ui_screen(C_BG);

    title_lbl = ui_label(scr, &dash_orb_18, C_Y, "OBD-II TEST", 14, 14,
                         0, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_letter_space(title_lbl, 2, 0);
    state_lbl = ui_label(scr, &dash_orb_14, C_GREY, "", 400, 17, 356,
                         LV_TEXT_ALIGN_RIGHT);
    build_tabs(scr);

    for (int i = 0; i < 2; i++) pg[i] = ui_box(scr, 0, 0, 800, 480);
    lv_obj_t *root = scr;
    scr = pg[0];                    /* the TEST page's widgets */

    const lv_coord_t w = 252, h = 186, gx = 12, gy = 10;
    for (int b = 0; b < B_COUNT; b++) {
        lv_coord_t x = 8 + (b % 3) * (w + gx);
        lv_coord_t y = 50 + (b / 3) * (h + gy);
        build_block(scr, b, x, y, w, h);
    }
    /* the engine block also carries the speed, small */
    speed_lbl = ui_label(scr, &dash_orb_18, C_GREY, "", 8 + 2 * (w + gx),
                         50 + h + gy + h - 40, w, LV_TEXT_ALIGN_CENTER);

    stats_lbl = ui_label(scr, &dash_lbl_13, C_GREY, "", 14, 438, 772,
                         LV_TEXT_ALIGN_LEFT);
    pids_lbl = ui_label(scr, &dash_lbl_13, C_DIM, "", 14, 458, 772,
                        LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(pids_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_height(pids_lbl, 18);

    build_dpf(pg[1]);
    scr = root;
    /* the pages cover the whole screen: keep the tabs tappable */
    for (int i = 0; i < 2; i++) lv_obj_move_foreground(tab[i]);
    show_page(page);

    shown_rx = UINT32_MAX;
    soot_level = regen_on = -1;
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);
    ui_corners(scr);
    return scr;
}

/* the VW measuring value standing in for a block's PID, -1 for none */
static int uds_of(int b)
{
    if (b == B_OIL) return OBD_UDS_OIL;
    if (b == B_EGT) return OBD_UDS_EGT;
    return -1;
}

static void show_block(int b, float v, bool known)
{
    uint8_t pid = pid_of(b);
    int u = uds_of(b);
    bool scanned = g_obd.state == OBD_POLLING;
    bool via_uds = u >= 0 && scanned && !obd_supported(pid);
    char buf[16];
    if (via_uds) snprintf(buf, sizeof buf, "UDS %04X", obd_uds_did[u]);
    else         snprintf(buf, sizeof buf, "PID %02X", pid);
    ui_text(tag[b], buf);

    bool unsupported = scanned && !obd_supported(pid) &&
                       (!via_uds || g_obd.uds[u] == UDS_REFUSED);
    if (via_uds && g_obd.uds[u] != UDS_OK) known = false;
    if (unsupported) {
        lv_obj_add_flag(unit_row[b], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(na[b], LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(unit_row[b], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(na[b], LV_OBJ_FLAG_HIDDEN);

    if (!known || isnan(v)) {
        ui_text(val[b], "--");
    } else if (BLK[b].dec == 2) {
        snprintf(buf, sizeof buf, "%.2f", v);
        ui_text(val[b], buf);
    } else {
        snprintf(buf, sizeof buf, "%d", (int)lroundf(v));
        ui_text(val[b], buf);
    }
    int w = known && BLK[b].lim >= 0 && ui_over(BLK[b].lim, v);
    if (w != warn[b]) {
        warn[b] = w;
        lv_obj_set_style_text_color(val[b], w ? C_RED : C_W, 0);
    }
}

static void update(const dash_data_t *d, int link)
{
    char buf[160];
    bool live = link != LINK_NONE;

    update_dpf(d, live);

    show_block(B_CLT, d->clt, live);
    show_block(B_OIL, d->oilt, live);
    show_block(B_BOOST, d->boost, live);
    show_block(B_IAT, d->iat, live);
    show_block(B_EGT, d->egt, live);
    show_block(B_ENGINE, d->rpm, live);
    snprintf(buf, sizeof buf, "%d km/h", (int)lroundf(d->speed));
    ui_text(speed_lbl, live ? buf : "");

    const char *st;
    lv_color_t sc;
    if (link == LINK_DEMO)                { st = "DEMO DATA";            sc = C_Y; }
    else if (g_obd.state == OBD_NO_ECU)   { st = "NO ECU ANSWER";        sc = C_RED; }
    else if (g_obd.state == OBD_SCANNING) { st = "SCANNING PIDs";        sc = C_Y; }
    else if (link == LINK_OK)             { st = "LINK OK  -  POLLING";  sc = C_GREEN; }
    else                                  { st = "WAITING FOR DATA";     sc = C_RED; }
    ui_text(state_lbl, st);
    lv_obj_set_style_text_color(state_lbl, sc, 0);

    /* counters: refreshed only when something moved, to spare the redraw */
    if (g_obd.rx != shown_rx || g_obd.state != OBD_POLLING) {
        shown_rx = g_obd.rx;
        char ecu[8];
        if (g_obd.ecu_id) snprintf(ecu, sizeof ecu, "%03X", g_obd.ecu_id);
        else              snprintf(ecu, sizeof ecu, "---");
        snprintf(buf, sizeof buf,
                 "ECU %s   500 kbit   TX %lu   RX %lu   TIMEOUT %lu   "
                 "REFUSED %lu   BARO %.0f kPa",
                 ecu, (unsigned long)g_obd.tx, (unsigned long)g_obd.rx,
                 (unsigned long)g_obd.timeouts, (unsigned long)g_obd.negative,
                 g_obd.baro_kpa);
        ui_text(stats_lbl, buf);

        /* once the particulate filter answers, its line replaces the PID
         * list: that one has done its job when the link came up */
        bool dpf = false;
        for (int i = OBD_UDS_DPF_DP; i <= OBD_UDS_DPF_TEMP; i++) {
            dpf |= g_obd.uds[i] == UDS_OK;
        }
        if (dpf) {
            char f[5][16];
            const float v[5] = { g_obd.dpf.soot_g, g_obd.dpf.soot_meas_g,
                                 g_obd.dpf.dp_hpa, g_obd.dpf.dist_km,
                                 g_obd.dpf.temp_c };
            const char *fm[5] = { "%.2f g", "%.2f g", "%.0f hPa", "%.1f km",
                                  "%.0f \xC2\xB0" "C" };
            for (int i = 0; i < 5; i++) {
                if (isnan(v[i])) snprintf(f[i], sizeof f[i], "--");
                else             snprintf(f[i], sizeof f[i], fm[i], v[i]);
            }
            snprintf(buf, sizeof buf,
                     "DPF  soot %s  (measured %s)   dp %s   since regen %s   "
                     "filter %s", f[0], f[1], f[2], f[3], f[4]);
            ui_text(pids_lbl, buf);
            lv_obj_set_style_text_color(pids_lbl, C_GREY, 0);
            return;
        }

        int n = snprintf(buf, sizeof buf, "PIDs:");
        for (int p = 1; p <= 0xA0 && n < (int)sizeof buf - 4; p++) {
            if (p % 0x20 == 0) continue;            /* the "next page" bits */
            if (obd_supported((uint8_t)p)) {
                n += snprintf(buf + n, sizeof buf - n, " %02X", p);
            }
        }
        ui_text(pids_lbl, n > 5 ? buf : "PIDs: none yet");
    }
}

const theme_t theme_obd = {
    .build = build, .update = update,
    .flash_r = 0,
};
