/* Settings screen.
 *
 * Designed to be usable in a moving car: 56 px rows, 64 px wide buttons, no
 * sliders, no keyboard. Every value is a minus / plus pair with a step size
 * chosen so a long hold sweeps the range in a few seconds. Changes apply
 * live so the effect is visible while adjusting; SAVE writes to NVS.
 */
#include "ui_menu.h"
#include "settings.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "esp_idf_version.h"
#include "esp_system.h"

#define C_Y       lv_color_hex(0xF5C518)
#define C_W       lv_color_hex(0xFFFFFF)
#define C_GREY    lv_color_hex(0x8A9096)
#define C_LINE    lv_color_hex(0x3A3D42)
#define C_BTN     lv_color_hex(0x1B1D20)
#define C_BTN_ACT lv_color_hex(0x2E3237)
#define C_CARDBG  lv_color_hex(0x0D0E10)
#define C_RED     lv_color_hex(0xE22424)
#define C_GREEN   lv_color_hex(0x25C25A)
#define C_SUBTLE  lv_color_hex(0x50535A)

typedef enum { T_INT, T_BOOL, T_CHOICE, T_LIST } kind_t;

typedef struct {
    const char *label;
    const char *unit;
    kind_t kind;
    void *field;
    bool is16;                  /* field is uint16_t rather than uint8_t */
    int lo, hi, step;
    int scale;                  /* value shown = raw / scale */
    const char *choice[4];      /* T_CHOICE: raw lo and hi labels;
                                 * T_LIST: one label per value lo..hi */
} row_cfg_t;

/* The shift-flash rows set the flash; the limit rows below them set the
 * level at which that gauge's own readout turns red. */
static const row_cfg_t rows[] = {
 { "Shift flash",      "",    T_BOOL,   &g_set.flash_enable,    false, 0, 1, 1, 1, {0} },
 { "Shift flash at",   "rpm", T_INT,    &g_set.rpm_flash,       true,  0, 9000, 100, 1, {0} },
 { "Shift flash level","%",   T_INT,    &g_set.flash_intensity, false, 10, 100, 5, 1, {0} },
 { "Shift flash area", "",    T_LIST,   &g_set.flash_area,      false, 0, 1, 1, 1, { "Screen", "Rev counter" } },
 { "Shift flash colour","",   T_LIST,   &g_set.flash_colour,    false, 0, 3, 1, 1, { "Red", "White", "Blue", "Amber" } },
 { "Shift flash period","ms", T_INT,    &g_set.flash_period,    true,  80, 600, 20, 1, {0} },
 { "Water temp",       "\xC2\xB0" "C", T_INT, &g_set.clt_warn,  false, 60, 130, 1, 1, {0} },
 { "Intake air temp",  "\xC2\xB0" "C", T_INT, &g_set.iat_warn,  false, 20, 120, 1, 1, {0} },
 { "Boost limit",      "bar", T_INT,    &g_set.boost_warn,      true,  0, 250, 5, 100, {0} },
 { "AFR lean limit",   "",    T_INT,    &g_set.afr_lean_warn,   true,  0, 200, 1, 10, {0} },
 { "Brightness",       "%",   T_INT,    &g_set.brightness,      false, 15, 100, 5, 1, {0} },
 { "Night mode",       "",    T_BOOL,   &g_set.night,           false, 0, 1, 1, 1, {0} },
 { "Night dim",        "%",   T_INT,    &g_set.night_level,     false, 20, 80, 5, 1, {0} },
 { "Fuel",             "",    T_CHOICE, &g_set.stoich,          false, 98, 147, 49, 1, { "E85", "Petrol" } },
 { "Baro offset",      "bar", T_INT,    &g_set.baro,            true,  80, 110, 1, 100, {0} },
 { "Demo mode",        "",    T_BOOL,   &g_set.demo,            false, 0, 1, 1, 1, {0} },
};
#define NROWS ((int)(sizeof(rows) / sizeof(rows[0])))
#define TOGGLE_ROW 0

