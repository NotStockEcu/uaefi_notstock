/* Round gauge UI: DIAGNOSTICS, the OBD trouble codes. See ui_round.h.
 *
 * Opened from the menu; reads the codes at once (rnd_dtc_read: mode 03
 * stored and 07 pending, every ECU). One code at a time, big, with what it
 * means (dtc_text.c, shared with the 7" dash) and whether it is stored or
 * only pending; swipe for the next, the dots say which. READ reads again,
 * CLEAR wants a second tap within CLEAR_ARM_MS and then clears (mode 04)
 * and reads again. Long press: back to the menu.
 */
#include "ui_round_int.h"
#include "dtc_text.h"

#include <stdio.h>
#include <string.h>

#define CLEAR_ARM_MS 4000
#define DOTS_MAX     12           /* more codes: the dots give way to "3 / 15" */

#define C_AMBER      lv_color_hex(0xE0A020)
#define C_OK         lv_color_hex(0x3DDC84)

static lv_obj_t *scr, *state_lbl, *code_lbl, *kind_lbl, *text_lbl, *ok_ring;
static lv_obj_t *pos_lbl, *dot_row, *dot[DOTS_MAX];
static lv_obj_t *clear_btn, *clear_lbl, *read_btn;
static rnd_dtc_status_t shown;    /* what is on screen */
static int at;                    /* which code */
static uint32_t armed_at;
static int armed_shown = -1;
static bool fresh;                /* rebuild the text on the next update */

static bool armed(void)
{
    return armed_at && lv_tick_elaps(armed_at) < CLEAR_ARM_MS;
}

/* ------------------------------------------------------------- showing */
static const char *count_text(int n, char *buf, size_t sz)
{
    if (g_rnd_set.lang == RND_LANG_CS) {
        /* 1 CHYBA, 2-4 CHYBY, 5 CHYB */
        snprintf(buf, sz, "%d %s", n, n == 1 ? "CHYBA" :
                 n >= 2 && n <= 4 ? "CHYBY" : "CHYB");
    } else {
        snprintf(buf, sz, "%d CODE%s", n, n == 1 ? "" : "S");
    }
    return buf;
}

static void show_state(void)
{
    char buf[96], n[24];
    lv_color_t c = C_GREY;
    const rnd_dtc_status_t *s = &shown;
    if (s->busy == RND_DTC_READING) {
        snprintf(buf, sizeof buf, "%s", TR("READING ...", "ČTU ..."));
    } else if (s->busy == RND_DTC_CLEARING) {
        snprintf(buf, sizeof buf, "%s", TR("CLEARING ...", "MAŽU ..."));
    } else {
        switch (s->result) {
        case RND_DTC_NOT_READ:
            snprintf(buf, sizeof buf, "%s", TR("NOT READ", "NENAČTENO"));
            break;
        case RND_DTC_NO_ANSWER:
            snprintf(buf, sizeof buf, "%s", TR("NO ECU ANSWERED",
                                              "ŽÁDNÁ JEDNOTKA NEODPOVĚDĚLA"));
            c = C_RED;
            break;
        case RND_DTC_REFUSED:
            snprintf(buf, sizeof buf, "%s\n%s", TR("NOT CLEARED", "NESMAZÁNO"),
                     dtc_nrc_text(s->nrc, g_rnd_set.lang == RND_LANG_CS));
            c = C_RED;
            break;
        default:
            if (s->n == 0) {
                snprintf(buf, sizeof buf, "%s", s->result == RND_DTC_CLEARED ?
                         TR("CLEARED", "SMAZÁNO") : TR("NO CODES", "BEZ CHYB"));
                c = C_OK;
            } else {
                snprintf(buf, sizeof buf, "%s%s%s",
                         s->result == RND_DTC_CLEARED ?
                         TR("CLEARED, LEFT: ", "SMAZÁNO, ZŮSTALO: ") : "",
                         count_text(s->n, n, sizeof n), s->more ? " +" : "");
                c = C_AMBER;
            }
            break;
        }
    }
    lv_label_set_text(state_lbl, buf);
    lv_obj_set_style_text_color(state_lbl, c, 0);
}

