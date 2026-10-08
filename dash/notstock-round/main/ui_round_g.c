/* Round gauge UI: the G-METER (menu -> G-METER), in the look picked in
 * SETTINGS, rebuilt when it changes:
 *   NOTSTOCK  grey rings on black, a white dot with a glow, red over 1 g
 *   RETRO     the VDO dial (face_retro_plain), printed rings and scale, a
 *             red ball with a shadow, readings in Barlow
 *   FUTURO    the hex background, cyan rings, a neon dot, magenta over 1 g
 *
 * The dot is the force the driver feels: braking moves it up, speeding up
 * down, a right bend to the left (as a ball on the dash would roll). The
 * highest of each direction at the edges, the total under the rings. Tap:
 * those back to zero. Double tap with the car standing: zero the sensor
 * (which way is up, for any mounting angle). Forward is learnt from the
 * speed while driving (gmeter.h). Long press: the menu.
 */
#include "ui_round_int.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CY        (RND_H / 2)
#define RING_R    (RND_W * 170 / 480)
#define G_FULL    1.2f            /* g at the outer ring */
#define G_HOT     1.0f            /* the dot turns over this */

enum { GS_NOTSTOCK, GS_RETRO, GS_FUTURO };

static lv_obj_t *scr, *dot, *pk_l[4], *now_l, *msg_l, *toast;
static int style;
static float pk[4];               /* brake, accel, left, right */
static float gx_s, gy_s;
static uint32_t last_tap, last_swipe;

/* colours and fonts of the look */
static lv_color_t c_ring, c_ring1, c_cross, c_text, c_dim, c_dot, c_hot;
static const lv_font_t *f_val, *f_small;

static void look_setup(void)
{
    style = g_rnd_set.look == RND_LOOK_RETRO ? GS_RETRO :
            g_rnd_set.look == RND_LOOK_FUTURO ? GS_FUTURO : GS_NOTSTOCK;
    switch (style) {
    case GS_RETRO:
        c_ring = C_INK_DIM; c_ring1 = C_W; c_cross = C_INK_DIM;
        c_text = C_W; c_dim = C_INK_DIM; c_dot = C_NEEDLE; c_hot = C_NEEDLE;
        f_val = &rnd_barlow_23; f_small = &rnd_barlow_20;
        break;
    case GS_FUTURO:
        c_ring = C_TEAL_DIM; c_ring1 = C_CYAN; c_cross = C_SEG_OFF;
        c_text = C_ICE; c_dim = C_TEAL_DIM; c_dot = C_CYAN; c_hot = C_MAGENTA;
        f_val = &rnd_26; f_small = &rnd_18;
        break;
    default:
        c_ring = C_DOT; c_ring1 = C_GREY; c_cross = C_DOT;
        c_text = C_W; c_dim = C_GREY; c_dot = C_W; c_hot = C_RED;
        f_val = &rnd_26; f_small = &rnd_18;
        break;
    }
}

/* --------------------------------------------------------------- helpers */
static lv_obj_t *label(lv_obj_t *par, const lv_font_t *f, lv_color_t c)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_label_set_text(l, "");
    return l;
}

static void at_xy(lv_obj_t *o, lv_coord_t x, lv_coord_t y)
{
    lv_obj_update_layout(o);
    lv_obj_set_pos(o, x - lv_obj_get_width(o) / 2, y - lv_obj_get_height(o) / 2);
}

