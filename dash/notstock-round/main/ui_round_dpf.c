/* Round gauge UI: the menu, the DPF status screen and the regeneration
 * popup. See ui_round.h.
 *
 * Menu: long press on any screen. GAUGES, DPF STATUS, SETTINGS
 * (ui_round_set.c). A long press in the menu goes back to the gauges.
 *
 * DPF status: the soot mass on the outer arc and as a filter drawing that
 * fills up, the measured soot, differential pressure, filter temperature and
 * distance since the last regeneration. Swipe or long press to leave.
 *
 * Regeneration (filter hotter than REGEN_TEMP, with some hysteresis): a
 * popup over whatever screen is up, three beeps; one beep and a second popup
 * when it ends. Tap closes a popup, or it goes by itself.
 */
#include "ui_round_int.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SOOT_MAX      40.0f    /* arc full scale and the filter's full width */
#define SOOT_WARN     (g_rnd_set.warn[RND_WARN_SOOT])
#define REGEN_HYST    50.0f    /* regeneration over below REGEN_TEMP - this */
#define POPUP_MS      10000

#define C_AMBER       lv_color_hex(0xE0A020)
#define C_DONE        lv_color_hex(0x3DDC84)
#define C_FILTER      lv_color_hex(0x24282D)
#define C_CELL        lv_color_hex(0x0B0C0E)

/* ------------------------------------------------------------- the menu */
static lv_obj_t *menu, *menu_dpf_icon;

static void go_gauges(lv_event_t *e)
{
    (void)e;
    lv_scr_load(rnd_gauge_screen());
}

static void go_settings(lv_event_t *e)
{
    (void)e;
    rnd_set_open();
}

static void go_dpf(lv_event_t *e)
{
    (void)e;
    lv_scr_load(rnd_dpf_screen());
}

void rnd_menu_open(void)
{
    lv_scr_load(menu);
}

static void go_menu(lv_event_t *e)
{
    (void)e;
    rnd_menu_open();
}

static lv_obj_t *menu_item(const char *text, lv_coord_t y, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_obj_create(menu);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 300, 70);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, C_PANEL, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, C_EDGE, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_bg_color(b, C_EDGE, LV_STATE_PRESSED);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &rnd_26, 0);
    lv_obj_set_style_text_color(l, cb ? C_W : C_DIM, 0);
    lv_obj_set_style_text_letter_space(l, 2, 0);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    else    lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
    return b;
}

