/* OBD-II test screen: plain blocks, made to prove the link to an OBD ECU
 * (VW T5.1 CAAC) before anything prettier is built on it.
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

static lv_obj_t *build(void)
{
    lv_obj_t *scr = ui_screen(C_BG);

    lv_obj_t *title = ui_label(scr, &dash_orb_18, C_Y, "OBD-II TEST", 14, 14,
                               0, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_letter_space(title, 2, 0);
    state_lbl = ui_label(scr, &dash_orb_14, C_GREY, "", 250, 17, 400,
                         LV_TEXT_ALIGN_RIGHT);

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

    shown_rx = UINT32_MAX;
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
