/* Round gauge UI, see ui_round.h: the gauge pages and what the screens
 * share. How a page looks is up to the look (ui_look_*.c, SETTINGS ->
 * LOOK); menu, DPF status and the regeneration popup: ui_round_dpf.c;
 * settings: ui_round_set.c.
 *
 * Here: which page is shown, the value smoothed onto the scale (so after a
 * page change or at power-up the needle or arc sweeps up on its own), the
 * peaks, the swipes, the long press for the menu, the double tap for night
 * the boot logo, and the page names and units in either language.
 *
 * Boot: the NOT STOCK badge (tools/gen_splash.py) fades in on black, holds,
 * and cross-fades into the gauges, which sweep up as they come in.
 */
#include "ui_round_int.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

_Static_assert(FACE_SIZE == RND_W, "faces.c was generated for another panel "
               "size: python tools/gen_faces.py --size RND_SIZE");
_Static_assert(FACE_COUNT == RND_COUNT, "pages in gen_faces.py and "
               "ui_round.h differ");

/* readout per page; ranges come from faces.h, limits from the settings */
static const struct {
    int  dec;
    bool peak;
} FMT[RND_COUNT] = {
    [RND_WATER]   = { 0, true },
    [RND_OIL]     = { 0, true },
    [RND_BOOST]   = { 2, true },
    [RND_INTAKE]  = { 0, true },
    [RND_EXHAUST] = { 0, true },
    [RND_RPM]     = { 0, false },
};

static const rnd_look_t *const LOOKS[RND_LOOK_COUNT] = {
    [RND_LOOK_NOTSTOCK] = &rnd_look_notstock,
    [RND_LOOK_RETRO]    = &rnd_look_retro,
    [RND_LOOK_FUTURO]   = &rnd_look_futuro,
};

/* value arc and its glow: width added, opacity */
static const struct { int extra; lv_opa_t opa; } GLOW[N_ARC] = {
    { 30, 18 }, { 14, 45 }, { 0, LV_OPA_COVER },
};

static const char *const NAME[RND_LANG_COUNT][RND_WARN_COUNT] = {
    [RND_LANG_EN] = {
        [RND_WATER] = "WATER", [RND_OIL] = "OIL", [RND_BOOST] = "BOOST",
        [RND_INTAKE] = "AIR", [RND_EXHAUST] = "EGT",
        [RND_RPM] = "ENGINE", [RND_WARN_SOOT] = "DPF SOOT",
    },
    [RND_LANG_CS] = {
        [RND_WATER] = "VODA", [RND_OIL] = "OLEJ", [RND_BOOST] = "TURBO",
        [RND_INTAKE] = "VZDUCH", [RND_EXHAUST] = "EGT",
        [RND_RPM] = "OTÁČKY", [RND_WARN_SOOT] = "SAZE DPF",
    },
};

const char *rnd_page_name(int pg)
{
    int l = g_rnd_set.lang < RND_LANG_COUNT ? g_rnd_set.lang : RND_LANG_EN;
    if (pg == RND_MULTI) return "MULTI";
    return pg >= 0 && pg < RND_WARN_COUNT ? NAME[l][pg] : "";
}

const char *rnd_unit(int pg)
{
    if (pg == RND_WARN_SOOT) return "g";
    if (pg == RND_RPM) return TR("rpm", "ot/min");
    return pg >= 0 && pg < RND_COUNT ? FACE_PAGE[pg].unit : "";
}

static lv_obj_t *scr;
static const rnd_look_t *look;
static int page;
static float shown = NAN;          /* smoothed position, 0..1 */
static float peak[RND_COUNT];

