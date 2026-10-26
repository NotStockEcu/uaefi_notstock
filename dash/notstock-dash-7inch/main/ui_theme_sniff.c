/* SNIFF screen: what a tester (VCDS) reads from the car, see sniff.h.
 *
 * Top: frame count and every CAN ID seen. Table: one row per ECU and DID
 * that answered a UDS read, with the answer bytes, the first two of them as
 * a number, how often it came and how long ago. A value VCDS shows moves
 * together with one of these rows.
 */
#include "ui.h"
#include "ui_theme.h"
#include "sniff.h"

#include <stdio.h>
#include <string.h>

#include "esp_timer.h"

#define C_BG     lv_color_hex(0x000000)
#define C_W      lv_color_hex(0xFFFFFF)
#define C_GREY   lv_color_hex(0x8A9096)
#define C_DIM    lv_color_hex(0x5A5E63)
#define C_Y      lv_color_hex(0xF5C518)
#define C_GREEN  lv_color_hex(0x3DDC84)
#define C_RED    lv_color_hex(0xE22424)

enum { COL_ID, COL_DATA, COL_NUM, COL_COUNT, COL_AGE, COLS };
static const lv_coord_t COL_X[COLS] = { 14, 150, 530, 640, 720 };
static const lv_coord_t COL_W[COLS] = { 130, 375, 105, 75, 66 };
static const char *const COL_HEAD[COLS] = {
    "ECU   DID", "DATA (hex)", "BYTES 0-1", "COUNT", "AGE",
};

#define ROW_Y0  118
#define ROW_H   24

static lv_obj_t *state_lbl, *stats_lbl, *ids_lbl;
static lv_obj_t *cell[SNIFF_DIDS][COLS];

static lv_obj_t *build(void)
{
    lv_obj_t *scr = ui_screen(C_BG);

    lv_obj_t *title = ui_label(scr, &dash_orb_18, C_Y, "CAN SNIFF", 14, 14, 0,
                               LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_letter_space(title, 2, 0);
    state_lbl = ui_label(scr, &dash_orb_14, C_GREY, "", 250, 17, 400,
                         LV_TEXT_ALIGN_RIGHT);
    stats_lbl = ui_label(scr, &dash_lbl_13, C_GREY, "", 14, 46, 772,
                         LV_TEXT_ALIGN_LEFT);
    ids_lbl = ui_label(scr, &dash_lbl_13, C_DIM, "", 14, 66, 772,
                       LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(ids_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_height(ids_lbl, 18);

    for (int c = 0; c < COLS; c++) {
        ui_label(scr, &dash_lbl_13, C_GREY, COL_HEAD[c], COL_X[c], 94,
                 COL_W[c], c >= COL_NUM ? LV_TEXT_ALIGN_RIGHT
                                        : LV_TEXT_ALIGN_LEFT);
    }
    ui_rect(scr, 14, 113, 772, 1, C_DIM);
    for (int r = 0; r < SNIFF_DIDS; r++) {
        for (int c = 0; c < COLS; c++) {
            cell[r][c] = ui_label(scr, &dash_lbl_18, C_W, "", COL_X[c],
                                  ROW_Y0 + r * ROW_H, COL_W[c],
                                  c >= COL_NUM ? LV_TEXT_ALIGN_RIGHT
                                               : LV_TEXT_ALIGN_LEFT);
            lv_label_set_long_mode(cell[r][c], LV_LABEL_LONG_CLIP);
        }
    }
    ui_corners(scr);
    return scr;
}

static void update(const dash_data_t *d, int link)
{
    (void)d;
    char buf[200];
    int64_t now = esp_timer_get_time();

    if (link == LINK_OK) {
        ui_text(state_lbl, "LISTENING  -  NO TX");
        lv_obj_set_style_text_color(state_lbl, C_GREEN, 0);
    } else {
        ui_text(state_lbl, "BUS QUIET");
        lv_obj_set_style_text_color(state_lbl, C_RED, 0);
    }

    snprintf(buf, sizeof buf,
             "FRAMES %lu   IDs %d   UDS READS %lu   REFUSED %lu   "
             "full log: idf.py monitor",
             (unsigned long)g_sniff.frames, g_sniff.n_id,
             (unsigned long)g_sniff.uds_req, (unsigned long)g_sniff.uds_neg);
    ui_text(stats_lbl, buf);

    int n = snprintf(buf, sizeof buf, "IDs:");
    for (int i = 0; i < g_sniff.n_id && n < (int)sizeof buf - 5; i++) {
        n += snprintf(buf + n, sizeof buf - n, " %03X", g_sniff.id[i]);
    }
    ui_text(ids_lbl, g_sniff.n_id ? buf : "IDs: none yet");

    for (int r = 0; r < SNIFF_DIDS; r++) {
        if (r >= g_sniff.n_did) {
            for (int c = 0; c < COLS; c++) ui_text(cell[r][c], "");
            continue;
        }
        const volatile sniff_did_t *e = &g_sniff.did[r];
        snprintf(buf, sizeof buf, "%03X   %04X", e->ecu, e->did);
        ui_text(cell[r][COL_ID], buf);

        int k = e->len < SNIFF_DATA ? e->len : SNIFF_DATA;
        int m = 0;
        buf[0] = 0;
        for (int i = 0; i < k; i++) {
            m += snprintf(buf + m, sizeof buf - m, i ? " %02X" : "%02X",
                          e->data[i]);
        }
        if (e->len > SNIFF_DATA) snprintf(buf + m, sizeof buf - m, " ..");
        ui_text(cell[r][COL_DATA], buf);

        if (e->len >= 2) {
            snprintf(buf, sizeof buf, "%u", (e->data[0] << 8) | e->data[1]);
        } else if (e->len == 1) {
            snprintf(buf, sizeof buf, "%u", e->data[0]);
        } else {
            buf[0] = 0;
        }
        ui_text(cell[r][COL_NUM], buf);

        snprintf(buf, sizeof buf, "%lu", (unsigned long)e->count);
        ui_text(cell[r][COL_COUNT], buf);

        int age = (int)((now - e->last_us) / 1000000);
        if (age > 999) age = 999;
        snprintf(buf, sizeof buf, "%ds", age);
        ui_text(cell[r][COL_AGE], buf);
        lv_obj_set_style_text_color(cell[r][COL_DATA], age < 2 ? C_W : C_GREY,
                                    0);
    }
}

const theme_t theme_sniff = {
    .build = build, .update = update,
    .flash_r = 0,
};
