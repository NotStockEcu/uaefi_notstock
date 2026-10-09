/* The A4 gauge's menu, see ui_a4.h: a long press on a dial opens it.
 *   NASTAVENI: LIMITY, PORADI, DPF (its page, which is not in the swipe),
 *   DIAGNOSTIKA, ZPET.
 *   DIAGNOSTIKA: ODCHYLKY (injection quantity deviation of cylinders 1..4,
 *   each a bore filling with it, the number under it, live) and CHYBY
 *   (the trouble codes: read, clear).
 *   LIMITY: one dial at a time (swipe for the next): its limit, - and +
 *   (hold to repeat). Red from there on the dial.
 *   PORADI: one dial at a time: its place in the swipe, < and > to move it
 *   earlier or later, ZOBRAZENO / SKRYTO to show or leave it out (the
 *   gauges only; DPF is not in the swipe).
 * A long press goes back (and stores the settings); in the menu, to the
 * dial. Black, white text, red where it is chosen, as the cluster. */
#include "ui_a4.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dtc_text.h"

LV_FONT_DECLARE(a4_txt_22);
LV_FONT_DECLARE(a4_txt_30);
LV_FONT_DECLARE(a4_big_96);
LV_FONT_DECLARE(a4_num_40);
LV_FONT_DECLARE(a4_code_76);

#define C         (A4_SIZE / 2)
#define C_W       lv_color_hex(0xEEEEEC)
#define C_SMALL   lv_color_hex(0xA0A0A0)
#define C_EDGE    lv_color_hex(0x3A3A3E)
#define C_RED     lv_color_hex(0xF02A26)
#define C_DIM     lv_color_hex(0x5A5A5E)
#define C_DOT     lv_color_hex(0x3A3A3E)
#define C_AMBER   lv_color_hex(0xFFA800)
#define C_OK      lv_color_hex(0x3DDC84)
#define DOTS_DY   152                /* the dots under the editors */

static lv_obj_t *menu_scr, *lim_scr, *ord_scr, *dg_scr, *inj_scr, *dtc_scr;

/* a screen change goes out whole, as a page change */
static void load(lv_obj_t *scr)
{
    a4_flip();
    lv_scr_load(scr);
}

/* ------------------------------------------------------------- widgets */
/* A menu screen: black with the dials' red lit rim. The rim is the DPF
 * page's face, an image: a lit rim drawn with LVGL's shadow took a third
 * of a second on the ESP32 for every screen. */
LV_IMG_DECLARE(a4_face_dpf);

static lv_obj_t *screen(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s, lv_color_black(), 0);
    lv_obj_t *rim = lv_img_create(s);
    lv_img_set_src(rim, &a4_face_dpf);
    lv_obj_set_pos(rim, 0, 0);
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