/* ---------------------------------------------------------------- shared */
void rnd_arcs(lv_obj_t *par, lv_obj_t *out[N_ARC])
{
    for (int i = 0; i < N_ARC; i++) {
        int w = FACE_ARC_W + GLOW[i].extra;
        int d = 2 * FACE_ARC_R + w;
        lv_obj_t *a = lv_arc_create(par);
        lv_obj_remove_style_all(a);
        lv_obj_set_size(a, d, d);
        lv_obj_center(a);
        lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
        lv_arc_set_rotation(a, FACE_START);
        lv_arc_set_bg_angles(a, 0, FACE_SWEEP);
        lv_arc_set_range(a, 0, ARC_MAX);
        lv_arc_set_value(a, 0);
        lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_arc_width(a, w, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(a, C_W, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(a, GLOW[i].opa, LV_PART_INDICATOR);
        out[i] = a;
    }
}

void rnd_arcs_set(lv_obj_t *a[N_ARC], float frac_1000, lv_color_t c)
{
    for (int i = 0; i < N_ARC; i++) {
        lv_arc_set_value(a[i], (int16_t)lroundf(frac_1000));
        lv_obj_set_style_arc_color(a[i], c, LV_PART_INDICATOR);
        /* nothing to show: no arc at all, not even the rounded start cap */
        if (frac_1000 < 1) lv_obj_add_flag(a[i], LV_OBJ_FLAG_HIDDEN);
        else               lv_obj_clear_flag(a[i], LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *rnd_zone_at(lv_obj_t *par, int r, int w, lv_color_t c)
{
    lv_obj_t *a = lv_arc_create(par);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, 2 * r + w, 2 * r + w);
    lv_obj_center(a);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(a, FACE_START);
    lv_obj_set_style_arc_width(a, w, LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, c, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    return a;
}

lv_obj_t *rnd_zone(lv_obj_t *par)
{
    return rnd_zone_at(par, FACE_ARC_R, FACE_GROOVE_W,
                       lv_color_hex(0x5A1414));
}

void rnd_zone_set(lv_obj_t *z, float frac)
{
    if (!(frac < 1)) {                    /* off the scale, or NAN */
        lv_obj_add_flag(z, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (frac < 0) frac = 0;
    lv_obj_clear_flag(z, LV_OBJ_FLAG_HIDDEN);
    lv_arc_set_bg_angles(z, (uint16_t)lroundf(FACE_SWEEP * frac), FACE_SWEEP);
}

void rnd_limits_changed(void)
{
    /* the looks follow warn_frac every frame; nothing to redo here */
}

lv_obj_t *rnd_label(lv_obj_t *par, const lv_font_t *f, lv_color_t c,
                    lv_coord_t y)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, RND_W);
    lv_obj_set_pos(l, 0, y);
    lv_label_set_text(l, "");
    return l;
}

void rnd_dots(lv_obj_t *par, lv_coord_t y, lv_obj_t *out[RND_PAGES])
{
    lv_obj_t *row = lv_obj_create(par);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 220, 10);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < RND_PAGES; i++) {
        lv_obj_t *d = lv_obj_create(row);
        lv_obj_remove_style_all(d);
        lv_obj_set_size(d, 8, 8);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE);
        out[i] = d;
    }
}

void rnd_dots_set(lv_obj_t *d[RND_PAGES], int pg, lv_color_t on,
                  lv_color_t off)
{
    int n = rnd_pages_shown(), at = rnd_page_pos(pg);
    for (int i = 0; i < RND_PAGES; i++) {
        if (i >= n) {
            lv_obj_add_flag(d[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(d[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(d[i], i == at ? 22 : 8);
        lv_obj_set_style_bg_color(d[i], i == at ? on : off, 0);
    }
}

/* ------------------------------------------------------ order, shown */
static bool shown_page(int pg)
{
    return !(g_rnd_set.hidden >> pg & 1);
}

int rnd_pages_shown(void)
{
    int n = 0;
    for (int i = 0; i < RND_PAGES; i++) n += shown_page(g_rnd_set.order[i]);
    return n;
}

int rnd_page_pos(int pg)
{
    int n = 0;
    for (int i = 0; i < RND_PAGES; i++) {
        int p = g_rnd_set.order[i];
        if (!shown_page(p)) continue;
        if (p == pg) return n;
        n++;
    }
    return -1;
}

/* the next shown page in order from pg, dir +1 or -1 (pg itself may be
 * hidden: then the first shown one after it) */
static int step_page(int pg, int dir)
{
    int at = 0;
    for (int i = 0; i < RND_PAGES; i++) {
        if (g_rnd_set.order[i] == pg) at = i;
    }
    for (int k = 1; k <= RND_PAGES; k++) {
        int p = g_rnd_set.order[((at + dir * k) % RND_PAGES + RND_PAGES) %
                                RND_PAGES];
        if (shown_page(p)) return p;
    }
    return pg;
}

lv_obj_t *rnd_gauge_screen(void)
{
    return scr;
}

/* ---------------------------------------------------------------- boot */
extern const lv_img_dsc_t *const boot_logo[];

static void opa_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa(obj, (lv_opa_t)v, 0);
}

static void boot(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *logo = lv_img_create(s);
    lv_img_set_src(logo, boot_logo[0]);
    lv_obj_center(logo);
    lv_obj_set_style_opa(logo, LV_OPA_TRANSP, 0);
    lv_scr_load(s);

    /* out of the dark: slow at first, like it is being lit */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, logo);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, RND_BOOT_IN_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    /* then into the gauges; the splash screen goes when done */
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, RND_BOOT_X_MS,
                     RND_BOOT_IN_MS + RND_BOOT_HOLD_MS, true);
}

/* ---------------------------------------------------------------- pages */
void ui_round_page(int p)
{
    page = rnd_page_valid(p) ? p : RND_WATER;
    if (!shown_page(page)) page = step_page(page, +1);
    shown = NAN;
    rnd_multi_show(page == RND_MULTI);
    if (page != RND_MULTI) look->page(page);
}

bool rnd_page_valid(int p)
{
    return (p >= 0 && p < RND_COUNT) || p == RND_MULTI;
}

void rnd_pages_changed(void)
{
    ui_round_page(page);          /* off a page just hidden; dots redone */
}

int ui_round_current(void)
{
    return page;
}

void rnd_look_apply(void)
{
    int l = g_rnd_set.look < RND_LOOK_COUNT ? g_rnd_set.look : 0;
    if (look == LOOKS[l]) return;
    look = LOOKS[l];
    lv_obj_clean(scr);
    look->build(scr);
    rnd_multi_build(scr);              /* on top, shown on its own page */
    ui_round_page(page);
    /* DPF, DIAGNOSTICS and the G-METER wear the look too. The look is picked on the LOOK
     * screen, but should either be up, it is not deleted while shown. */
    if (rnd_dpf_screen()) {
        lv_obj_t *act = lv_scr_act();
        bool on_dpf = act == rnd_dpf_screen();
        bool on_diag = act == rnd_diag_screen();
        bool on_g = act == rnd_g_screen();
        if (on_dpf || on_diag || on_g) lv_scr_load(scr);
        rnd_dpf_create();
        rnd_diag_create();
        rnd_g_create();
        if (on_dpf)  lv_scr_load(rnd_dpf_screen());
        if (on_diag) lv_scr_load(rnd_diag_screen());
        if (on_g)    lv_scr_load(rnd_g_screen());
    }
}

/* ------------------------------------------------------------ day / night */
static uint32_t last_tap, last_swipe;
static lv_obj_t *toast, *toast_lbl;

void rnd_backlight_apply(void)
{
    rnd_backlight(g_rnd_set.night ? g_rnd_set.night_level : 100);
}

void rnd_night_toggle(void)
{
    char buf[24];
    g_rnd_set.night = !g_rnd_set.night;
    rnd_backlight_apply();
    rnd_settings_save();

    if (!toast) {
        toast = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(toast);
        lv_obj_set_size(toast, 230, 60);
        lv_obj_center(toast);
        lv_obj_set_style_radius(toast, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(toast, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(toast, LV_OPA_80, 0);
        lv_obj_set_style_border_color(toast, C_EDGE, 0);
        lv_obj_set_style_border_width(toast, 2, 0);
        lv_obj_clear_flag(toast, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        toast_lbl = lv_label_create(toast);
        lv_obj_set_style_text_font(toast_lbl, &rnd_26, 0);
        lv_obj_set_style_text_color(toast_lbl, C_W, 0);
        lv_obj_set_style_text_letter_space(toast_lbl, 2, 0);
        lv_obj_center(toast_lbl);
    }
    if (g_rnd_set.night) {
        snprintf(buf, sizeof buf, "%s %d%%", TR("NIGHT", "NOC"),
                 g_rnd_set.night_level);
    } else {
        snprintf(buf, sizeof buf, "%s", TR("DAY", "DEN"));
    }
    lv_label_set_text(toast_lbl, buf);
    lv_obj_set_style_opa(toast, LV_OPA_COVER, 0);
    lv_obj_fade_out(toast, 400, 1000);
}

/* ------------------------------------------------------------ language */
/* Every screen sets its fixed text when it is built, so a new language
 * builds them all again. Run from lv_async_call: the tap that picked the
 * language comes from a button on one of the screens deleted here. */
static void lang_rebuild(void *arg)
{
    (void)arg;
    lv_obj_t *tmp = lv_obj_create(NULL);    /* nothing of ours is shown */
    lv_scr_load(tmp);
    if (toast) {
        lv_obj_del(toast);
        toast = NULL;
    }
    rnd_menu_create();                      /* each drops its old screens */
    rnd_dpf_create();
    rnd_set_create();
    rnd_diag_create();
    rnd_g_create();
    look = NULL;
    rnd_look_apply();
    rnd_set_open();
    lv_obj_del(tmp);
}

void rnd_lang_apply(void)
{
    lv_async_call(lang_rebuild, NULL);
}

void rnd_swiped(void)
{
    last_swipe = lv_tick_get();
    last_tap = 0;
}

void rnd_tap_cb(lv_event_t *e)
{
    (void)e;
    /* the end of a swipe is no tap */
    if (last_swipe && lv_tick_elaps(last_swipe) < 400) {
        last_tap = 0;
        return;
    }
    if (last_tap && lv_tick_elaps(last_tap) < 400) {
        last_tap = 0;
        rnd_night_toggle();
    } else {
        last_tap = lv_tick_get();
    }
}

static void gesture_cb(lv_event_t *e)
{
    (void)e;
    rnd_swiped();
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)  ui_round_page(step_page(page, +1));
    if (dir == LV_DIR_RIGHT) ui_round_page(step_page(page, -1));
}

static void long_cb(lv_event_t *e)
{
    (void)e;
    rnd_menu_open();
}

static void hold_cb(lv_event_t *e)
{
    lv_indev_t *in = lv_event_get_indev(e);
    if (!in) in = lv_indev_get_act();
    if (in) lv_indev_wait_release(in);
}

void rnd_on_long(lv_obj_t *obj, lv_event_cb_t cb)
{
    lv_obj_add_event_cb(obj, cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(obj, hold_cb, LV_EVENT_LONG_PRESSED, NULL);
}

void ui_round_create(bool boot_logo_on)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);
    rnd_on_long(scr, long_cb);
    lv_obj_add_event_cb(scr, rnd_tap_cb, LV_EVENT_SHORT_CLICKED, NULL);
    rnd_backlight_apply();

    /* start on the first page of the chosen order */
    page = rnd_page_valid(g_rnd_set.order[0]) ? g_rnd_set.order[0] : 0;
    for (int i = 0; i < RND_COUNT; i++) peak[i] = NAN;
    look = NULL;
    rnd_look_apply();

    rnd_menu_create();
    rnd_dpf_create();
    rnd_set_create();
    rnd_diag_create();
    rnd_g_create();
    if (boot_logo_on) boot();
    else              lv_scr_load(scr);
}

/* ---------------------------------------------------------------- values */
static void fmt(char *buf, size_t n, int pg, float v)
{
    if (FMT[pg].dec == 2) snprintf(buf, n, "%.2f", v);
    else                  snprintf(buf, n, "%d", (int)lroundf(v));
}

static float to_frac(const face_page_t *p, float v)
{
    float f = (v - p->lo) / (p->hi - p->lo);
    return f < 0 ? 0 : f > 1 ? 1 : f;
}

static void gauge_update(const rnd_data_t *d)
{
    char text[16], pk[24];

    for (int i = 0; i < RND_COUNT; i++) {
        float x = d->link ? d->v[i] : NAN;
        if (FMT[i].peak && !isnan(x) && (isnan(peak[i]) || x > peak[i])) {
            peak[i] = x;
        }
    }
    if (lv_scr_act() != scr) return;
    if (page == RND_MULTI) {
        rnd_multi_update(d);
        return;
    }
    const face_page_t *p = &FACE_PAGE[page];
    float v = d->link ? d->v[page] : NAN;

    rnd_view_t view = {
        .page = page, .valid = !isnan(v), .text = text, .peak = pk,
        .peak_frac = NAN, .regen = rnd_regen_active(),
    };
    float target = 0;
    if (isnan(v)) {
        snprintf(text, sizeof text, "--");
        snprintf(pk, sizeof pk, "%s", d->link ? TR("NOT READ", "NENAČTENO")
                                              : TR("NO DATA", "BEZ DAT"));
        view.alert = true;
        view.big = true;
    } else {
        fmt(text, sizeof text, page, v);
        int digits = 0;
        for (const char *c = text; *c; c++) digits += *c != '.';
        view.big = digits < 4;
        pk[0] = 0;
        if (FMT[page].peak && !isnan(peak[page])) {
            char n[16];
            fmt(n, sizeof n, page, peak[page]);
            snprintf(pk, sizeof pk, "MAX %s", n);
            view.peak_frac = to_frac(p, peak[page]);
        }
        target = to_frac(p, v);
    }

    /* smoothed: after a page change it sweeps up from the bottom */
    if (isnan(shown)) shown = 0;
    shown += (target - shown) * 0.25f;
    view.frac = shown;
    view.warn = !isnan(v) && v >= g_rnd_set.warn[page];
    view.warn_frac = (g_rnd_set.warn[page] - p->lo) / (p->hi - p->lo);
    look->draw(&view);
}

void ui_round_update(const rnd_data_t *d)
{
    rnd_regen_watch(d);
    gauge_update(d);
    rnd_dpf_update(d);
    rnd_diag_update(d);
    rnd_g_update(d);
}
