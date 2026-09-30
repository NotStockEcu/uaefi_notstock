/* Round gauge UI: settings. See ui_round.h.
 *
 * SETTINGS (from the menu): LOOK, BEEP on / off (the regeneration beeps; the
 * popup comes either way), LIMITS. A long press goes one level back and
 * stores the settings.
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

const rnd_limit_t RND_LIMIT[RND_WARN_COUNT] = {
    [RND_WATER]     = { "WATER",    "\xC2\xB0" "C", 60,  130,  1,    105,  0 },
    [RND_OIL]       = { "OIL",      "\xC2\xB0" "C", 80,  150,  1,    130,  0 },
    [RND_BOOST]     = { "BOOST",    "bar",          0.5f, 2.5f, 0.05f, 2.2f, 2 },
    [RND_INTAKE]    = { "INTAKE",   "\xC2\xB0" "C", 20,  100,  1,    60,   0 },
    [RND_EXHAUST]   = { "EXHAUST",  "\xC2\xB0" "C", 300, 1000, 10,   750,  0 },
    [RND_RPM]       = { "ENGINE",   "rpm",          2000, 5000, 100, 4500, 0 },
    [RND_WARN_SOOT] = { "DPF SOOT", "g",            5,   40,   1,    24,   0 },
};

void rnd_settings_defaults(void)
{
    memset(&g_rnd_set, 0, sizeof g_rnd_set);
    g_rnd_set.look = RND_LOOK_NOTSTOCK;
    g_rnd_set.beep = true;
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
    lv_obj_add_event_cb(s, on_long, LV_EVENT_LONG_PRESSED, NULL);
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
    lv_obj_set_size(b, 300, 70);
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
static lv_obj_t *set_scr, *look_scr, *lim_scr;
static lv_obj_t *beep_lbl, *look_lbl, *look_pill;

static void show_beep(void)
{
    lv_label_set_text(beep_lbl, g_rnd_set.beep ? "BEEP   ON" : "BEEP   OFF");
    lv_obj_set_style_text_color(beep_lbl, g_rnd_set.beep ? C_W : C_DIM, 0);
}

void rnd_set_open(void)
{
    show_beep();
    lv_scr_load(set_scr);
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

/* LOOK: only NOTSTOCK so far */
static void show_look(void)
{
    bool sel = g_rnd_set.look == RND_LOOK_NOTSTOCK;
    lv_obj_set_style_border_color(look_pill, sel ? C_W : C_EDGE, 0);
    lv_label_set_text(look_lbl, "NOTSTOCK");
}

static void look_pick(lv_event_t *e)
{
    (void)e;
    g_rnd_set.look = RND_LOOK_NOTSTOCK;
    show_look();
}

static void go_look(lv_event_t *e)
{
    (void)e;
    show_look();
    lv_scr_load(look_scr);
}

/* LIMITS */
static int lim;                       /* which one is shown */
static lv_obj_t *lim_name, *lim_val, *lim_unit, *lim_def;
static lv_obj_t *lim_dot[RND_WARN_COUNT];

static void show_limit(void)
{
    const rnd_limit_t *l = &RND_LIMIT[lim];
    char buf[24];
    lv_label_set_text(lim_name, l->name);
    snprintf(buf, sizeof buf, "%.*f", l->dec, g_rnd_set.warn[lim]);
    lv_label_set_text(lim_val, buf);
    lv_label_set_text(lim_unit, l->unit);
    snprintf(buf, sizeof buf, "DEFAULT %.*f", l->dec, l->def);
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

void rnd_set_create(void)
{
    lv_obj_t *l;

    set_scr = screen(set_back, "SETTINGS");
    pill(set_scr, 110, go_look, &l);
    lv_label_set_text(l, "LOOK");
    pill(set_scr, 200, beep_toggle, &beep_lbl);
    pill(set_scr, 290, go_limits, &l);
    lv_label_set_text(l, "LIMITS");
    l = rnd_label(set_scr, &rnd_18, C_DIM, 390);
    lv_label_set_text(l, "LONG PRESS: BACK");

    look_scr = screen(sub_back, "LOOK");
    look_pill = pill(look_scr, 150, look_pick, &look_lbl);
    l = rnd_label(look_scr, &rnd_18, C_DIM, 250);
    lv_label_set_text(l, "MORE LOOKS TO COME");
    l = rnd_label(look_scr, &rnd_18, C_DIM, 390);
    lv_label_set_text(l, "LONG PRESS: BACK");

    lim_scr = screen(sub_back, "LIMITS");
    lv_obj_add_event_cb(lim_scr, lim_gesture, LV_EVENT_GESTURE, NULL);
    lim_name = rnd_label(lim_scr, &rnd_26, C_W, 118);
    lv_obj_set_style_text_letter_space(lim_name, 3, 0);
    lim_val = rnd_label(lim_scr, &rnd_84, C_RED, CX - 50);
    lim_unit = rnd_label(lim_scr, &rnd_26, C_GREY, CX + 52);
    lim_def = rnd_label(lim_scr, &rnd_18, C_DIM, CX + 98);
    step_button(lim_scr, 20, -1);
    step_button(lim_scr, RND_W - 20 - 84, +1);
    l = rnd_label(lim_scr, &rnd_18, C_DIM, CX + 132);
    lv_label_set_text(l, "SWIPE: NEXT");

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
}

#ifdef RND_SIM
/* the sim's way into the settings screens */
void rnd_sim_settings(const char *which, int limit)
{
    if (strcmp(which, "settings") == 0) rnd_set_open();
    if (strcmp(which, "look") == 0)     go_look(NULL);
    if (strcmp(which, "limits") == 0) {
        lim = limit % RND_WARN_COUNT;
        go_limits(NULL);
    }
}
#endif
