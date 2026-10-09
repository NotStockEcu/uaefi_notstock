/* The A4 gauge's menu, see ui_a4.h: a long press on a dial opens it.
 *   NASTAVENI: LIMITY, PORADI, ZPET.
 *   LIMITY: one dial at a time (swipe for the next): its limit, - and +
 *   (hold to repeat). Red from there on the dial.
 *   PORADI: one dial at a time: its place in the swipe, < and > to move it
 *   earlier or later, ZOBRAZENO / SKRYTO to show or leave it out.
 * A long press goes back (and stores the settings); in the menu, to the
 * dial. Black, white text, red where it is chosen, as the cluster. */
#include "ui_a4.h"

#include <stdint.h>
#include <stdio.h>

LV_FONT_DECLARE(a4_txt_22);
LV_FONT_DECLARE(a4_txt_30);
LV_FONT_DECLARE(a4_big_96);

#define C         (A4_SIZE / 2)
#define C_W       lv_color_hex(0xEEEEEC)
#define C_SMALL   lv_color_hex(0xA0A0A0)
#define C_EDGE    lv_color_hex(0x3A3A3E)
#define C_RED     lv_color_hex(0xF02A26)
#define C_DIM     lv_color_hex(0x5A5A5E)
#define C_DOT     lv_color_hex(0x3A3A3E)
#define DOTS_DY   152                /* the dots under the editors */

static lv_obj_t *menu_scr, *lim_scr, *ord_scr;

/* a screen change goes out whole, as a page change */
static void load(lv_obj_t *scr)
{
    a4_flip();
    lv_scr_load(scr);
}

/* ------------------------------------------------------------- widgets */
static lv_obj_t *screen(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s, lv_color_black(), 0);
    /* the red rim, as on the dials */
    lv_obj_t *rim = lv_obj_create(s);
    lv_obj_remove_style_all(rim);
    lv_obj_set_size(rim, 2 * 225, 2 * 225);
    lv_obj_center(rim);
    lv_obj_set_style_radius(rim, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(rim, 2, 0);
    lv_obj_set_style_border_color(rim, C_RED, 0);
    lv_obj_set_style_shadow_width(rim, 18, 0);
    lv_obj_set_style_shadow_color(rim, lv_color_hex(0xA00E16), 0);
    lv_obj_clear_flag(rim, LV_OBJ_FLAG_CLICKABLE);
    return s;
}

static lv_obj_t *text(lv_obj_t *par, const lv_font_t *f, lv_color_t c,
                      const char *t, lv_coord_t dy)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_label_set_text(l, t);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, dy);
    return l;
}

/* a pill to tap: grey edge, red while pressed */
static lv_obj_t *pill(lv_obj_t *par, const char *t, lv_coord_t w, lv_coord_t h,
                      lv_coord_t dx, lv_coord_t dy, lv_obj_t **lbl)
{
    lv_obj_t *b = lv_obj_create(par);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, w, h);
    lv_obj_align(b, LV_ALIGN_CENTER, dx, dy);
    lv_obj_set_style_radius(b, h / 2, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, C_EDGE, 0);
    lv_obj_set_style_border_color(b, C_RED, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1A0808), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &a4_txt_30, 0);
    lv_obj_set_style_text_color(l, C_W, 0);
    lv_label_set_text(l, t);
    lv_obj_center(l);
    if (lbl) *lbl = l;
    return b;
}

