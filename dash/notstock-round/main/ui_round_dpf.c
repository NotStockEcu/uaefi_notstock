/* Round gauge UI: the menu, the DPF status screen and the regeneration
 * popup. See ui_round.h.
 *
 * Menu: long press on any screen. GAUGES, DPF STATUS, DIAGNOSTICS
 * (ui_round_diag.c), G-METER (ui_round_g.c), SETTINGS (ui_round_set.c). A long press in the menu goes back to the gauges.
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

static void go_diag(lv_event_t *e)
{
    (void)e;
    rnd_diag_open();
}

static void go_g(lv_event_t *e)
{
    (void)e;
    rnd_g_open();
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
    lv_obj_set_size(b, 300, 58);
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
    rnd_on_long(menu, go_gauges);

    lv_obj_t *t = rnd_label(menu, &rnd_18, C_GREY, 52);
    lv_obj_set_style_text_letter_space(t, 4, 0);
    lv_label_set_text(t, "MENU");

    menu_item(TR("GAUGES", "BUDÍKY"), 84, go_gauges);
    lv_obj_t *d = menu_item(TR("DPF STATUS", "STAV DPF"), 148, go_dpf);
    lv_obj_align(lv_obj_get_child(d, 0), LV_ALIGN_CENTER, 26, 0);
    menu_dpf_icon = lv_img_create(d);
    lv_img_set_src(menu_dpf_icon, &icon_dpf_40);
    lv_obj_align(menu_dpf_icon, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_set_style_img_recolor(menu_dpf_icon,
                                 rnd_regen_active() ? C_REGEN : C_GREY, 0);
    lv_obj_set_style_img_recolor_opa(menu_dpf_icon, LV_OPA_COVER, 0);
    menu_item(TR("DIAGNOSTICS", "DIAGNOSTIKA"), 212, go_diag);
    menu_item(TR("G-METER", "G-METR"), 276, go_g);
    menu_item(TR("SETTINGS", "NASTAVENÍ"), 340, go_settings);

    lv_obj_t *h = rnd_label(menu, &rnd_18, C_DIM, 410);
    lv_label_set_text(h, TR("LONG PRESS: BACK", "PODRŽ: ZPĚT"));
}

/* ------------------------------------------------------ DPF status screen */
/* Drawn in the look picked in SETTINGS, rebuilt when it changes:
 *   NOTSTOCK  the arc over the face's groove and a filter that fills up
 *   RETRO     a VDO-like dial 0..40 g (face_retro_dpf), red needle and band,
 *             readings in windows, a DPF tell-tale: amber over the limit,
 *             orange and glowing while it regenerates
 *   FUTURO    the ring of segments on the hex background, neon readings */
enum { ST_NOTSTOCK, ST_RETRO, ST_FUTURO };
static int dstyle;

static lv_obj_t *dpf, *dpf_arc[N_ARC], *dpf_zone, *body, *fill, *pipe[2];
static float zone_at = NAN;       /* the warn level the zone was drawn for */
static lv_obj_t *soot_lbl, *meas_lbl, *pill, *pill_lbl;
enum { V_DP, V_TEMP, V_DIST, V_COUNT };
static lv_obj_t *v_lbl[V_COUNT];
static float dpf_shown = NAN;
static int dpf_level = -1;
/* RETRO */
static lv_obj_t *needle, *lamp, *glow;
static lv_point_t np[2];
/* FUTURO */
static lv_obj_t *meter, *icon;
static lv_meter_indicator_t *seg_lit, *seg_zone;
static int seg_at;

#define R_SWEEP   180              /* RETRO: 0 g at 9 o'clock, 40 g at 3 */
#define F_SEGS    46

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

/* a label centred on x, its middle at y */
static lv_obj_t *label_mid(const lv_font_t *f, lv_color_t c, int x, int y,
                           int w)
{
    lv_obj_t *l = rnd_label(dpf, f, c, y - lv_font_get_line_height(f) / 2);
    lv_obj_set_width(l, w);
    lv_obj_set_x(l, x - w / 2);
    return l;
}

