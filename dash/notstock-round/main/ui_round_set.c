/* Round gauge UI: settings. See ui_round.h.
 *
 * SETTINGS (from the menu): LOOK, PAGES (which gauges and in what order),
 * NIGHT (the backlight at night, the double
 * tap's level; tap for the next step), BEEP on / off (the regeneration
 * beeps; the popup comes either way), LIMITS, and the language (tap: English
 * / Czech; every screen is built again in it). A long press goes one level
 * back and stores the settings.
 *
 * LIMITS: one warn limit at a time, big, with - and + either side (hold to
 * repeat); swipe for the next one, the dots say which. Over its limit a
 * gauge's arc and readout go red and its red zone starts there; the DPF
 * limit is the soot mass the DPF status screen measures fullness against.
 */
#include "ui_round_int.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

rnd_settings_t g_rnd_set;

/* names and units: rnd_page_name(), rnd_unit() */
const rnd_limit_t RND_LIMIT[RND_WARN_COUNT] = {
    [RND_WATER]     = { 60,   130,  1,     105,  0 },
    [RND_OIL]       = { 80,   150,  1,     130,  0 },
    [RND_BOOST]     = { 0.5f, 2.5f, 0.05f, 2.2f, 2 },
    [RND_INTAKE]    = { 20,   100,  1,     60,   0 },
    [RND_EXHAUST]   = { 300,  1000, 10,    750,  0 },
    [RND_RPM]       = { 2000, 5000, 100,   4500, 0 },
    [RND_WARN_SOOT] = { 5,    40,   1,     24,   0 },
};

void rnd_settings_defaults(void)
{
    memset(&g_rnd_set, 0, sizeof g_rnd_set);
    g_rnd_set.look = RND_LOOK_NOTSTOCK;
    g_rnd_set.beep = true;
    g_rnd_set.night = false;
    g_rnd_set.night_level = 30;
    for (int i = 0; i < RND_COUNT; i++) g_rnd_set.order[i] = (uint8_t)i;
    g_rnd_set.order[RND_COUNT] = RND_MULTI;          /* last, after rpm */
    g_rnd_set.hidden = 0;
    /* MULTI: boost big, water, oil and exhaust small */
    g_rnd_set.multi[0] = RND_BOOST;
    g_rnd_set.multi[1] = RND_WATER;
    g_rnd_set.multi[2] = RND_OIL;
    g_rnd_set.multi[3] = RND_EXHAUST;
    g_rnd_set.lang = RND_LANG_EN;
    for (int i = 0; i < RND_WARN_COUNT; i++) {
        g_rnd_set.warn[i] = RND_LIMIT[i].def;
    }
}

/* ------------------------------------------------------------- helpers */
static lv_obj_t *screen(lv_event_cb_t on_long, const char *title)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    rnd_on_long(s, on_long);
    lv_obj_t *t = rnd_label(s, &rnd_18, C_GREY, 64);
    lv_obj_set_style_text_letter_space(t, 4, 0);
    lv_label_set_text(t, title);
    return s;
}

static lv_obj_t *pill(lv_obj_t *par, lv_coord_t y, lv_event_cb_t cb,
                      lv_obj_t **label)
{
    lv_obj_t *b = lv_obj_create(par);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 300, 56);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, C_PANEL, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, C_EDGE, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_bg_color(b, C_EDGE, LV_STATE_PRESSED);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &rnd_26, 0);
    lv_obj_set_style_text_color(l, C_W, 0);
    lv_obj_set_style_text_letter_space(l, 2, 0);
    lv_obj_center(l);
    *label = l;
    return b;
}

/* -------------------------------------------------------------- screens */
static lv_obj_t *set_scr, *look_scr, *lim_scr, *pg_scr;
static lv_obj_t *beep_lbl, *night_lbl, *lang_lbl, *look_pill[RND_LOOK_COUNT];

static void show_night(void)
{
    char buf[24];
    snprintf(buf, sizeof buf, "%s  %d%%", TR("NIGHT", "NOC"),
             g_rnd_set.night_level);
    lv_label_set_text(night_lbl, buf);
}