static lv_obj_t *scr_menu;
static lv_obj_t *val_lbl[NROWS];

static int row_get(const row_cfg_t *r)
{
    if (r->kind == T_BOOL) return *(bool *)r->field ? 1 : 0;
    return r->is16 ? *(uint16_t *)r->field : *(uint8_t *)r->field;
}

static void row_set(const row_cfg_t *r, int v)
{
    if (v < r->lo) v = r->lo;
    if (v > r->hi) v = r->hi;
    if (r->kind == T_BOOL)   *(bool *)r->field = v != 0;
    else if (r->is16)        *(uint16_t *)r->field = (uint16_t)v;
    else                     *(uint8_t *)r->field = (uint8_t)v;
}

static void row_text(const row_cfg_t *r, char *buf, size_t n)
{
    int v = row_get(r);
    switch (r->kind) {
    case T_BOOL:
        snprintf(buf, n, v ? "ON" : "OFF");
        break;
    case T_CHOICE:
        snprintf(buf, n, "%s", v >= r->hi ? r->choice[1] : r->choice[0]);
        break;
    case T_LIST:
        snprintf(buf, n, "%s", r->choice[v - r->lo]);
        break;
    default:
        if (r->scale == 100)      snprintf(buf, n, "%d.%02d", v / 100, v % 100);
        else if (r->scale == 10)  snprintf(buf, n, "%d.%d", v / 10, v % 10);
        else if (v == 0 && r->lo == 0) snprintf(buf, n, "OFF");
        else                      snprintf(buf, n, "%d", v);
        break;
    }
}

static void refresh_row(int i)
{
    char b[24];
    row_text(&rows[i], b, sizeof b);
    if (rows[i].unit[0] && rows[i].kind == T_INT && strcmp(b, "OFF") != 0) {
        size_t l = strlen(b);
        snprintf(b + l, sizeof b - l, " %s", rows[i].unit);
    }
    lv_label_set_text(val_lbl[i], b);
}

static void step_cb(lv_event_t *e)
{
    intptr_t packed = (intptr_t)lv_event_get_user_data(e);
    int i = (int)(packed >> 1);
    int dir = (packed & 1) ? 1 : -1;
    const row_cfg_t *r = &rows[i];

    if (r->kind == T_BOOL || r->kind == T_CHOICE) {
        row_set(r, row_get(r) >= r->hi ? r->lo : r->hi);
    } else if (r->kind == T_LIST) {
        row_set(r, row_get(r) >= r->hi ? r->lo : row_get(r) + 1);
    } else {
        row_set(r, row_get(r) + dir * r->step);
    }
    refresh_row(i);
    ui_apply_settings();
}

static void close_cb(lv_event_t *e)
{
    (void)e;
    settings_save();
    ui_show_dash();
}

static void revert_cb(lv_event_t *e)
{
    (void)e;
    settings_defaults();
    for (int i = 0; i < NROWS; i++) refresh_row(i);
    ui_apply_settings();
}

static lv_obj_t *mk_btn(lv_obj_t *par, const char *txt, lv_coord_t w,
                        lv_coord_t h, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = lv_btn_create(par);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, C_BTN, 0);
    lv_obj_set_style_bg_color(b, C_BTN_ACT, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(b, C_LINE, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &dash_lbl_18, 0);
    lv_obj_set_style_text_color(l, C_W, 0);
    lv_obj_center(l);
    return b;
}