static lv_obj_t *background(const lv_img_dsc_t *img)
{
    lv_obj_t *bg = lv_img_create(dpf);
    lv_img_set_src(bg, img);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_CLICKABLE);
    return bg;
}

static const char *cap_text(int i)
{
    switch (i) {
    case V_DP:   return "DP hPa";
    case V_TEMP: return TR("TEMP \xC2\xB0" "C", "TEPL \xC2\xB0" "C");
    default:     return TR("REGEN km", "OD REG km");
    }
}

#define BODY_X   (CX - 100)
#define BODY_Y   84
#define BODY_W   200
#define BODY_H   96
#define BODY_B   5
#define FILL_W   (BODY_W - 2 * BODY_B)
#define COL_W    (RND_W * 110 / 480)   /* the three readings' columns */

static void build_notstock(void)
{
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

    for (int i = 0; i < V_COUNT; i++) {
        lv_coord_t x = CX - COL_W * 3 / 2 + i * COL_W;
        v_lbl[i] = rnd_label(dpf, &rnd_26, C_W, CX + 82);
        lv_obj_set_width(v_lbl[i], COL_W);
        lv_obj_set_x(v_lbl[i], x);
        lv_obj_t *c = rnd_label(dpf, &rnd_18, C_DIM, CX + 116);
        lv_obj_set_width(c, COL_W + 20);  /* wider than the column: CS */
        lv_obj_set_x(c, x - 10);
        lv_label_set_text(c, cap_text(i));
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

static void build_retro(void)
{
    background(&face_retro_dpf);
    /* the red band from the limit to 40 g, on the tick ring */
    dpf_zone = lv_arc_create(dpf);
    lv_obj_remove_style_all(dpf_zone);
    lv_obj_set_size(dpf_zone, 2 * RETRO_ZONE_R + RETRO_ZONE_W,
                    2 * RETRO_ZONE_R + RETRO_ZONE_W);
    lv_obj_center(dpf_zone);
    lv_obj_clear_flag(dpf_zone, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(dpf_zone, 180);
    lv_obj_set_style_arc_width(dpf_zone, RETRO_ZONE_W, LV_PART_MAIN);
    lv_obj_set_style_arc_color(dpf_zone, C_BAND, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(dpf_zone, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(dpf_zone, LV_OPA_TRANSP, LV_PART_INDICATOR);

    soot_lbl = label_mid(&rnd_barlow_46, C_W, CX,
                         CX + (RDPF_WIN_TOP + RDPF_WIN_BOT) / 2 + 2,
                         RDPF_WIN_W);
    meas_lbl = rnd_label(dpf, &rnd_barlow_20, C_INK_DIM, CX + 30);
    lv_obj_set_style_text_letter_space(meas_lbl, 2, 0);

    for (int i = 0; i < V_COUNT; i++) {
        int x = CX + (i - 1) * RDPF_SM_DX;
        v_lbl[i] = label_mid(&rnd_barlow_23, C_W, x,
                             CX + (RDPF_SM_TOP + RDPF_SM_BOT) / 2 + 1,
                             RDPF_SM_W);
        lv_obj_t *c = label_mid(&rnd_barlow_20, C_INK_DIM, x,
                                CX + RDPF_SM_BOT + 16, 110);
        lv_label_set_text(c, cap_text(i));
    }

    /* the tell-tale, with a glow behind it while it regenerates */
    glow = rect(dpf, CX - 34, CX + 126, 68, 68, C_REGEN);
    lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(glow, LV_OPA_20, 0);
    lv_obj_add_flag(glow, LV_OBJ_FLAG_HIDDEN);
    lamp = lv_img_create(dpf);
    lv_img_set_src(lamp, &icon_dpf_40);
    lv_obj_align(lamp, LV_ALIGN_TOP_MID, 0, CX + 140);
    lv_obj_set_style_img_recolor_opa(lamp, LV_OPA_COVER, 0);
    lv_obj_set_style_img_recolor(lamp, lv_color_hex(0x3A3A3A), 0);
    pill_lbl = rnd_label(dpf, &rnd_barlow_20, C_INK_DIM, CX + 184);
    lv_obj_set_style_text_letter_space(pill_lbl, 2, 0);

    /* needle and hub over everything */
    needle = lv_line_create(dpf);
    lv_obj_set_pos(needle, 0, 0);
    lv_obj_set_style_line_width(needle, 7, 0);
    lv_obj_set_style_line_color(needle, C_NEEDLE, 0);
    lv_obj_set_style_line_rounded(needle, true, 0);
    lv_obj_clear_flag(needle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *hub = rect(dpf, CX - 20, CX - 20, 40, 40,
                         lv_color_hex(0x111111));
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(hub, lv_color_hex(0x55575A), 0);
    lv_obj_set_style_border_width(hub, 2, 0);
}

static void build_futuro(void)
{
    background(&face_futuro_bg);
    meter = lv_meter_create(dpf);
    lv_obj_remove_style_all(meter);
    lv_obj_set_size(meter, 2 * FUTURO_SEG_R, 2 * FUTURO_SEG_R);
    lv_obj_center(meter);
    lv_obj_clear_flag(meter, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_width(meter, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(meter, 0, LV_PART_INDICATOR);
    lv_obj_set_style_text_opa(meter, LV_OPA_TRANSP, LV_PART_TICKS);
    lv_meter_scale_t *sc = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, sc, F_SEGS, 9, FUTURO_SEG_L, C_SEG_OFF);
    lv_meter_set_scale_major_ticks(meter, sc, 1000, 9, FUTURO_SEG_L,
                                   C_SEG_OFF, 0);
    lv_meter_set_scale_range(meter, sc, 0, ARC_MAX, FACE_SWEEP, FACE_START);
    seg_zone = lv_meter_add_scale_lines(meter, sc, C_SEG_ZONE, C_SEG_ZONE,
                                        false, 0);
    seg_lit = lv_meter_add_scale_lines(meter, sc, C_CYAN, C_MAGENTA, false, 0);
    lv_meter_set_indicator_start_value(meter, seg_lit, 0);
    lv_meter_set_indicator_end_value(meter, seg_lit, -1);
    seg_at = -2;

    icon = lv_img_create(dpf);
    lv_img_set_src(icon, &icon_dpf_40);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 86);
    lv_obj_set_style_img_recolor(icon, C_CYAN, 0);
    lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_t *t = rnd_label(dpf, &rnd_18, C_TEAL_DIM, 136);
    lv_obj_set_style_text_letter_space(t, 5, 0);
    lv_label_set_text(t, TR("DPF STATUS", "STAV DPF"));

    soot_lbl = rnd_label(dpf, &rnd_84, C_ICE, 170);
    meas_lbl = rnd_label(dpf, &rnd_18, C_TEAL_DIM, 268);
    lv_obj_set_style_text_letter_space(meas_lbl, 2, 0);

    for (int i = 0; i < V_COUNT; i++) {
        lv_coord_t x = CX - COL_W * 3 / 2 + i * COL_W;
        v_lbl[i] = rnd_label(dpf, &rnd_26, C_ICE, CX + 64);
        lv_obj_set_width(v_lbl[i], COL_W);
        lv_obj_set_x(v_lbl[i], x);
        lv_obj_t *c = rnd_label(dpf, &rnd_18, C_TEAL_DIM, CX + 98);
        lv_obj_set_width(c, COL_W + 20);
        lv_obj_set_x(c, x - 10);
        lv_label_set_text(c, cap_text(i));
    }

    pill = rect(dpf, CX - 110, 376, 220, 38, lv_color_black());
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pill, 2, 0);
    lv_obj_set_style_border_color(pill, C_TEAL_DIM, 0);
    pill_lbl = lv_label_create(pill);
    lv_obj_set_style_text_font(pill_lbl, &rnd_18, 0);
    lv_obj_set_style_text_color(pill_lbl, C_CYAN, 0);
    lv_obj_set_style_text_letter_space(pill_lbl, 2, 0);
    lv_label_set_text(pill_lbl, "");
    lv_obj_center(pill_lbl);
}

static void popup_drop(void);

void rnd_dpf_create(void)
{
    if (dpf) {                            /* rebuilt: language or look */
        lv_obj_del(dpf);
        popup_drop();
    }
    zone_at = dpf_shown = NAN;
    dpf_level = -1;
    dpf_zone = body = fill = pipe[0] = pipe[1] = pill = NULL;
    needle = lamp = glow = meter = icon = NULL;
    memset(dpf_arc, 0, sizeof dpf_arc);
    dstyle = g_rnd_set.look == RND_LOOK_RETRO ? ST_RETRO :
             g_rnd_set.look == RND_LOOK_FUTURO ? ST_FUTURO : ST_NOTSTOCK;

    dpf = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(dpf, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(dpf, LV_OPA_COVER, 0);
    lv_obj_clear_flag(dpf, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(dpf, dpf_gesture, LV_EVENT_GESTURE, NULL);
    rnd_on_long(dpf, go_menu);
    lv_obj_add_event_cb(dpf, rnd_tap_cb, LV_EVENT_SHORT_CLICKED, NULL);

    if (dstyle == ST_RETRO)       build_retro();
    else if (dstyle == ST_FUTURO) build_futuro();
    else                          build_notstock();
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

static void zone_draw(float frac)
{
    if (dstyle == ST_NOTSTOCK) {
        rnd_zone_set(dpf_zone, frac);
    } else if (dstyle == ST_RETRO) {
        if (!(frac < 1)) {
            lv_obj_add_flag(dpf_zone, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if (frac < 0) frac = 0;
        lv_obj_clear_flag(dpf_zone, LV_OBJ_FLAG_HIDDEN);
        lv_arc_set_bg_angles(dpf_zone, (uint16_t)lroundf(R_SWEEP * frac),
                             R_SWEEP);
    } else {
        lv_meter_set_indicator_start_value(meter, seg_zone, frac < 1 ?
            (int32_t)lroundf((frac < 0 ? 0 : frac) * ARC_MAX) : ARC_MAX + 1);
        lv_meter_set_indicator_end_value(meter, seg_zone, ARC_MAX);
    }
}

/* where the soot stands, 0..ARC_MAX */
static void level_draw(float at, int lvl)
{
    static const uint32_t COL[4] = { 0xFFFFFF, 0xE0A020, 0xFF3030, 0xFF9A1F };
    if (dstyle == ST_NOTSTOCK) {
        rnd_arcs_set(dpf_arc, at, lv_color_hex(COL[lvl]));
        lv_obj_set_width(fill, (lv_coord_t)lroundf(at / ARC_MAX * FILL_W));
    } else if (dstyle == ST_RETRO) {
        float a = (180 + at / ARC_MAX * R_SWEEP) * (float)M_PI / 180.0f;
        float c = cosf(a), s = sinf(a);
        np[0].x = (lv_coord_t)lroundf(CX - 14 * c);
        np[0].y = (lv_coord_t)lroundf(CX - 14 * s);
        np[1].x = (lv_coord_t)lroundf(CX + (RETRO_TICK_R - 8) * c);
        np[1].y = (lv_coord_t)lroundf(CX + (RETRO_TICK_R - 8) * s);
        lv_line_set_points(needle, np, 2);
    } else {
        int n = (int)lroundf(at / ARC_MAX * (F_SEGS - 1));
        if (at < 10) n = -1;
        if (n != seg_at) {
            seg_at = n;
            lv_meter_set_indicator_end_value(meter, seg_lit, n < 0 ? -1 :
                (int32_t)lroundf((float)n * ARC_MAX / (F_SEGS - 1)));
        }
    }
}

/* 0 clean, 1 filling, 2 over the warn level, 3 regenerating */
static void state_draw(int lvl)
{
    bool regen = lvl == 3;
    if (dstyle == ST_NOTSTOCK) {
        static const uint32_t FIL[4] = { 0x8A9096, 0xE0A020, 0xFF3030,
                                         0xFF9A1F };
        lv_obj_set_style_bg_color(fill, lv_color_hex(FIL[lvl]), 0);
        lv_color_t edge = regen ? C_REGEN : lvl == 2 ? C_RED : C_GREY;
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
    } else if (dstyle == ST_RETRO) {
        lv_obj_set_style_text_color(soot_lbl, lvl == 2 ? C_RED : C_W, 0);
        lv_obj_set_style_img_recolor(lamp, regen ? C_REGEN :
                                     lvl == 2 ? C_AMBER :
                                     lv_color_hex(0x3A3A3A), 0);
        if (regen) lv_obj_clear_flag(glow, LV_OBJ_FLAG_HIDDEN);
        else       lv_obj_add_flag(glow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(pill_lbl, regen ? C_REGEN : C_INK_DIM, 0);
    } else {
        lv_color_t warn = lvl == 2 ? C_SEG_WARN : C_CYAN;
        seg_lit->type_data.scale_lines.color_start =
            regen ? C_REGEN : lvl == 2 ? C_SEG_WARN : C_CYAN;
        seg_lit->type_data.scale_lines.color_end =
            regen ? C_REGEN : lvl == 2 ? C_SEG_WARN : C_MAGENTA;
        lv_obj_invalidate(meter);
        lv_obj_set_style_img_recolor(icon, regen ? C_REGEN : warn, 0);
        lv_obj_set_style_text_color(soot_lbl, lvl == 2 ? C_SEG_WARN : C_ICE,
                                    0);
        lv_obj_set_style_border_color(pill, regen ? C_REGEN : C_TEAL_DIM, 0);
        lv_obj_set_style_bg_opa(pill, regen ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(pill, C_REGEN, 0);
        lv_obj_set_style_text_color(pill_lbl, regen ? lv_color_black()
                                                    : C_CYAN, 0);
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
        zone_draw(SOOT_WARN / SOOT_MAX);
        dpf_level = -1;
    }
    char buf[40];

    fmt_or_dash(soot_lbl, "%.1f", s, live);
    /* RETRO has the unit printed on the dial */
    const char *g = dstyle == ST_RETRO ? "" : "g   ";
    if (!live) {
        lv_label_set_text(meas_lbl, TR("NO DATA", "BEZ DAT"));
    } else if (isnan(d->dpf.soot_meas_g)) {
        lv_label_set_text(meas_lbl, dstyle == ST_RETRO ? "" : "g");
    } else {
        snprintf(buf, sizeof buf, "%s%s %.2f g", g, TR("MEASURED", "MĚŘENO"),
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

    int lvl = regen ? 3 : isnan(s) ? 0 : s >= SOOT_WARN ? 2 :
              s >= SOOT_WARN * 0.7f ? 1 : 0;
    level_draw(dpf_shown, lvl);
    if (lvl != dpf_level) {
        dpf_level = lvl;
        state_draw(lvl);
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
    /* the labels are screen wide, but the popup's content starts inside
     * its coloured border: fit them to the content, or they sit off centre */
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(popup); i++) {
        lv_obj_t *c = lv_obj_get_child(popup, i);
        if (c != pop_icon) lv_obj_set_width(c, LV_PCT(100));
    }
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