static void dots_make(lv_obj_t *par, lv_obj_t **dots, lv_coord_t dy)
{
    for (int i = 0; i < A4_PAGES; i++) {
        lv_obj_t *o = lv_obj_create(par);
        lv_obj_remove_style_all(o);
        lv_obj_set_size(o, 8, 8);
        lv_obj_set_style_radius(o, 4, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(o, C_DOT, 0);
        lv_obj_align(o, LV_ALIGN_CENTER, -(A4_PAGES - 1) * 7 + i * 14, dy);
        lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
        dots[i] = o;
    }
}

static void dots_set(lv_obj_t **dots, int at)
{
    for (int i = 0; i < A4_PAGES; i++) {
        lv_obj_set_width(dots[i], i == at ? 20 : 8);
        lv_obj_align(dots[i], LV_ALIGN_CENTER, -(A4_PAGES - 1) * 7 + i * 14, DOTS_DY);
        lv_obj_set_style_bg_color(dots[i], i == at ? C_RED : C_DOT, 0);
    }
}

static int swipe_dir(void)
{
    lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
    return d == LV_DIR_LEFT ? 1 : d == LV_DIR_RIGHT ? -1 : 0;
}

/* ---------------------------------------------------------------- menu */
static void go_gauge(lv_event_t *e)
{
    (void)e;
    load(a4_gauge_screen());
}

static void back_to_menu(lv_event_t *e)
{
    (void)e;
    a4_settings_save();
    a4_settings_changed();
    load(menu_scr);
}

/* --------------------------------------------------------------- limits */
static int lim_at;                    /* the dial, A4_* */
static lv_obj_t *lim_name, *lim_val, *lim_unit, *lim_dots[A4_PAGES];

static void lim_show(void)
{
    const a4_limit_t *l = &A4_LIMIT[lim_at];
    char buf[24];
    lv_label_set_text(lim_name, l->name);
    snprintf(buf, sizeof buf, "%.*f", l->dec, g_a4_set.warn[lim_at]);
    lv_label_set_text(lim_val, buf);
    lv_label_set_text(lim_unit, l->unit);
    dots_set(lim_dots, lim_at);
}

static void lim_step(lv_event_t *e)
{
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    const a4_limit_t *l = &A4_LIMIT[lim_at];
    float v = g_a4_set.warn[lim_at] + dir * l->step;
    if (v < l->lo) v = l->lo;
    if (v > l->hi) v = l->hi;
    g_a4_set.warn[lim_at] = v;
    lim_show();
}

static void lim_gesture(lv_event_t *e)
{
    (void)e;
    int d = swipe_dir();
    if (!d) return;
    lim_at = (lim_at + d + A4_PAGES) % A4_PAGES;
    lim_show();
}

static void go_limits(lv_event_t *e)
{
    (void)e;
    lim_at = ui_a4_current();
    lim_show();
    load(lim_scr);
}

static void lim_create(void)
{
    lim_scr = screen();
    text(lim_scr, &a4_txt_22, C_SMALL, "LIMIT", -168);
    lim_name = text(lim_scr, &a4_txt_30, C_W, "", -128);
    lim_val = text(lim_scr, &a4_big_96, C_RED, "", -42);
    lv_obj_set_width(lim_val, 300);
    lv_obj_set_style_text_align(lim_val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lim_val, LV_ALIGN_CENTER, 0, -42);
    lim_unit = text(lim_scr, &a4_txt_22, C_SMALL, "", 20);
    lv_obj_t *m = pill(lim_scr, "-", 96, 72, -66, 88, NULL);
    lv_obj_t *p = pill(lim_scr, "+", 96, 72, 66, 88, NULL);
    lv_obj_add_event_cb(m, lim_step, LV_EVENT_PRESSED, (void *)(intptr_t)-1);
    lv_obj_add_event_cb(m, lim_step, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)-1);
    lv_obj_add_event_cb(p, lim_step, LV_EVENT_PRESSED, (void *)(intptr_t)1);
    lv_obj_add_event_cb(p, lim_step, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)1);
    dots_make(lim_scr, lim_dots, DOTS_DY);
    text(lim_scr, &a4_txt_22, C_DIM, "PODR\xC5\xBD: ZP\xC4\x9AT", 186);
    lv_obj_add_event_cb(lim_scr, lim_gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(lim_scr, back_to_menu, LV_EVENT_LONG_PRESSED, NULL);
}

/* ---------------------------------------------------------------- order */
static int ord_at;                    /* position in g_a4_set.order */
static lv_obj_t *ord_name, *ord_pos, *ord_state, *ord_state_lbl;
static lv_obj_t *ord_dots[A4_PAGES];

static int shown_count(void)
{
    int n = 0;
    for (int i = 0; i < A4_PAGES; i++) n += !(g_a4_set.hidden >> i & 1);
    return n;
}