static void show_code(void)
{
    const rnd_dtc_status_t *s = &shown;
    bool any = s->n > 0 && !s->busy;
    lv_obj_t *const parts[] = { code_lbl, kind_lbl, text_lbl };
    for (size_t i = 0; i < sizeof parts / sizeof parts[0]; i++) {
        if (any) lv_obj_clear_flag(parts[i], LV_OBJ_FLAG_HIDDEN);
        else     lv_obj_add_flag(parts[i], LV_OBJ_FLAG_HIDDEN);
    }
    /* nothing to page through: a big tick when the car is clean */
    bool clean = !s->busy && s->n == 0 &&
                 (s->result == RND_DTC_READ || s->result == RND_DTC_CLEARED);
    if (clean) lv_obj_clear_flag(ok_ring, LV_OBJ_FLAG_HIDDEN);
    else       lv_obj_add_flag(ok_ring, LV_OBJ_FLAG_HIDDEN);

    int n = any ? s->n : 0;
    for (int i = 0; i < DOTS_MAX; i++) {
        if (i >= n || n > DOTS_MAX || n < 2) {
            lv_obj_add_flag(dot[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(dot[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(dot[i], i == at ? 22 : 8);
        lv_obj_set_style_bg_color(dot[i], i == at ? C_W : C_DOT, 0);
    }
    char buf[32];
    if (n > DOTS_MAX) snprintf(buf, sizeof buf, "%d / %d", at + 1, n);
    else buf[0] = 0;
    lv_label_set_text(pos_lbl, buf);
    if (!any) return;

    const rnd_dtc_t *c = &s->list[at];
    bool cs = g_rnd_set.lang == RND_LANG_CS;
    static const char L[4] = { 'P', 'C', 'B', 'U' };
    snprintf(buf, sizeof buf, "%c%04X", L[c->code >> 14], c->code & 0x3FFF);
    lv_label_set_text(code_lbl, buf);
    bool stored = c->kind & RND_DTC_STORED;
    lv_obj_set_style_text_color(code_lbl, stored ? C_AMBER : C_GREY, 0);

    char k[48];
    snprintf(k, sizeof k, "%s   %03X",
             stored ? TR("STORED", "ULOŽENÁ") : TR("PENDING", "ČEKAJÍCÍ"),
             0x7E8 + c->ecu);
    lv_label_set_text(kind_lbl, k);
    const char *t = dtc_text(c->code, cs);
    lv_label_set_text(text_lbl, t ? t : dtc_group(c->code, cs));
}

static void show_buttons(void)
{
    int a = armed();
    if (a == armed_shown) return;
    armed_shown = a;
    lv_label_set_text(clear_lbl, a ? TR("SURE?", "OPRAVDU?")
                                   : TR("CLEAR", "SMAZAT"));
    lv_obj_set_style_bg_color(clear_btn, a ? C_RED : C_PANEL, 0);
    lv_obj_set_style_border_color(clear_btn, a ? C_RED : C_EDGE, 0);
}

/* ------------------------------------------------------------- actions */
static void read_cb(lv_event_t *e)
{
    (void)e;
    armed_at = 0;
    rnd_dtc_read();
}

static void clear_cb(lv_event_t *e)
{
    (void)e;
    if (armed()) {
        armed_at = 0;
        rnd_dtc_clear();
    } else {
        armed_at = lv_tick_get();
        if (!armed_at) armed_at = 1;
    }
    show_buttons();
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    armed_at = 0;
    rnd_menu_open();
}

static void gesture_cb(lv_event_t *e)
{
    (void)e;
    rnd_swiped();
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    int n = shown.busy ? 0 : shown.n;
    if (n < 2) return;
    if (dir == LV_DIR_LEFT)  at = (at + 1) % n;
    if (dir == LV_DIR_RIGHT) at = (at + n - 1) % n;
    show_code();
}

void rnd_diag_open(void)
{
    armed_at = 0;
    fresh = true;
    lv_scr_load(scr);
    rnd_dtc_read();                     /* always what the car says now */
}

void rnd_diag_update(const rnd_data_t *d)
{
    if (lv_scr_act() != scr) return;
    show_buttons();
    const rnd_dtc_status_t *s = &d->dtc;
    if (!fresh && s->busy == shown.busy && s->seq == shown.seq &&
        s->result == shown.result) {
        return;
    }
    fresh = false;
    shown = *s;
    if (at >= shown.n) at = 0;
    if (s->busy) at = 0;
    show_state();
    show_code();
}

/* ------------------------------------------------------------- building */
static lv_obj_t *pill(lv_coord_t x, const char *text, lv_event_cb_t cb,
                      lv_obj_t **lbl)
{
    lv_obj_t *b = lv_obj_create(scr);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 146, 54);
    lv_obj_set_pos(b, x, 344);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, C_PANEL, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, C_EDGE, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_bg_color(b, C_EDGE, LV_STATE_PRESSED);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &rnd_18, 0);
    lv_obj_set_style_text_color(l, C_W, 0);
    lv_obj_set_style_text_letter_space(l, 2, 0);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    if (lbl) *lbl = l;
    return b;
}

void rnd_diag_create(void)
{
    if (scr) lv_obj_del(scr);           /* rebuilt for a new language */
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    rnd_on_long(scr, back_cb);
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *t = rnd_label(scr, &rnd_18, C_GREY, 58);
    lv_obj_set_style_text_letter_space(t, 4, 0);
    lv_label_set_text(t, TR("DIAGNOSTICS", "DIAGNOSTIKA"));

    state_lbl = rnd_label(scr, &rnd_18, C_GREY, 92);
    lv_obj_set_width(state_lbl, 360);
    lv_obj_set_x(state_lbl, CX - 180);
    lv_label_set_long_mode(state_lbl, LV_LABEL_LONG_WRAP);

    code_lbl = rnd_label(scr, &rnd_56, C_AMBER, 150);
    kind_lbl = rnd_label(scr, &rnd_18, C_GREY, 216);
    lv_obj_set_style_text_letter_space(kind_lbl, 2, 0);
    text_lbl = rnd_label(scr, &rnd_18, C_W, 250);
    lv_obj_set_width(text_lbl, 340);
    lv_obj_set_x(text_lbl, CX - 170);
    lv_label_set_long_mode(text_lbl, LV_LABEL_LONG_WRAP);

    /* no codes: a green ring with a tick */
    ok_ring = lv_obj_create(scr);
    lv_obj_remove_style_all(ok_ring);
    lv_obj_set_size(ok_ring, 120, 120);
    lv_obj_align(ok_ring, LV_ALIGN_TOP_MID, 0, 156);
    lv_obj_set_style_radius(ok_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ok_ring, 6, 0);
    lv_obj_set_style_border_color(ok_ring, C_OK, 0);
    lv_obj_clear_flag(ok_ring, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    static const lv_point_t TICK[] = { { 32, 60 }, { 52, 80 }, { 88, 42 } };
    lv_obj_t *tick = lv_line_create(ok_ring);
    lv_line_set_points(tick, TICK, 3);
    lv_obj_set_pos(tick, -6, -6);             /* inside the border */
    lv_obj_set_style_line_width(tick, 10, 0);
    lv_obj_set_style_line_color(tick, C_OK, 0);
    lv_obj_set_style_line_rounded(tick, true, 0);
    lv_obj_add_flag(ok_ring, LV_OBJ_FLAG_HIDDEN);

    dot_row = lv_obj_create(scr);
    lv_obj_remove_style_all(dot_row);
    lv_obj_set_size(dot_row, 240, 10);
    lv_obj_align(dot_row, LV_ALIGN_TOP_MID, 0, 316);
    lv_obj_set_flex_flow(dot_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dot_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dot_row, 8, 0);
    lv_obj_clear_flag(dot_row, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < DOTS_MAX; i++) {
        dot[i] = lv_obj_create(dot_row);
        lv_obj_remove_style_all(dot[i]);
        lv_obj_set_size(dot[i], 8, 8);
        lv_obj_set_style_radius(dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(dot[i], LV_OBJ_FLAG_CLICKABLE);
    }
    pos_lbl = rnd_label(scr, &rnd_18, C_GREY, 306);

    read_btn = pill(CX - 152, TR("READ", "ČÍST"), read_cb, NULL);
    clear_btn = pill(CX + 6, "", clear_cb, &clear_lbl);

    t = rnd_label(scr, &rnd_18, C_DIM, 412);
    lv_label_set_text(t, TR("LONG PRESS: BACK", "PODRŽ: ZPĚT"));

    armed_shown = -1;
    fresh = true;
    show_buttons();
}