static void night_step(lv_event_t *e)
{
    (void)e;
    int l = g_rnd_set.night_level + 10;
    g_rnd_set.night_level = (uint8_t)(l > 50 ? 10 : l);
    if (g_rnd_set.night) rnd_backlight_apply();     /* see it change */
    show_night();
}

static const char *const LOOK_NAME[RND_LOOK_COUNT] = {
    [RND_LOOK_NOTSTOCK] = "NOTSTOCK",
    [RND_LOOK_RETRO]    = "RETRO",
    [RND_LOOK_FUTURO]   = "FUTURO",
};

static void show_beep(void)
{
    lv_label_set_text(beep_lbl, g_rnd_set.beep ? TR("BEEP   ON", "PÍPÁNÍ   ZAP")
                                               : TR("BEEP   OFF", "PÍPÁNÍ   VYP"));
    lv_obj_set_style_text_color(beep_lbl, g_rnd_set.beep ? C_W : C_DIM, 0);
}

void rnd_set_open(void)
{
    show_beep();
    show_night();
    lv_scr_load(set_scr);
}

/* the language's own name, so whoever cannot read the other one finds it */
static void lang_toggle(lv_event_t *e)
{
    (void)e;
    g_rnd_set.lang = (uint8_t)((g_rnd_set.lang + 1) % RND_LANG_COUNT);
    rnd_lang_apply();
}

static void set_back(lv_event_t *e)
{
    (void)e;
    rnd_settings_save();
    rnd_menu_open();
}

static void sub_back(lv_event_t *e)
{
    (void)e;
    rnd_settings_save();
    rnd_set_open();
}

static void beep_toggle(lv_event_t *e)
{
    (void)e;
    g_rnd_set.beep = !g_rnd_set.beep;
    if (g_rnd_set.beep) rnd_beep(1);      /* so you hear what you chose */
    show_beep();
}

/* LOOK: the picked one has a white rim; it applies at once */
static void show_look(void)
{
    for (int i = 0; i < RND_LOOK_COUNT; i++) {
        lv_obj_set_style_border_color(look_pill[i], i == g_rnd_set.look ?
                                      C_W : C_EDGE, 0);
    }
}

static void look_pick(lv_event_t *e)
{
    g_rnd_set.look = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    rnd_look_apply();
    show_look();
}

static void go_look(lv_event_t *e)
{
    (void)e;
    show_look();
    lv_scr_load(look_scr);
}

/* PAGES: one gauge at a time, in swipe order: shown or hidden, and moved
 * earlier (<) or later (>) */
static int pg_at;                     /* position in g_rnd_set.order */
static lv_obj_t *pg_icon, *pg_multi_icon, *pg_edit;
static lv_obj_t *pg_name, *pg_state, *pg_state_lbl, *pg_pos;
static lv_obj_t *pg_dot[RND_PAGES];