static void dots_make(lv_obj_t *par, lv_obj_t **dots, int n, lv_coord_t dy)
{
    for (int i = 0; i < n; i++) {
        lv_obj_t *o = lv_obj_create(par);
        lv_obj_remove_style_all(o);
        lv_obj_set_size(o, 8, 8);
        lv_obj_set_style_radius(o, 4, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(o, C_DOT, 0);
        lv_obj_align(o, LV_ALIGN_CENTER, -(n - 1) * 7 + i * 14, dy);
        lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
        dots[i] = o;
    }
}

static void dots_set(lv_obj_t **dots, int n, int at)
{
    for (int i = 0; i < n; i++) {
        lv_obj_set_width(dots[i], i == at ? 20 : 8);
        lv_obj_align(dots[i], LV_ALIGN_CENTER, -(n - 1) * 7 + i * 14, DOTS_DY);
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

/* the settings as they were when the menu opened: stored (and the dial
 * redrawn) only when something changed */
static a4_settings_t s_at_open;

static void back_to_menu(lv_event_t *e)
{
    (void)e;
    if (memcmp(&s_at_open, &g_a4_set, sizeof g_a4_set)) {
        a4_settings_save();
        a4_settings_changed();
        s_at_open = g_a4_set;
    }
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
    dots_set(lim_dots, A4_PAGES, lim_at);
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
    dots_make(lim_scr, lim_dots, A4_PAGES, DOTS_DY);
    text(lim_scr, &a4_txt_22, C_DIM, "PODR\xC5\xBD: ZP\xC4\x9AT", 186);
    lv_obj_add_event_cb(lim_scr, lim_gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(lim_scr, back_to_menu, LV_EVENT_LONG_PRESSED, NULL);
}

/* ---------------------------------------------------------------- order */
/* the order of the swipe's dials: the first A4_SWIPE places (DPF is kept
 * last, out of it) */
static int ord_at;                    /* position in g_a4_set.order */
static lv_obj_t *ord_name, *ord_pos, *ord_state, *ord_state_lbl;
static lv_obj_t *ord_dots[A4_SWIPE];

static int shown_count(void)
{
    int n = 0;
    for (int i = 0; i < A4_SWIPE; i++) n += !(g_a4_set.hidden >> i & 1);
    return n;
}

static void ord_show(void)
{
    char buf[32];
    int p = g_a4_set.order[ord_at];
    bool on = !(g_a4_set.hidden >> p & 1);
    lv_label_set_text(ord_name, A4_LIMIT[p].name);
    lv_obj_set_style_text_color(ord_name, on ? C_W : C_DIM, 0);
    snprintf(buf, sizeof buf, "POZICE %d / %d", ord_at + 1, A4_SWIPE);
    lv_label_set_text(ord_pos, buf);
    lv_label_set_text(ord_state_lbl, on ? "ZOBRAZENO" : "SKRYTO");
    lv_obj_set_style_text_color(ord_state_lbl, on ? C_W : C_DIM, 0);
    lv_obj_set_style_border_color(ord_state, on ? C_RED : C_EDGE, 0);
    dots_set(ord_dots, A4_SWIPE, ord_at);
    for (int i = 0; i < A4_SWIPE; i++) {
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
    if (to < 0 || to >= A4_SWIPE) return;
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
    ord_at = (ord_at + d + A4_SWIPE) % A4_SWIPE;
    ord_show();
}

static void go_order(lv_event_t *e)
{
    (void)e;
    ord_at = 0;
    for (int i = 0; i < A4_SWIPE; i++) {
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
    dots_make(ord_scr, ord_dots, A4_SWIPE, DOTS_DY);
    text(ord_scr, &a4_txt_22, C_DIM, "PODR\xC5\xBD: ZP\xC4\x9AT", 186);
    lv_obj_add_event_cb(ord_scr, ord_gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(ord_scr, back_to_menu, LV_EVENT_LONG_PRESSED, NULL);
}

/* DPF: its page (not in the swipe); a swipe there goes back to the dials */
static void go_dpf(lv_event_t *e)
{
    (void)e;
    load(a4_gauge_screen());
    ui_a4_page(A4_DPF);
}

/* ----------------------------------------------------------- diagnosis */
#define INJ_SPEC  2.8f            /* mg/stroke: VW's usual idle limit, rough */
#define INJ_SCALE 4.0f            /* empty -4 .. half full 0 .. full +4 mg */
#define CYL_W     62
#define CYL_H     220
#define CYL_Y     (-6)            /* the cylinders' centre, from the screen's */
#define CYL_DX    86
static lv_obj_t *inj_fill[4], *inj_val[4];

static void back_to_diag(lv_event_t *e)
{
    (void)e;
    load(dg_scr);
}

static void go_inj(lv_event_t *e)
{
    (void)e;
    load(inj_scr);
}

static lv_obj_t *box(lv_obj_t *par, lv_coord_t w, lv_coord_t h, lv_coord_t dx,
                     lv_coord_t dy, lv_color_t c)
{
    lv_obj_t *o = lv_obj_create(par);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_align(o, LV_ALIGN_CENTER, dx, dy);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

/* Each cylinder a bore that fills with its deviation: half full is 0, up
 * to the top +INJ_SCALE (more fuel than the others), down to empty minus
 * it; the white line is 0, the red ones the spec. The number under it. */
static void inj_create(void)
{
    inj_scr = screen();
    text(inj_scr, &a4_txt_22, C_SMALL, "VST\xC5\x98IK  mg/zdvih", -176);
    int spec = (int)lroundf(INJ_SPEC / INJ_SCALE * (CYL_H / 2));
    for (int i = 0; i < 4; i++) {
        lv_coord_t x = -(3 * CYL_DX) / 2 + i * CYL_DX;
        /* the bore: a dark tube with a light wall, clipping its fill */
        lv_obj_t *bore = box(inj_scr, CYL_W, CYL_H, x, CYL_Y, lv_color_hex(0x101012));
        lv_obj_set_style_radius(bore, 10, 0);
        lv_obj_set_style_clip_corner(bore, true, 0);
        lv_obj_set_style_border_width(bore, 2, 0);
        lv_obj_set_style_border_color(bore, lv_color_hex(0x8A8A8E), 0);
        lv_obj_set_style_border_post(bore, true, 0);
        inj_fill[i] = lv_obj_create(bore);
        lv_obj_remove_style_all(inj_fill[i]);
        lv_obj_set_width(inj_fill[i], CYL_W);
        lv_obj_set_height(inj_fill[i], 0);
        lv_obj_align(inj_fill[i], LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(inj_fill[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_grad_dir(inj_fill[i], LV_GRAD_DIR_HOR, 0);
        lv_obj_clear_flag(inj_fill[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *z = box(bore, CYL_W, 2, 0, 0, C_W);          /* 0 */
        lv_obj_set_style_bg_opa(z, LV_OPA_70, 0);
        box(bore, CYL_W, 1, 0, -spec, C_RED);                  /* + spec */
        box(bore, CYL_W, 1, 0, spec, C_RED);                   /* - spec */
        char n[4];
        snprintf(n, sizeof n, "%d", i + 1);
        lv_obj_t *l = text(inj_scr, &a4_txt_22, C_SMALL, "", 0);
        lv_label_set_text(l, n);
        lv_obj_align(l, LV_ALIGN_CENTER, x, CYL_Y - CYL_H / 2 - 14);
        inj_val[i] = text(inj_scr, &a4_txt_22, C_W, "--", 0);
        lv_obj_set_width(inj_val[i], CYL_DX);
        lv_obj_set_style_text_align(inj_val[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(inj_val[i], LV_ALIGN_CENTER, x, CYL_Y + CYL_H / 2 + 18);
    }
    text(inj_scr, &a4_txt_22, C_DIM, "PODR\xC5\xBD: ZP\xC4\x9AT", 192);
    lv_obj_add_event_cb(inj_scr, back_to_diag, LV_EVENT_LONG_PRESSED, NULL);
}

static void inj_update(const rnd_data_t *d)
{
    char buf[16];
    for (int i = 0; i < 4; i++) {
        float x = d->link ? d->diag.inj_mg[i] : NAN;
        if (isnan(x)) {
            lv_obj_set_height(inj_fill[i], 0);
            lv_label_set_text(inj_val[i], "--");
            lv_obj_set_style_text_color(inj_val[i], C_SMALL, 0);
            continue;
        }
        float a = fabsf(x);
        lv_color_t c = a >= INJ_SPEC ? C_RED : a >= INJ_SPEC / 2 ? C_AMBER : C_OK;
        float k = (x + INJ_SCALE) / (2 * INJ_SCALE);           /* 0 .. 1 */
        if (k < 0) k = 0;
        if (k > 1) k = 1;
        lv_obj_set_height(inj_fill[i], (lv_coord_t)lroundf(k * CYL_H));
        lv_obj_align(inj_fill[i], LV_ALIGN_BOTTOM_MID, 0, 0);
        /* a little light on the left: a round bore */
        lv_obj_set_style_bg_color(inj_fill[i], lv_color_lighten(c, LV_OPA_40), 0);
        lv_obj_set_style_bg_grad_color(inj_fill[i], lv_color_darken(c, LV_OPA_40), 0);
        snprintf(buf, sizeof buf, "%+.2f", x);
        lv_label_set_text(inj_val[i], buf);
        lv_obj_set_style_text_color(inj_val[i], a >= INJ_SPEC / 2 ? c : C_W, 0);
    }
}

/* the trouble codes: the state, one code at a time (swipe), READ, CLEAR
 * (tap twice: the second within CLEAR_ARM_MS) */
#define CLEAR_ARM_MS 3000
static lv_obj_t *dtc_state, *dtc_code, *dtc_kind, *dtc_text_lbl, *dtc_pos;
static lv_obj_t *dtc_clear_lbl, *dtc_clear;
static int dtc_at;
static uint32_t dtc_armed_at;
static rnd_dtc_status_t dtc_shown;

static bool dtc_armed(void)
{
    return dtc_armed_at && lv_tick_elaps(dtc_armed_at) < CLEAR_ARM_MS;
}

static void dtc_show(void)
{
    const rnd_dtc_status_t *s = &dtc_shown;
    char buf[96];
    lv_color_t c = C_SMALL;
    if (s->busy == RND_DTC_READING) {
        snprintf(buf, sizeof buf, "\xC4\x8CTU ...");
    } else if (s->busy == RND_DTC_CLEARING) {
        snprintf(buf, sizeof buf, "MA\xC5\xBDU ...");
    } else switch (s->result) {
    case RND_DTC_NOT_READ:
        snprintf(buf, sizeof buf, "NENA\xC4\x8CTENO");
        break;
    case RND_DTC_NO_ANSWER:
        snprintf(buf, sizeof buf, "\xC5\xBD\xC3\x81" "DN\xC3\x81 ODPOV\xC4\x9A\xC4\x8E");
        c = C_RED;
        break;
    case RND_DTC_REFUSED:
        snprintf(buf, sizeof buf, "NESMAZ\xC3\x81NO: %s", dtc_nrc_text(s->nrc, 1));
        c = C_RED;
        break;
    default:
        if (s->n == 0) {
            snprintf(buf, sizeof buf, "%s", s->result == RND_DTC_CLEARED ?
                     "SMAZ\xC3\x81NO" : "BEZ CHYB");
            c = C_OK;
        } else {
            int n = s->n;
            snprintf(buf, sizeof buf, "%s%d %s%s",
                     s->result == RND_DTC_CLEARED ? "ZB\xC3\x9DV\xC3\x81: " : "",
                     n, n == 1 ? "CHYBA" : n <= 4 ? "CHYBY" : "CHYB", s->more ? " +" : "");
            c = C_AMBER;
        }
    }
    lv_label_set_text(dtc_state, buf);
    lv_obj_set_style_text_color(dtc_state, c, 0);

    bool any = s->n > 0 && !s->busy;
    lv_obj_t *parts[] = { dtc_code, dtc_kind, dtc_text_lbl, dtc_pos };
    for (unsigned i = 0; i < 4; i++) {
        if (any) lv_obj_clear_flag(parts[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(parts[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (any) {
        if (dtc_at >= s->n) dtc_at = 0;
        const rnd_dtc_t *k = &s->list[dtc_at];
        static const char L[4] = { 'P', 'C', 'B', 'U' };
        snprintf(buf, sizeof buf, "%c%04X", L[k->code >> 14], k->code & 0x3FFF);
        lv_label_set_text(dtc_code, buf);
        bool stored = k->kind & RND_DTC_STORED;
        lv_obj_set_style_text_color(dtc_code, stored ? C_RED : C_AMBER, 0);
        snprintf(buf, sizeof buf, "%s   %03X", stored ? "ULO\xC5\xBD" "EN\xC3\x81" :
                 "\xC4\x8C" "EKAJ\xC3\x8D" "C\xC3\x8D", 0x7E8 + k->ecu);
        lv_label_set_text(dtc_kind, buf);
        const char *t = dtc_text(k->code, 1);
        lv_label_set_text(dtc_text_lbl, t ? t : dtc_group(k->code, 1));
        snprintf(buf, sizeof buf, "%d / %d", dtc_at + 1, s->n);
        lv_label_set_text(dtc_pos, buf);
    }
    bool a = dtc_armed();
    lv_label_set_text(dtc_clear_lbl, a ? "OPRAVDU?" : "SMAZAT");
    lv_obj_set_style_border_color(dtc_clear, a ? C_RED : C_EDGE, 0);
    lv_obj_set_style_text_color(dtc_clear_lbl, a ? C_RED : C_W, 0);
}

static void dtc_read_cb(lv_event_t *e)
{
    (void)e;
    dtc_armed_at = 0;
    rnd_dtc_read();
}

static void dtc_clear_cb(lv_event_t *e)
{
    (void)e;
    if (dtc_armed()) {
        dtc_armed_at = 0;
        rnd_dtc_clear();
    } else {
        dtc_armed_at = lv_tick_get();
        if (!dtc_armed_at) dtc_armed_at = 1;
    }
    dtc_show();
}

static void dtc_gesture(lv_event_t *e)
{
    (void)e;
    int d = swipe_dir();
    if (!d || dtc_shown.n < 2) return;
    dtc_at = (dtc_at + d + dtc_shown.n) % dtc_shown.n;
    dtc_show();
}

static void go_dtc(lv_event_t *e)
{
    (void)e;
    dtc_at = 0;
    dtc_armed_at = 0;
    /* first time here: read at once */
    if (dtc_shown.result == RND_DTC_NOT_READ && !dtc_shown.busy) rnd_dtc_read();
    dtc_show();
    load(dtc_scr);
}

static void dtc_create(void)
{
    dtc_scr = screen();
    text(dtc_scr, &a4_txt_22, C_SMALL, "CHYBY", -184);
    dtc_state = text(dtc_scr, &a4_txt_22, C_SMALL, "", -150);
    lv_obj_set_width(dtc_state, 300);
    lv_obj_set_style_text_align(dtc_state, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(dtc_state, LV_LABEL_LONG_WRAP);
    lv_obj_align(dtc_state, LV_ALIGN_CENTER, 0, -150);
    dtc_code = text(dtc_scr, &a4_code_76, C_RED, "", -86);
    dtc_kind = text(dtc_scr, &a4_txt_22, C_SMALL, "", -28);
    dtc_text_lbl = text(dtc_scr, &a4_txt_22, C_W, "", 22);
    lv_obj_set_width(dtc_text_lbl, 330);
    lv_obj_set_style_text_align(dtc_text_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(dtc_text_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_align(dtc_text_lbl, LV_ALIGN_CENTER, 0, 22);
    dtc_pos = text(dtc_scr, &a4_txt_22, C_SMALL, "", 66);
    lv_obj_t *r = pill(dtc_scr, "\xC4\x8C\xC3\x8DST", 140, 60, -76, 112, NULL);
    lv_obj_add_event_cb(r, dtc_read_cb, LV_EVENT_CLICKED, NULL);
    dtc_clear = pill(dtc_scr, "SMAZAT", 140, 60, 76, 112, &dtc_clear_lbl);
    lv_obj_add_event_cb(dtc_clear, dtc_clear_cb, LV_EVENT_CLICKED, NULL);
    text(dtc_scr, &a4_txt_22, C_DIM, "PODR\xC5\xBD: ZP\xC4\x9AT", 186);
    lv_obj_add_event_cb(dtc_scr, dtc_gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(dtc_scr, back_to_diag, LV_EVENT_LONG_PRESSED, NULL);
}

static void go_diag(lv_event_t *e)
{
    (void)e;
    load(dg_scr);
}

static void diag_create(void)
{
    dg_scr = screen();
    text(dg_scr, &a4_txt_22, C_SMALL, "DIAGNOSTIKA", -150);
    lv_obj_t *b = pill(dg_scr, "ODCHYLKY", 260, 62, 0, -78, NULL);
    lv_obj_add_event_cb(b, go_inj, LV_EVENT_CLICKED, NULL);
    b = pill(dg_scr, "CHYBY", 260, 62, 0, -4, NULL);
    lv_obj_add_event_cb(b, go_dtc, LV_EVENT_CLICKED, NULL);
    b = pill(dg_scr, "ZP\xC4\x9AT", 260, 62, 0, 70, NULL);
    lv_obj_add_event_cb(b, back_to_menu, LV_EVENT_CLICKED, NULL);
    text(dg_scr, &a4_txt_22, C_DIM, "PODR\xC5\xBD: ZP\xC4\x9AT", 150);
    lv_obj_add_event_cb(dg_scr, back_to_menu, LV_EVENT_LONG_PRESSED, NULL);
    inj_create();
    dtc_create();
}

/* live data for the screens that show it, from ui_a4_update */
void a4_menu_update(const rnd_data_t *d)
{
    lv_obj_t *a = lv_scr_act();
    if (a == inj_scr) inj_update(d);
    if (a == dtc_scr) {
        /* redraw when the state or the list changed */
        if (d->dtc.busy != dtc_shown.busy || d->dtc.result != dtc_shown.result ||
            d->dtc.seq != dtc_shown.seq || d->dtc.n != dtc_shown.n) {
            dtc_shown = d->dtc;
            dtc_show();
        }
        if (dtc_armed_at && !dtc_armed()) {
            dtc_armed_at = 0;
            dtc_show();
        }
    } else {
        dtc_shown = d->dtc;
    }
}

/* ----------------------------------------------------------------- menu */
void a4_menu_create(void)
{
    menu_scr = screen();
    text(menu_scr, &a4_txt_22, C_SMALL, "NASTAVEN\xC3\x8D", -182);
    static const struct { const char *t; lv_event_cb_t cb; } ITEM[] = {
        { "LIMITY", go_limits },
        { "PO\xC5\x98" "AD\xC3\x8D", go_order },
        { "DPF", go_dpf },
        { "DIAGNOSTIKA", go_diag },
        { "ZP\xC4\x9AT", go_gauge },
    };
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = pill(menu_scr, ITEM[i].t, 260, 56, 0, -128 + i * 64, NULL);
        lv_obj_add_event_cb(b, ITEM[i].cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_add_event_cb(menu_scr, go_gauge, LV_EVENT_LONG_PRESSED, NULL);
    lim_create();
    ord_create();
    diag_create();
}

void a4_menu_open(void)
{
    s_at_open = g_a4_set;
    load(menu_scr);
}