void rnd_menu_create(void)
{
    if (menu) lv_obj_del(menu);           /* rebuilt for a new language */
    menu = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(menu, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(menu, LV_OPA_COVER, 0);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(menu, go_gauges, LV_EVENT_LONG_PRESSED, NULL);

    lv_obj_t *t = rnd_label(menu, &rnd_18, C_GREY, 64);
    lv_obj_set_style_text_letter_space(t, 4, 0);
    lv_label_set_text(t, "MENU");

    menu_item(TR("GAUGES", "BUDÍKY"), 110, go_gauges);
    lv_obj_t *d = menu_item(TR("DPF STATUS", "STAV DPF"), 200, go_dpf);
    lv_obj_align(lv_obj_get_child(d, 0), LV_ALIGN_CENTER, 26, 0);
    menu_dpf_icon = lv_img_create(d);
    lv_img_set_src(menu_dpf_icon, &icon_dpf_40);
    lv_obj_align(menu_dpf_icon, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_set_style_img_recolor(menu_dpf_icon,
                                 rnd_regen_active() ? C_REGEN : C_GREY, 0);
    lv_obj_set_style_img_recolor_opa(menu_dpf_icon, LV_OPA_COVER, 0);
    menu_item(TR("SETTINGS", "NASTAVENÍ"), 290, go_settings);

    lv_obj_t *h = rnd_label(menu, &rnd_18, C_DIM, 390);
    lv_label_set_text(h, TR("LONG PRESS: BACK", "PODRŽ: ZPĚT"));
}

/* ------------------------------------------------------ DPF status screen */
static lv_obj_t *dpf, *dpf_arc[N_ARC], *dpf_zone, *body, *fill, *pipe[2];
static float zone_at = NAN;       /* the warn level the zone was drawn for */
static lv_obj_t *soot_lbl, *meas_lbl, *pill, *pill_lbl;
enum { V_DP, V_TEMP, V_DIST, V_COUNT };
static lv_obj_t *v_lbl[V_COUNT];
static float dpf_shown = NAN;
static int dpf_level = -1;

lv_obj_t *rnd_dpf_screen(void)
{
    return dpf;
}

static void dpf_gesture(lv_event_t *e)
{
    rnd_swiped();                 /* a swipe: no tap */
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) go_gauges(NULL);
}

static lv_obj_t *ring(lv_obj_t *par, uint16_t from, uint16_t to,
                      lv_color_t c, int w)
{
    lv_obj_t *a = lv_arc_create(par);
    lv_obj_remove_style_all(a);
    int d = 2 * FACE_ARC_R + w;
    lv_obj_set_size(a, d, d);
    lv_obj_center(a);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(a, FACE_START);
    lv_arc_set_bg_angles(a, from, to);
    lv_obj_set_style_arc_width(a, w, LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, c, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    return a;
}

static lv_obj_t *rect(lv_obj_t *par, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                      lv_coord_t h, lv_color_t c)
{
    lv_obj_t *o = lv_obj_create(par);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

#define BODY_X   (CX - 100)
#define BODY_Y   84
#define BODY_W   200
#define BODY_H   96
#define BODY_B   5
#define FILL_W   (BODY_W - 2 * BODY_B)

static void popup_drop(void);

void rnd_dpf_create(void)
{
    if (dpf) {                            /* rebuilt for a new language */
        lv_obj_del(dpf);
        popup_drop();
        zone_at = dpf_shown = NAN;
        dpf_level = -1;
    }
    dpf = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(dpf, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(dpf, LV_OPA_COVER, 0);
    lv_obj_clear_flag(dpf, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(dpf, dpf_gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(dpf, go_menu, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(dpf, rnd_tap_cb, LV_EVENT_SHORT_CLICKED, NULL);

    /* groove and red zone like the gauge faces, then the soot arc */
    ring(dpf, 0, FACE_SWEEP, lv_color_hex(0x0C0E11), FACE_GROOVE_W);
    dpf_zone = rnd_zone(dpf);
    rnd_arcs(dpf, dpf_arc);

    lv_obj_t *t = rnd_label(dpf, &rnd_18, C_GREY, 58);
    lv_obj_set_style_text_letter_space(t, 4, 0);
    lv_label_set_text(t, TR("DPF STATUS", "STAV DPF"));

    /* the filter: pipes, body, soot filling from the inlet, cells */
    for (int i = 0; i < 2; i++) {
        pipe[i] = rect(dpf, i ? BODY_X + BODY_W - 4 : BODY_X - 26,
                       BODY_Y + 31, 30, 34, C_GREY);
        lv_obj_set_style_radius(pipe[i], 4, 0);
    }
    body = rect(dpf, BODY_X, BODY_Y, BODY_W, BODY_H, C_FILTER);
    lv_obj_set_style_radius(body, 18, 0);
    lv_obj_set_style_border_width(body, BODY_B, 0);
    lv_obj_set_style_border_color(body, C_GREY, 0);
    lv_obj_set_style_clip_corner(body, true, 0);
    lv_obj_set_style_shadow_color(body, C_REGEN, 0);
    fill = rect(body, 0, 0, 0, BODY_H - 2 * BODY_B, C_GREY);
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 5; c++) {
            lv_obj_t *h = rect(body, 22 + c * 36 - 7, 17 + r * 26 - 7, 14, 14,
                               C_CELL);
            lv_obj_set_style_radius(h, LV_RADIUS_CIRCLE, 0);
        }
    }

    soot_lbl = rnd_label(dpf, &rnd_84, C_W, 196);
    meas_lbl = rnd_label(dpf, &rnd_18, C_GREY, 290);

    const char *const CAP[V_COUNT] = {
        "DP hPa",
        TR("TEMP \xC2\xB0" "C", "TEPL \xC2\xB0" "C"),
        TR("REGEN km", "OD REG km"),
    };
    for (int i = 0; i < V_COUNT; i++) {
        lv_coord_t x = CX - 165 + i * 110;
        v_lbl[i] = rnd_label(dpf, &rnd_26, C_W, 322);
        lv_obj_set_width(v_lbl[i], 110);
        lv_obj_set_x(v_lbl[i], x);
        lv_obj_t *c = rnd_label(dpf, &rnd_18, C_DIM, 356);
        lv_obj_set_width(c, 130);         /* wider than the column: CS */
        lv_obj_set_x(c, x - 10);
        lv_label_set_text(c, CAP[i]);
    }

    pill = rect(dpf, CX - 110, 390, 220, 38, C_PANEL);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    pill_lbl = lv_label_create(pill);
    lv_obj_set_style_text_font(pill_lbl, &rnd_18, 0);
    lv_obj_set_style_text_color(pill_lbl, C_GREY, 0);
    lv_obj_set_style_text_letter_space(pill_lbl, 2, 0);
    lv_label_set_text(pill_lbl, "");
    lv_obj_center(pill_lbl);
}

static void fmt_or_dash(lv_obj_t *l, const char *fmt, float v, bool live)
{
    char buf[24];
    if (!live || isnan(v)) lv_label_set_text(l, "--");
    else {
        snprintf(buf, sizeof buf, fmt, v);
        lv_label_set_text(l, buf);
    }
}

void rnd_dpf_update(const rnd_data_t *d)
{
    if (lv_scr_act() != dpf) {
        dpf_shown = NAN;          /* sweep up again when it is opened */
        return;
    }
    bool live = d->link;
    float s = live ? d->dpf.soot_g : NAN;
    bool regen = rnd_regen_active();
    if (SOOT_WARN != zone_at) {           /* the limit is a setting */
        zone_at = SOOT_WARN;
        rnd_zone_set(dpf_zone, SOOT_WARN / SOOT_MAX);
        dpf_level = -1;
    }
    char buf[40];

    fmt_or_dash(soot_lbl, "%.1f", s, live);
    if (!live || isnan(d->dpf.soot_meas_g)) {
        lv_label_set_text(meas_lbl, live ? "g" : TR("NO DATA", "BEZ DAT"));
    } else {
        snprintf(buf, sizeof buf, "g   %s %.2f g", TR("MEASURED", "MĚŘENO"),
                 d->dpf.soot_meas_g);
        lv_label_set_text(meas_lbl, buf);
    }
    fmt_or_dash(v_lbl[V_DP], "%.0f", d->dpf.dp_hpa, live);
    fmt_or_dash(v_lbl[V_TEMP], "%.0f", d->dpf.temp_c, live);
    fmt_or_dash(v_lbl[V_DIST], "%.0f", d->dpf.dist_km, live);

    float frac = isnan(s) ? 0 : s / SOOT_MAX;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    if (isnan(dpf_shown)) dpf_shown = 0;
    dpf_shown += (frac * ARC_MAX - dpf_shown) * 0.25f;
    lv_obj_set_width(fill, (lv_coord_t)lroundf(dpf_shown / ARC_MAX * FILL_W));

    /* 0 clean, 1 filling, 2 over the warn level, 3 regenerating */
    int lvl = regen ? 3 : isnan(s) ? 0 : s >= SOOT_WARN ? 2 :
              s >= SOOT_WARN * 0.7f ? 1 : 0;
    static const uint32_t COL[4] = { 0xFFFFFF, 0xE0A020, 0xFF3030, 0xFF9A1F };
    static const uint32_t FIL[4] = { 0x8A9096, 0xE0A020, 0xFF3030, 0xFF9A1F };
    rnd_arcs_set(dpf_arc, dpf_shown, lv_color_hex(COL[lvl]));
    if (lvl != dpf_level) {
        dpf_level = lvl;
        lv_obj_set_style_bg_color(fill, lv_color_hex(FIL[lvl]), 0);
        lv_color_t edge = lvl == 3 ? C_REGEN : lvl == 2 ? C_RED : C_GREY;
        lv_obj_set_style_border_color(body, edge, 0);
        for (int i = 0; i < 2; i++) {
            lv_obj_set_style_bg_color(pipe[i], edge, 0);
        }
        lv_obj_set_style_shadow_width(body, regen ? 40 : 0, 0);
        lv_obj_set_style_bg_color(body, regen ? lv_color_hex(0x3A2006)
                                              : C_FILTER, 0);
        lv_obj_set_style_text_color(soot_lbl, lvl == 2 ? C_RED : C_W, 0);
        lv_obj_set_style_bg_color(pill, regen ? C_REGEN : C_PANEL, 0);
        lv_obj_set_style_text_color(pill_lbl, regen ? lv_color_black()
                                                    : C_GREY, 0);
    }
    if (regen) {
        lv_label_set_text(pill_lbl, TR("REGENERATING", "REGENERACE"));
    } else if (isnan(s)) {
        lv_label_set_text(pill_lbl, "");
    } else {
        snprintf(buf, sizeof buf, "%.0f %% %s %.0f g", s / SOOT_WARN * 100,
                 TR("OF", "Z"), SOOT_WARN);
        lv_label_set_text(pill_lbl, buf);
    }
}

/* ---------------------------------------------------------- regeneration */
static lv_obj_t *popup, *pop_icon, *pop_state, *pop_soot;
static lv_timer_t *pop_timer;
static bool regen;
static float soot_at_start = NAN;

bool rnd_regen_active(void)
{
    return regen;
}

static void popup_close(void)
{
    if (popup) lv_obj_add_flag(popup, LV_OBJ_FLAG_HIDDEN);
    if (pop_timer) {
        lv_timer_del(pop_timer);
        pop_timer = NULL;
    }
}

static void popup_drop(void)
{
    popup_close();
    if (popup) lv_obj_del(popup);
    popup = NULL;                 /* built again, in the new language */
}

static void popup_click(lv_event_t *e)
{
    (void)e;
    popup_close();
}

static void popup_timeout(lv_timer_t *t)
{
    (void)t;
    pop_timer = NULL;           /* one-shot: LVGL deletes it after this */
    popup_close();
}

static void popup_build(void)
{
    popup = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(popup);
    lv_obj_set_size(popup, RND_W, RND_H);
    lv_obj_set_style_radius(popup, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(popup, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_90, 0);
    lv_obj_set_style_border_width(popup, 12, 0);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(popup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(popup, popup_click, LV_EVENT_CLICKED, NULL);

    pop_icon = lv_img_create(popup);
    lv_img_set_src(pop_icon, &icon_dpf_120);
    lv_obj_align(pop_icon, LV_ALIGN_TOP_MID, 0, 62);
    lv_obj_set_style_img_recolor_opa(pop_icon, LV_OPA_COVER, 0);

    lv_obj_t *l = rnd_label(popup, &rnd_26, C_W, 204);
    lv_obj_set_style_text_letter_space(l, 3, 0);
    lv_label_set_text(l, "DPF");
    l = rnd_label(popup, &rnd_26, C_W, 244);
    lv_obj_set_style_text_letter_space(l, 3, 0);
    lv_label_set_text(l, TR("REGENERATION", "REGENERACE"));
    pop_state = rnd_label(popup, &rnd_26, C_REGEN, 284);
    lv_obj_set_style_text_letter_space(pop_state, 3, 0);
    pop_soot = rnd_label(popup, &rnd_18, C_GREY, 336);
    l = rnd_label(popup, &rnd_18, C_DIM, 392);
    lv_label_set_text(l, TR("TAP TO CLOSE", "ŤUKNI: ZAVŘÍT"));
    lv_obj_add_flag(popup, LV_OBJ_FLAG_HIDDEN);
}

static void popup_show(bool started, float soot)
{
    char buf[48];
    if (!popup) popup_build();
    lv_color_t c = started ? C_REGEN : C_DONE;
    lv_obj_set_style_border_color(popup, c, 0);
    lv_obj_set_style_img_recolor(pop_icon, c, 0);
    lv_obj_set_style_text_color(pop_state, c, 0);
    lv_label_set_text(pop_state, started ? TR("STARTED", "ZAHÁJENA")
                                         : TR("FINISHED", "UKONČENA"));
    if (isnan(soot)) {
        lv_label_set_text(pop_soot, "");
    } else if (started || isnan(soot_at_start)) {
        snprintf(buf, sizeof buf, "%s %.1f g", TR("SOOT", "SAZE"), soot);
        lv_label_set_text(pop_soot, buf);
    } else {
        snprintf(buf, sizeof buf, "%s %.1f g -> %.1f g", TR("SOOT", "SAZE"),
                 soot_at_start, soot);
        lv_label_set_text(pop_soot, buf);
    }
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_fade_in(popup, 200, 0);
    if (pop_timer) lv_timer_del(pop_timer);
    pop_timer = lv_timer_create(popup_timeout, POPUP_MS, NULL);
    lv_timer_set_repeat_count(pop_timer, 1);
}

void rnd_regen_watch(const rnd_data_t *d)
{
    float t = d->link ? d->dpf.temp_c : NAN;
    if (isnan(t)) return;                 /* no news: keep what we had */
    bool now = regen ? t >= REGEN_TEMP - REGEN_HYST : t >= REGEN_TEMP;
    if (now == regen) return;
    regen = now;
    float s = d->dpf.soot_g;
    if (regen) soot_at_start = s;
    popup_show(regen, s);
    if (g_rnd_set.beep) rnd_beep(regen ? 3 : 1);
    lv_obj_set_style_img_recolor(menu_dpf_icon, regen ? C_REGEN : C_GREY, 0);
}