static void show_pages(void)
{
    char buf[32];
    int p = g_rnd_set.order[pg_at];
    bool on = !(g_rnd_set.hidden >> p & 1);
    bool multi = p == RND_MULTI;
    if (multi) {
        lv_obj_add_flag(pg_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pg_multi_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pg_edit, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_img_set_src(pg_icon, page_icon[p]);
        lv_obj_set_style_img_recolor(pg_icon, on ? C_W : C_DIM, 0);
        lv_obj_clear_flag(pg_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pg_multi_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pg_edit, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(pg_name, rnd_page_name(p));
    lv_obj_set_style_text_color(pg_name, on ? C_W : C_DIM, 0);
    lv_label_set_text(pg_state_lbl, on ? TR("SHOWN", "ZOBRAZENO")
                                       : TR("HIDDEN", "SKRYTO"));
    lv_obj_set_style_text_color(pg_state_lbl, on ? C_W : C_DIM, 0);
    lv_obj_set_style_border_color(pg_state, on ? C_W : C_EDGE, 0);
    snprintf(buf, sizeof buf, "%s %d / %d", TR("POSITION", "POZICE"),
             pg_at + 1, RND_PAGES);
    lv_label_set_text(pg_pos, buf);
    for (int i = 0; i < RND_PAGES; i++) {
        bool h = g_rnd_set.hidden >> g_rnd_set.order[i] & 1;
        lv_obj_set_width(pg_dot[i], i == pg_at ? 22 : 8);
        lv_obj_set_style_bg_color(pg_dot[i], i == pg_at ? C_W :
                                  h ? lv_color_hex(0x1C1F23) : C_DOT, 0);
    }
}

static void pg_toggle(lv_event_t *e)
{
    (void)e;
    int p = g_rnd_set.order[pg_at];
    bool on = !(g_rnd_set.hidden >> p & 1);
    if (on && rnd_pages_shown() <= 1) return;     /* keep one to look at */
    g_rnd_set.hidden ^= (uint8_t)(1u << p);
    rnd_pages_changed();
    show_pages();
}

static void pg_move(lv_event_t *e)
{
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    int to = pg_at + dir;
    if (to < 0 || to >= RND_PAGES) return;
    uint8_t t = g_rnd_set.order[to];
    g_rnd_set.order[to] = g_rnd_set.order[pg_at];
    g_rnd_set.order[pg_at] = t;
    pg_at = to;                       /* the selection travels with it */
    rnd_pages_changed();
    show_pages();
}

static void pg_gesture(lv_event_t *e)
{
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)  pg_at = (pg_at + 1) % RND_PAGES;
    if (dir == LV_DIR_RIGHT) pg_at = (pg_at + RND_PAGES - 1) % RND_PAGES;
    show_pages();
}

static void go_pages(lv_event_t *e)
{
    (void)e;
    show_pages();
    lv_scr_load(pg_scr);
}

void rnd_pages_open(void)
{
    go_pages(NULL);
}

static void go_multi_edit(lv_event_t *e)
{
    (void)e;
    rnd_multi_edit_open();
}

/* LIMITS */
static int lim;                       /* which one is shown */
static lv_obj_t *lim_name, *lim_val, *lim_unit, *lim_def;
static lv_obj_t *lim_dot[RND_WARN_COUNT];

static void show_limit(void)
{
    const rnd_limit_t *l = &RND_LIMIT[lim];
    char buf[24];
    lv_label_set_text(lim_name, rnd_page_name(lim));
    snprintf(buf, sizeof buf, "%.*f", l->dec, g_rnd_set.warn[lim]);
    lv_label_set_text(lim_val, buf);
    lv_label_set_text(lim_unit, rnd_unit(lim));
    snprintf(buf, sizeof buf, "%s %.*f", TR("DEFAULT", "VÝCHOZÍ"), l->dec,
             l->def);
    lv_label_set_text(lim_def, buf);
    for (int i = 0; i < RND_WARN_COUNT; i++) {
        lv_obj_set_width(lim_dot[i], i == lim ? 22 : 8);
        lv_obj_set_style_bg_color(lim_dot[i], i == lim ? C_W : C_DOT, 0);
    }
}

static void lim_step(lv_event_t *e)
{
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    const rnd_limit_t *l = &RND_LIMIT[lim];
    float v = g_rnd_set.warn[lim] + dir * l->step;
    v = roundf(v / l->step) * l->step;    /* no float drift */
    if (v < l->lo) v = l->lo;
    if (v > l->hi) v = l->hi;
    g_rnd_set.warn[lim] = v;
    rnd_limits_changed();
    show_limit();
}

static void lim_gesture(lv_event_t *e)
{
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)  lim = (lim + 1) % RND_WARN_COUNT;
    if (dir == LV_DIR_RIGHT) lim = (lim + RND_WARN_COUNT - 1) % RND_WARN_COUNT;
    show_limit();
}

static void go_limits(lv_event_t *e)
{
    (void)e;
    show_limit();
    lv_scr_load(lim_scr);
}

/* a round - or + button, the sign drawn from bars */
static void step_button(lv_obj_t *par, lv_coord_t x, int dir)
{
    lv_obj_t *b = lv_obj_create(par);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 84, 84);
    lv_obj_set_pos(b, x, CX - 42);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, C_PANEL, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, C_EDGE, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_bg_color(b, C_EDGE, LV_STATE_PRESSED);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(b, lim_step, LV_EVENT_CLICKED, (void *)(intptr_t)dir);
    lv_obj_add_event_cb(b, lim_step, LV_EVENT_LONG_PRESSED_REPEAT,
                        (void *)(intptr_t)dir);
    for (int i = 0; i < (dir > 0 ? 2 : 1); i++) {
        lv_obj_t *bar = lv_obj_create(b);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, i ? 6 : 30, i ? 30 : 6);
        lv_obj_center(bar);
        lv_obj_set_style_radius(bar, 3, 0);
        lv_obj_set_style_bg_color(bar, C_W, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void text_button(lv_obj_t *par, lv_coord_t x, const char *t,
                        lv_event_cb_t cb, int dir)
{
    lv_obj_t *b = lv_obj_create(par);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 84, 84);
    lv_obj_set_pos(b, x, CX - 42);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, C_PANEL, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, C_EDGE, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_bg_color(b, C_EDGE, LV_STATE_PRESSED);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *)(intptr_t)dir);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &rnd_26, 0);
    lv_obj_set_style_text_color(l, C_W, 0);
    lv_label_set_text(l, t);
    lv_obj_center(l);
}