static void ord_show(void)
{
    char buf[32];
    int p = g_a4_set.order[ord_at];
    bool on = !(g_a4_set.hidden >> p & 1);
    lv_label_set_text(ord_name, A4_LIMIT[p].name);
    lv_obj_set_style_text_color(ord_name, on ? C_W : C_DIM, 0);
    snprintf(buf, sizeof buf, "POZICE %d / %d", ord_at + 1, A4_PAGES);
    lv_label_set_text(ord_pos, buf);
    lv_label_set_text(ord_state_lbl, on ? "ZOBRAZENO" : "SKRYTO");
    lv_obj_set_style_text_color(ord_state_lbl, on ? C_W : C_DIM, 0);
    lv_obj_set_style_border_color(ord_state, on ? C_RED : C_EDGE, 0);
    dots_set(ord_dots, ord_at);
    for (int i = 0; i < A4_PAGES; i++) {
        bool h = g_a4_set.hidden >> g_a4_set.order[i] & 1;
        if (i != ord_at && h) lv_obj_set_style_bg_color(ord_dots[i], lv_color_hex(0x18181A), 0);
    }
}

static void ord_toggle(lv_event_t *e)
{
    (void)e;
    int p = g_a4_set.order[ord_at];
    bool on = !(g_a4_set.hidden >> p & 1);
    if (on && shown_count() <= 1) return;        /* keep one to look at */
    g_a4_set.hidden ^= (uint8_t)(1u << p);
    ord_show();
}

static void ord_move(lv_event_t *e)
{
    int dir = (int)(intptr_t)lv_event_get_user_data(e);
    int to = ord_at + dir;
    if (to < 0 || to >= A4_PAGES) return;
    uint8_t t = g_a4_set.order[to];
    g_a4_set.order[to] = g_a4_set.order[ord_at];
    g_a4_set.order[ord_at] = t;
    ord_at = to;                     /* the selection travels with it */
    ord_show();
}

static void ord_gesture(lv_event_t *e)
{
    (void)e;
    int d = swipe_dir();
    if (!d) return;
    ord_at = (ord_at + d + A4_PAGES) % A4_PAGES;
    ord_show();
}

static void go_order(lv_event_t *e)
{
    (void)e;
    ord_at = 0;
    for (int i = 0; i < A4_PAGES; i++) {
        if (g_a4_set.order[i] == ui_a4_current()) ord_at = i;
    }
    ord_show();
    load(ord_scr);
}

static void ord_create(void)
{
    ord_scr = screen();
    text(ord_scr, &a4_txt_22, C_SMALL, "PO\xC5\x98" "AD\xC3\x8D", -168);
    ord_name = text(ord_scr, &a4_txt_30, C_W, "", -110);
    ord_pos = text(ord_scr, &a4_txt_22, C_SMALL, "", -58);
    ord_state = pill(ord_scr, "", 240, 60, 0, 6, &ord_state_lbl);
    lv_obj_add_event_cb(ord_state, ord_toggle, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = pill(ord_scr, "<", 96, 72, -66, 92, NULL);
    lv_obj_t *r = pill(ord_scr, ">", 96, 72, 66, 92, NULL);
    lv_obj_add_event_cb(l, ord_move, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
    lv_obj_add_event_cb(r, ord_move, LV_EVENT_CLICKED, (void *)(intptr_t)1);
    dots_make(ord_scr, ord_dots, DOTS_DY);
    text(ord_scr, &a4_txt_22, C_DIM, "PODR\xC5\xBD: ZP\xC4\x9AT", 186);
    lv_obj_add_event_cb(ord_scr, ord_gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(ord_scr, back_to_menu, LV_EVENT_LONG_PRESSED, NULL);
}

/* ----------------------------------------------------------------- menu */
void a4_menu_create(void)
{
    menu_scr = screen();
    text(menu_scr, &a4_txt_22, C_SMALL, "NASTAVEN\xC3\x8D", -150);
    lv_obj_t *b = pill(menu_scr, "LIMITY", 260, 62, 0, -78, NULL);
    lv_obj_add_event_cb(b, go_limits, LV_EVENT_CLICKED, NULL);
    b = pill(menu_scr, "PO\xC5\x98" "AD\xC3\x8D", 260, 62, 0, -4, NULL);
    lv_obj_add_event_cb(b, go_order, LV_EVENT_CLICKED, NULL);
    b = pill(menu_scr, "ZP\xC4\x9AT", 260, 62, 0, 70, NULL);
    lv_obj_add_event_cb(b, go_gauge, LV_EVENT_CLICKED, NULL);
    text(menu_scr, &a4_txt_22, C_DIM, "PODR\xC5\xBD: ZP\xC4\x9AT", 150);
    lv_obj_add_event_cb(menu_scr, go_gauge, LV_EVENT_LONG_PRESSED, NULL);
    lim_create();
    ord_create();
}

void a4_menu_open(void)
{
    load(menu_scr);
}