static lv_obj_t *ring(int r, lv_color_t c, int w)
{
    lv_obj_t *o = lv_obj_create(scr);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, 2 * r, 2 * r);
    lv_obj_set_pos(o, CX - r, CY - r);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(o, c, 0);
    lv_obj_set_style_border_width(o, w, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void line(lv_point_t *p, lv_color_t c, int w)
{
    lv_obj_t *l = lv_line_create(scr);
    lv_line_set_points(l, p, 2);
    lv_obj_set_style_line_color(l, c, 0);
    lv_obj_set_style_line_width(l, w, 0);
}

/* a reading over the rings and lines: on a dark plate (RETRO: a window
 * in the dial, with a thin frame) */
static lv_obj_t *window(lv_obj_t *l)
{
    lv_obj_set_style_bg_color(l, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(l, style == GS_RETRO ? LV_OPA_COVER : LV_OPA_70, 0);
    lv_obj_set_style_pad_hor(l, 8, 0);
    lv_obj_set_style_pad_ver(l, 3, 0);
    lv_obj_set_style_radius(l, style == GS_RETRO ? 3 : 8, 0);
    if (style == GS_RETRO) {
        lv_obj_set_style_border_color(l, C_INK_DIM, 0);
        lv_obj_set_style_border_width(l, 1, 0);
    }
    return l;
}

static void show_toast(const char *t)
{
    lv_label_set_text(lv_obj_get_child(toast, 0), t);
    lv_obj_set_style_opa(toast, LV_OPA_COVER, 0);
    lv_obj_fade_out(toast, 400, 1200);
}

/* ---------------------------------------------------------------- input */
static void gesture_cb(lv_event_t *e)
{
    (void)e;
    last_swipe = lv_tick_get();
    last_tap = 0;
}

static void tap_cb(lv_event_t *e)
{
    (void)e;
    if (last_swipe && lv_tick_elaps(last_swipe) < 400) return;
    if (last_tap && lv_tick_elaps(last_tap) < 400) {
        last_tap = 0;
        rnd_g_zero();
        show_toast(TR("ZEROED", "NULOVÁNO"));
        return;
    }
    last_tap = lv_tick_get();
    for (int i = 0; i < 4; i++) pk[i] = 0;
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    rnd_menu_open();
}

/* ---------------------------------------------------------------- build */
static void build_face(void)
{
    int r1 = (int)(RING_R * 1.0f / G_FULL), r05 = (int)(RING_R * 0.5f / G_FULL);
    if (style == GS_RETRO) {
        lv_obj_t *bg = lv_img_create(scr);
        lv_img_set_src(bg, &face_retro_plain);
        lv_obj_center(bg);
    } else if (style == GS_FUTURO) {
        lv_obj_t *bg = lv_img_create(scr);
        lv_img_set_src(bg, &face_futuro_bg);
        lv_obj_center(bg);
    }

    /* crosshair, the rings, and on RETRO a printed scale */
    static lv_point_t h[2], v[2];
    h[0] = (lv_point_t){ CX - RING_R, CY };
    h[1] = (lv_point_t){ CX + RING_R, CY };
    v[0] = (lv_point_t){ CX, CY - RING_R };
    v[1] = (lv_point_t){ CX, CY + RING_R };
    line(h, c_cross, 2);
    line(v, c_cross, 2);
    if (style != GS_RETRO) ring(RING_R, style == GS_FUTURO ? C_TEAL_DIM : C_EDGE, 2);
    ring(r05, c_ring, 2);
    ring(r1, c_ring1, style == GS_RETRO ? 3 : 2);
    if (style == GS_RETRO) {
        /* ticks every 0.1 g along the axes, as printed on a dial */
        static lv_point_t t[4 * 12][2];
        int n = 0;
        for (int k = 1; k <= 12; k++) {
            int r = (int)(RING_R * (k / 10.0f) / G_FULL);
            int len = k % 5 == 0 ? 10 : 5;
            for (int q = 0; q < 4; q++, n++) {
                int sx = q == 0 ? 1 : q == 1 ? -1 : 0;
                int sy = q == 2 ? 1 : q == 3 ? -1 : 0;
                t[n][0] = (lv_point_t){ CX + sx * r - sy * len, CY + sy * r - sx * len };
                t[n][1] = (lv_point_t){ CX + sx * r + sy * len, CY + sy * r + sx * len };
                line(t[n], C_W, k % 5 == 0 ? 3 : 2);
            }
        }
    }
    lv_obj_t *s = label(scr, f_small, c_dim);
    lv_label_set_text(s, "1 G");
    at_xy(s, CX + r1 * 0.72f + 22, CY - r1 * 0.72f - 4);
    s = label(scr, f_small, c_dim);
    lv_label_set_text(s, "0.5");
    at_xy(s, CX + r05 * 0.72f + 16, CY - r05 * 0.72f - 2);

    lv_obj_t *t = label(scr, f_small, c_dim);
    lv_obj_set_style_text_letter_space(t, style == GS_RETRO ? 2 : 4, 0);
    lv_label_set_text(t, TR("G-METER", "G-METR"));
    at_xy(t, CX, 32);
}

void rnd_g_create(void)
{
    if (scr) lv_obj_del(scr);           /* a new look or language */
    look_setup();
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(scr, tap_cb, LV_EVENT_SHORT_CLICKED, NULL);
    rnd_on_long(scr, back_cb);

    build_face();
    for (int i = 0; i < 4; i++) pk_l[i] = window(label(scr, f_small, c_text));
    now_l = window(label(scr, f_val, c_text));
    msg_l = window(label(scr, f_small, style == GS_FUTURO ? C_MAGENTA : C_REGEN));

    dot = lv_obj_create(scr);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 26, 26);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    if (style == GS_RETRO) {
        /* a ball on the dial: a dark shadow under it, a highlight */
        lv_obj_set_style_shadow_color(dot, lv_color_black(), 0);
        lv_obj_set_style_shadow_width(dot, 12, 0);
        lv_obj_set_style_shadow_ofs_x(dot, 3, 0);
        lv_obj_set_style_shadow_ofs_y(dot, 4, 0);
        lv_obj_set_style_border_color(dot, lv_color_hex(0x5A0A06), 0);
        lv_obj_set_style_border_width(dot, 2, 0);
        lv_obj_set_style_bg_grad_color(dot, lv_color_hex(0x8A120C), 0);
        lv_obj_set_style_bg_grad_dir(dot, LV_GRAD_DIR_VER, 0);
    } else {
        lv_obj_set_style_shadow_width(dot, 24, 0);
        lv_obj_set_style_shadow_spread(dot, 2, 0);
    }
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    gx_s = gy_s = 0;

    toast = lv_obj_create(scr);
    lv_obj_remove_style_all(toast);
    lv_obj_set_size(toast, 220, 50);
    lv_obj_align(toast, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(toast, 25, 0);
    lv_obj_set_style_bg_color(toast, C_PANEL, 0);
    lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(toast, C_EDGE, 0);
    lv_obj_set_style_border_width(toast, 2, 0);
    lv_obj_t *tl = label(toast, &rnd_26, C_W);
    lv_obj_center(tl);
    lv_obj_set_style_opa(toast, LV_OPA_TRANSP, 0);
}

lv_obj_t *rnd_g_screen(void)
{
    return scr;
}

void rnd_g_open(void)
{
    lv_scr_load(scr);
}

/* --------------------------------------------------------------- update */
void rnd_g_update(const rnd_data_t *d)
{
    if (!scr || lv_scr_act() != scr) return;
    char b[40];
    if (!d->g.present) {
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(now_l, TR("NO SENSOR", "BEZ SNÍMAČE"));
        at_xy(now_l, CX, CY + RING_R / 2);
        return;
    }
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
    float lon = d->g.lon_g, lat = d->g.lat_g;
    gx_s += (lat - gx_s) * 0.3f;
    gy_s += (lon - gy_s) * 0.3f;
    /* the felt force: braking up, speeding up down, right bend left */
    float px = -gx_s, py = gy_s;
    float r = sqrtf(px * px + py * py);
    if (r > G_FULL) {
        px *= G_FULL / r;
        py *= G_FULL / r;
    }
    float k = RING_R / G_FULL;
    lv_obj_set_pos(dot, (lv_coord_t)(CX + px * k - 13),
                   (lv_coord_t)(CY + py * k - 13));
    lv_color_t c = r >= G_HOT ? c_hot : c_dot;
    lv_obj_set_style_bg_color(dot, c, 0);
    if (style != GS_RETRO) lv_obj_set_style_shadow_color(dot, c, 0);

    if (-lon > pk[0]) pk[0] = -lon;
    if (lon > pk[1]) pk[1] = lon;
    if (-lat > pk[2]) pk[2] = -lat;
    if (lat > pk[3]) pk[3] = lat;
    static const char *const EN[4] = { "BRAKE", "ACCEL", "LEFT", "RIGHT" };
    static const char *const CS[4] = { "BRZDA", "ZRYCHL", "VLEVO", "VPRAVO" };
    for (int i = 0; i < 4; i++) {
        snprintf(b, sizeof b, "%s %.2f",
                 g_rnd_set.lang == RND_LANG_CS ? CS[i] : EN[i], pk[i]);
        lv_label_set_text(pk_l[i], b);
    }
    /* braking pushes the dot up: its highest at the top; a right bend
     * pushes it left: RIGHT on the left */
    at_xy(pk_l[0], CX, CY - RING_R + 30);
    at_xy(pk_l[1], CX, CY + RING_R - 30);
    at_xy(pk_l[3], CX - RING_R + 62, CY - 18);
    at_xy(pk_l[2], CX + RING_R - 62, CY - 18);

    snprintf(b, sizeof b, "%.2f G", sqrtf(lon * lon + lat * lat));
    lv_label_set_text(now_l, b);
    at_xy(now_l, CX, CY + RING_R - 64);
    lv_label_set_text(msg_l, !d->g.zeroed
        ? TR("STANDING: DOUBLE TAP = ZERO", "STOJÍŠ: 2x ŤUK = NULA") : "");
    at_xy(msg_l, CX, CY - RING_R / 2 - 6);
}