#define SET_Y0   94          /* the settings pills: first one, spacing */
#define SET_DY   54
#define SET_H    48

void rnd_set_create(void)
{
    lv_obj_t *l;
    const char *back = TR("LONG PRESS: BACK", "PODRŽ: ZPĚT");

    /* rebuilt for a new language: drop the old screens (none is shown) */
    lv_obj_t *old[] = { set_scr, look_scr, pg_scr, lim_scr };
    for (size_t i = 0; i < sizeof old / sizeof old[0]; i++) {
        if (old[i]) lv_obj_del(old[i]);
    }

    set_scr = screen(set_back, TR("SETTINGS", "NASTAVENÍ"));
    pill(set_scr, SET_Y0 + 0 * SET_DY, go_look, &l);
    lv_label_set_text(l, TR("LOOK", "VZHLED"));
    pill(set_scr, SET_Y0 + 1 * SET_DY, go_pages, &l);
    lv_label_set_text(l, TR("PAGES", "STRÁNKY"));
    pill(set_scr, SET_Y0 + 2 * SET_DY, night_step, &night_lbl);
    pill(set_scr, SET_Y0 + 3 * SET_DY, beep_toggle, &beep_lbl);
    pill(set_scr, SET_Y0 + 4 * SET_DY, go_limits, &l);
    lv_label_set_text(l, TR("LIMITS", "LIMITY"));
    pill(set_scr, SET_Y0 + 5 * SET_DY, lang_toggle, &lang_lbl);
    lv_label_set_text(lang_lbl, TR("ENGLISH", "ČEŠTINA"));
    /* six pills: a little lower than the others (the title is child 0) */
    for (uint32_t i = 1; i < lv_obj_get_child_cnt(set_scr); i++) {
        lv_obj_set_height(lv_obj_get_child(set_scr, i), SET_H);
    }
    l = rnd_label(set_scr, &rnd_18, C_DIM, SET_Y0 + 6 * SET_DY + 4);
    lv_label_set_text(l, back);

    look_scr = screen(sub_back, TR("LOOK", "VZHLED"));
    for (int i = 0; i < RND_LOOK_COUNT; i++) {
        look_pill[i] = pill(look_scr, 110 + i * 90, look_pick, &l);
        lv_obj_remove_event_cb(look_pill[i], look_pick);
        lv_obj_add_event_cb(look_pill[i], look_pick, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        lv_label_set_text(l, LOOK_NAME[i]);
    }
    l = rnd_label(look_scr, &rnd_18, C_DIM, 390);
    lv_label_set_text(l, back);

    pg_scr = screen(sub_back, TR("PAGES", "STRÁNKY"));
    lv_obj_add_event_cb(pg_scr, pg_gesture, LV_EVENT_GESTURE, NULL);
    pg_icon = lv_img_create(pg_scr);
    lv_obj_align(pg_icon, LV_ALIGN_TOP_MID, 0, 98);
    lv_obj_set_style_img_recolor_opa(pg_icon, LV_OPA_COVER, 0);
    rnd_multi_icon(pg_scr, 98, &pg_multi_icon);
    pg_name = rnd_label(pg_scr, &rnd_26, C_W, 160);
    lv_obj_set_style_text_letter_space(pg_name, 3, 0);
    pg_state = pill(pg_scr, CX - 28, pg_toggle, &pg_state_lbl);
    lv_obj_set_width(pg_state, 230);     /* ZOBRAZENO */
    text_button(pg_scr, 20, "<", pg_move, -1);
    text_button(pg_scr, RND_W - 20 - 84, ">", pg_move, +1);
    pg_pos = rnd_label(pg_scr, &rnd_18, C_GREY, CX + 52);
    l = rnd_label(pg_scr, &rnd_18, C_DIM, CX + 84);
    lv_label_set_text(l, TR("< > MOVE    SWIPE: NEXT",
                            "< > POSUN    PŘEJEĎ: DALŠÍ"));
    pg_edit = pill(pg_scr, CX + 114, go_multi_edit, &l);
    lv_obj_set_size(pg_edit, 180, 46);
    lv_label_set_text(l, TR("EDIT", "UPRAVIT"));
    lv_obj_t *prow = lv_obj_create(pg_scr);
    lv_obj_remove_style_all(prow);
    lv_obj_set_size(prow, 240, 10);
    lv_obj_align(prow, LV_ALIGN_TOP_MID, 0, CX + 185);
    lv_obj_set_flex_flow(prow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(prow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(prow, 8, 0);
    lv_obj_clear_flag(prow, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < RND_PAGES; i++) {
        pg_dot[i] = lv_obj_create(prow);
        lv_obj_remove_style_all(pg_dot[i]);
        lv_obj_set_size(pg_dot[i], 8, 8);
        lv_obj_set_style_radius(pg_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(pg_dot[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(pg_dot[i], LV_OBJ_FLAG_CLICKABLE);
    }

    lim_scr = screen(sub_back, TR("LIMITS", "LIMITY"));
    lv_obj_add_event_cb(lim_scr, lim_gesture, LV_EVENT_GESTURE, NULL);
    lim_name = rnd_label(lim_scr, &rnd_26, C_W, 118);
    lv_obj_set_style_text_letter_space(lim_name, 3, 0);
    lim_val = rnd_label(lim_scr, &rnd_84, C_RED, CX - 50);
    lim_unit = rnd_label(lim_scr, &rnd_26, C_GREY, CX + 52);
    lim_def = rnd_label(lim_scr, &rnd_18, C_DIM, CX + 98);
    step_button(lim_scr, 20, -1);
    step_button(lim_scr, RND_W - 20 - 84, +1);
    l = rnd_label(lim_scr, &rnd_18, C_DIM, CX + 132);
    lv_label_set_text(l, TR("SWIPE: NEXT", "PŘEJEĎ: DALŠÍ"));

    lv_obj_t *row = lv_obj_create(lim_scr);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 240, 10);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, CX + 185);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < RND_WARN_COUNT; i++) {
        lim_dot[i] = lv_obj_create(row);
        lv_obj_remove_style_all(lim_dot[i]);
        lv_obj_set_size(lim_dot[i], 8, 8);
        lv_obj_set_style_radius(lim_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(lim_dot[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(lim_dot[i], LV_OBJ_FLAG_CLICKABLE);
    }

    rnd_multi_edit_create();
}

#ifdef RND_SIM
/* the sim's way into the settings screens */
void rnd_sim_settings(const char *which, int limit)
{
    if (strcmp(which, "settings") == 0) rnd_set_open();
    if (strcmp(which, "look") == 0)     go_look(NULL);
    if (strcmp(which, "pages") == 0) {
        pg_at = limit % RND_PAGES;
        go_pages(NULL);
    }
    if (strcmp(which, "limits") == 0) {
        lim = limit % RND_WARN_COUNT;
        go_limits(NULL);
    }
}
#endif