void ui_menu_create(void)
{
    scr_menu = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr_menu);
    lv_obj_set_style_bg_color(scr_menu, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr_menu, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(scr_menu);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_font(title, &dash_lbl_18, 0);
    lv_obj_set_style_text_color(title, C_Y, 0);
    lv_obj_set_style_text_letter_space(title, 3, 0);
    lv_obj_set_pos(title, 16, 12);

    lv_obj_t *hint = lv_label_create(scr_menu);
    lv_label_set_text(hint, "flash is rev limit only  /  swipe to scroll");
    lv_obj_set_style_text_font(hint, &dash_lbl_13, 0);
    lv_obj_set_style_text_color(hint, C_GREY, 0);
    lv_obj_set_pos(hint, 140, 16);

    /* scrolling list */
    lv_obj_t *list = lv_obj_create(scr_menu);
    lv_obj_remove_style_all(list);
    lv_obj_set_pos(list, 8, 44);
    lv_obj_set_size(list, 784, 366);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    for (int i = 0; i < NROWS; i++) {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 760, 56);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(row, C_CARDBG, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, C_LINE, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 7, 0);

        lv_obj_t *l = lv_label_create(row);
        lv_label_set_text(l, rows[i].label);
        lv_obj_set_style_text_font(l, &dash_lbl_18, 0);
        lv_obj_set_style_text_color(l, C_W, 0);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 16, 0);

        val_lbl[i] = lv_label_create(row);
        lv_obj_set_style_text_font(val_lbl[i], &dash_lbl_18, 0);
        lv_obj_set_style_text_color(val_lbl[i], C_Y, 0);
        lv_obj_set_width(val_lbl[i], 190);
        lv_obj_set_style_text_align(val_lbl[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(val_lbl[i], LV_ALIGN_LEFT_MID, 330, 0);

        intptr_t dn = ((intptr_t)i << 1) | 0;
        intptr_t up = ((intptr_t)i << 1) | 1;
        lv_obj_t *b1 = mk_btn(row, "-", 64, 44, step_cb, (void *)dn);
        lv_obj_align(b1, LV_ALIGN_RIGHT_MID, -84, 0);
        lv_obj_t *b2 = mk_btn(row, "+", 64, 44, step_cb, (void *)up);
        lv_obj_align(b2, LV_ALIGN_RIGHT_MID, -12, 0);

        if (rows[i].kind != T_INT) {
            /* a toggle needs one button, not two */
            lv_obj_add_flag(b1, LV_OBJ_FLAG_HIDDEN);
            lv_obj_t *lbl = lv_obj_get_child(b2, 0);
            lv_label_set_text(lbl, "SET");
        }
        refresh_row(i);
    }

    lv_obj_t *save = mk_btn(scr_menu, "SAVE & CLOSE", 220, 52, close_cb, NULL);
    lv_obj_set_pos(save, 8, 418);
    lv_obj_set_style_border_color(save, C_GREEN, 0);

    lv_obj_t *def = mk_btn(scr_menu, "DEFAULTS", 170, 52, revert_cb, NULL);
    lv_obj_set_pos(def, 240, 418);
    lv_obj_set_style_border_color(def, C_RED, 0);

    /* build stamp, bottom right, out of the way of both buttons */
    char stamp[64];
    lv_snprintf(stamp, sizeof stamp, "NOT STOCK  v%s", DASH_VERSION);
    lv_obj_t *ver = lv_label_create(scr_menu);
    lv_label_set_text(ver, stamp);
    lv_obj_set_style_text_font(ver, &dash_lbl_13, 0);
    lv_obj_set_style_text_color(ver, C_GREY, 0);
    lv_obj_set_width(ver, 340);
    lv_obj_set_style_text_align(ver, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(ver, 452, 424);

    lv_snprintf(stamp, sizeof stamp, "built %s %s   LVGL %d.%d.%d   IDF %s",
                __DATE__, __TIME__, LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
                LVGL_VERSION_PATCH, esp_get_idf_version());
    lv_obj_t *bld = lv_label_create(scr_menu);
    lv_label_set_text(bld, stamp);
    lv_obj_set_style_text_font(bld, &dash_lbl_13, 0);
    lv_obj_set_style_text_color(bld, C_SUBTLE, 0);
    lv_obj_set_width(bld, 340);
    lv_obj_set_style_text_align(bld, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(bld, 452, 444);
}

lv_obj_t *ui_menu_screen(void)
{
    return scr_menu;
}

void ui_menu_refresh(void)
{
    for (int i = 0; i < NROWS; i++) refresh_row(i);
}
