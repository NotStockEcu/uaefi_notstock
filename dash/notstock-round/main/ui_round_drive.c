/* Round gauge UI: DRIVE, two pages, swipe between them; long press: menu.
 *
 * COMPASS: the GPS course over ground on a rose that turns, heading up,
 * the course in degrees and as a direction, speed and satellites. The GPS
 * only knows a course while the car moves (from about 5 km/h); standing,
 * the last one stays, dimmed.
 *
 * G-METER: the force the driver feels, as a dot in rings of 0.5 and 1 g:
 * braking moves it up, speeding up down, a right bend to the left (the
 * way a ball on the dash would roll). The highest of each direction at the
 * edges. Tap: those back to zero. Double tap with the car standing: zero
 * the sensor (which way is up); forward is learnt from the GPS while
 * driving. See gmeter.h.
 */
#include "ui_round_int.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CY        (RND_H / 2)
#define ROSE_R    (RND_W * 170 / 480)
#define TICKS     36
#define G_FULL    1.2f            /* g at the ring's edge */
#define MOVING_KMH 5.0f
#define C_ORANGE  lv_color_hex(0xFF8A00)

enum { PG_COMPASS, PG_G, PG_N };

static lv_obj_t *scr, *pg[PG_N], *dots[PG_N], *toast;
static int at;
static uint32_t last_swipe, last_tap;

/* compass */
static lv_obj_t *tick[TICKS], *card[4], *deg, *deg_sign, *dir, *info, *cmsg;
static lv_point_t tick_pt[TICKS][2];
static float shown_hdg = NAN, drawn_hdg = NAN, last_hdg = NAN;

/* G-meter */
static lv_obj_t *gdot, *gpk[4], *gnow, *gmsg;
static float pk[4];               /* brake, accel, left, right */
static float gx_s, gy_s;

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

static lv_obj_t *circle(lv_obj_t *par, int r, lv_color_t c, int w)
{
    lv_obj_t *o = lv_obj_create(par);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, 2 * r, 2 * r);
    lv_obj_set_pos(o, CX - r, CY - r);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(o, c, 0);
    lv_obj_set_style_border_width(o, w, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *page_obj(void)
{
    lv_obj_t *o = lv_obj_create(scr);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, RND_W, RND_H);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void title(lv_obj_t *par, const char *t)
{
    lv_obj_t *l = rnd_label(par, &rnd_18, C_GREY, 22);
    lv_obj_set_style_text_letter_space(l, 4, 0);
    lv_label_set_text(l, t);
}

static void show_toast(const char *t)
{
    lv_label_set_text(lv_obj_get_child(toast, 0), t);
    lv_obj_set_style_opa(toast, LV_OPA_COVER, 0);
    lv_obj_fade_out(toast, 400, 1200);
}

/* ---------------------------------------------------------------- pages */
static void show_page(int p)
{
    at = p;
    for (int i = 0; i < PG_N; i++) {
        if (i == p) lv_obj_clear_flag(pg[i], LV_OBJ_FLAG_HIDDEN);
        else        lv_obj_add_flag(pg[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(dots[i], i == p ? C_W : C_DOT, 0);
    }
}

static void gesture_cb(lv_event_t *e)
{
    (void)e;
    last_swipe = lv_tick_get();
    last_tap = 0;
    lv_dir_t d = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (d == LV_DIR_LEFT || d == LV_DIR_RIGHT) show_page((at + 1) % PG_N);
}

static void tap_cb(lv_event_t *e)
{
    (void)e;
    if (last_swipe && lv_tick_elaps(last_swipe) < 400) return;
    if (at != PG_G) return;
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

/* -------------------------------------------------------------- compass */
static const char *dir_name(float h)
{
    static const char *const EN[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    static const char *const CS[8] = { "S", "SV", "V", "JV", "J", "JZ", "Z", "SZ" };
    int i = (int)floorf((h + 22.5f) / 45.0f) & 7;
    return g_rnd_set.lang == RND_LANG_CS ? CS[i] : EN[i];
}

static void rose_draw(float hdg)
{
    for (int i = 0; i < TICKS; i++) {
        float a = (i * 10.0f - hdg) * (float)M_PI / 180.0f;
        int r1 = ROSE_R - (i % 3 == 0 ? 22 : 12);
        tick_pt[i][0].x = (lv_coord_t)(CX + sinf(a) * ROSE_R);
        tick_pt[i][0].y = (lv_coord_t)(CY - cosf(a) * ROSE_R);
        tick_pt[i][1].x = (lv_coord_t)(CX + sinf(a) * r1);
        tick_pt[i][1].y = (lv_coord_t)(CY - cosf(a) * r1);
        lv_line_set_points(tick[i], tick_pt[i], 2);
    }
    for (int i = 0; i < 4; i++) {
        float a = (i * 90.0f - hdg) * (float)M_PI / 180.0f;
        int r = ROSE_R - 44;
        at_xy(card[i], (lv_coord_t)(CX + sinf(a) * r), (lv_coord_t)(CY - cosf(a) * r));
    }
}

static void compass_build(lv_obj_t *p)
{
    title(p, TR("COMPASS", "KOMPAS"));
    circle(p, ROSE_R + 2, C_EDGE, 2);
    for (int i = 0; i < TICKS; i++) {
        tick[i] = lv_line_create(p);
        lv_obj_set_style_line_width(tick[i], i % 3 == 0 ? 4 : 2, 0);
        lv_obj_set_style_line_color(tick[i], i % 9 == 0 ? C_W : C_GREY, 0);
        lv_obj_set_style_line_rounded(tick[i], true, 0);
    }
    static const char *const EN[4] = { "N", "E", "S", "W" };
    static const char *const CS[4] = { "S", "V", "J", "Z" };
    for (int i = 0; i < 4; i++) {
        card[i] = label(p, &rnd_26, i == 0 ? C_RED : C_W);
        lv_label_set_text(card[i], g_rnd_set.lang == RND_LANG_CS ? CS[i] : EN[i]);
    }
    /* the index: where the car heads, at the top */
    static lv_point_t idx[4];
    idx[0] = (lv_point_t){ CX - 12, CY - ROSE_R - 16 };
    idx[1] = (lv_point_t){ CX + 12, CY - ROSE_R - 16 };
    idx[2] = (lv_point_t){ CX, CY - ROSE_R + 6 };
    idx[3] = idx[0];
    lv_obj_t *l = lv_line_create(p);
    lv_line_set_points(l, idx, 4);
    lv_obj_set_style_line_width(l, 4, 0);
    lv_obj_set_style_line_color(l, C_ORANGE, 0);

    deg = label(p, &rnd_84, C_W);
    deg_sign = label(p, &rnd_26, C_W);
    lv_label_set_text(deg_sign, "\xC2\xB0");
    dir = label(p, &rnd_26, C_ORANGE);
    info = label(p, &rnd_18, C_GREY);
    cmsg = label(p, &rnd_26, C_GREY);
    rose_draw(0);
}

static void compass_update(const rnd_data_t *d)
{
    char b[48];
    bool moving = d->gps.fix && d->gps.speed_kmh >= MOVING_KMH &&
                  !isnan(d->gps.course_deg);
    if (moving) last_hdg = d->gps.course_deg;

    if (!d->gps.present) {
        lv_label_set_text(cmsg, TR("NO GPS", "GPS NENÍ"));
    } else if (!d->gps.fix) {
        lv_label_set_text(cmsg, TR("NO FIX", "HLEDÁ SIGNÁL"));
    } else {
        lv_label_set_text(cmsg, "");
    }
    lv_obj_align(cmsg, LV_ALIGN_TOP_MID, 0, CY + 46);

    float target = isnan(last_hdg) ? 0 : last_hdg;
    if (isnan(shown_hdg)) shown_hdg = target;
    float diff = fmodf(target - shown_hdg + 540.0f, 360.0f) - 180.0f;
    shown_hdg = fmodf(shown_hdg + diff * 0.2f + 360.0f, 360.0f);
    if (isnan(drawn_hdg) ||
        fabsf(fmodf(shown_hdg - drawn_hdg + 540.0f, 360.0f) - 180.0f) > 0.2f) {
        rose_draw(shown_hdg);
        drawn_hdg = shown_hdg;
    }

    lv_color_t c = moving ? C_W : C_DIM;
    if (isnan(last_hdg)) {
        lv_label_set_text(deg, "--");
        lv_label_set_text(dir, "");
    } else {
        snprintf(b, sizeof b, "%d", (int)lroundf(last_hdg) % 360);
        lv_label_set_text(deg, b);
        lv_label_set_text(dir, dir_name(last_hdg));
    }
    lv_obj_set_style_text_color(deg, c, 0);
    lv_obj_set_style_text_color(deg_sign, c, 0);
    at_xy(deg, CX, CY - 10);
    lv_obj_align_to(deg_sign, deg, LV_ALIGN_OUT_RIGHT_TOP, 2, 8);
    at_xy(dir, CX, CY + 46);
    if (lv_label_get_text(cmsg)[0]) lv_obj_add_flag(dir, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(dir, LV_OBJ_FLAG_HIDDEN);

    if (!d->gps.present) {
        b[0] = 0;
    } else if (d->gps.sats >= 0 && !isnan(d->gps.speed_kmh)) {
        snprintf(b, sizeof b, "%.0f KM/H   %d SAT", d->gps.speed_kmh, d->gps.sats);
    } else if (d->gps.sats >= 0) {
        snprintf(b, sizeof b, "%d SAT", d->gps.sats);
    } else {
        b[0] = 0;
    }
    lv_label_set_text(info, b);
    at_xy(info, CX, CY + 88);
}

/* -------------------------------------------------------------- G-meter */
static void g_build(lv_obj_t *p)
{
    title(p, TR("G-METER", "G-METR"));
    int r1 = (int)(ROSE_R * 1.0f / G_FULL), r05 = (int)(ROSE_R * 0.5f / G_FULL);
    circle(p, ROSE_R, C_EDGE, 2);
    circle(p, r1, C_GREY, 2);
    circle(p, r05, C_DOT, 2);
    static lv_point_t hl[2], vl[2];
    hl[0] = (lv_point_t){ CX - ROSE_R, CY };
    hl[1] = (lv_point_t){ CX + ROSE_R, CY };
    vl[0] = (lv_point_t){ CX, CY - ROSE_R };
    vl[1] = (lv_point_t){ CX, CY + ROSE_R };
    lv_obj_t *l = lv_line_create(p);
    lv_line_set_points(l, hl, 2);
    lv_obj_set_style_line_color(l, C_DOT, 0);
    lv_obj_set_style_line_width(l, 2, 0);
    l = lv_line_create(p);
    lv_line_set_points(l, vl, 2);
    lv_obj_set_style_line_color(l, C_DOT, 0);
    lv_obj_set_style_line_width(l, 2, 0);
    lv_obj_t *s = label(p, &rnd_18, C_GREY);
    lv_label_set_text(s, "1 G");
    at_xy(s, CX + r1 * 0.72f + 22, CY - r1 * 0.72f - 4);

    for (int i = 0; i < 4; i++) {
        gpk[i] = label(p, &rnd_18, C_W);
        pk[i] = 0;
    }
    gnow = label(p, &rnd_26, C_W);
    gmsg = label(p, &rnd_18, C_REGEN);

    gdot = lv_obj_create(p);
    lv_obj_remove_style_all(gdot);
    lv_obj_set_size(gdot, 26, 26);
    lv_obj_set_style_radius(gdot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(gdot, C_CYAN, 0);
    lv_obj_set_style_bg_opa(gdot, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_color(gdot, C_CYAN, 0);
    lv_obj_set_style_shadow_width(gdot, 22, 0);
    lv_obj_set_style_shadow_spread(gdot, 2, 0);
    lv_obj_clear_flag(gdot, LV_OBJ_FLAG_CLICKABLE);
    gx_s = gy_s = 0;
}

static void g_update(const rnd_data_t *d)
{
    char b[40];
    if (!d->g.present) {
        lv_obj_add_flag(gdot, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(gnow, TR("NO SENSOR", "BEZ SNÍMAČE"));
        at_xy(gnow, CX, CY + ROSE_R / 2);
        return;
    }
    lv_obj_clear_flag(gdot, LV_OBJ_FLAG_HIDDEN);
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
    float k = ROSE_R / G_FULL;
    lv_obj_set_pos(gdot, (lv_coord_t)(CX + px * k - 13),
                   (lv_coord_t)(CY + py * k - 13));

    if (-lon > pk[0]) pk[0] = -lon;
    if (lon > pk[1]) pk[1] = lon;
    if (-lat > pk[2]) pk[2] = -lat;
    if (lat > pk[3]) pk[3] = lat;
    const char *nm_en[4] = { "BRAKE", "ACCEL", "LEFT", "RIGHT" };
    const char *nm_cs[4] = { "BRZDA", "ZRYCHL", "VLEVO", "VPRAVO" };
    for (int i = 0; i < 4; i++) {
        snprintf(b, sizeof b, "%s %.2f",
                 g_rnd_set.lang == RND_LANG_CS ? nm_cs[i] : nm_en[i], pk[i]);
        lv_label_set_text(gpk[i], b);
    }
    /* braking pushes the dot up: its peak at the top, and so on; a right
     * bend pushes it left: RIGHT on the left */
    at_xy(gpk[0], CX, CY - ROSE_R + 30);
    at_xy(gpk[1], CX, CY + ROSE_R - 30);
    at_xy(gpk[3], CX - ROSE_R + 62, CY - 18);
    at_xy(gpk[2], CX + ROSE_R - 62, CY - 18);

    snprintf(b, sizeof b, "%.2f G", sqrtf(lon * lon + lat * lat));
    lv_label_set_text(gnow, b);
    at_xy(gnow, CX, CY + ROSE_R - 64);
    lv_label_set_text(gmsg, !d->g.zeroed
        ? TR("STANDING: DOUBLE TAP = ZERO", "STOJÍŠ: 2x ŤUK = NULA") : "");
    at_xy(gmsg, CX, CY - ROSE_R / 2 - 6);
}

/* ---------------------------------------------------------------- screen */
void rnd_drive_create(void)
{
    if (scr) lv_obj_del(scr);                 /* rebuilt for a new language */
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(scr, tap_cb, LV_EVENT_SHORT_CLICKED, NULL);
    rnd_on_long(scr, back_cb);

    pg[PG_COMPASS] = page_obj();
    compass_build(pg[PG_COMPASS]);
    pg[PG_G] = page_obj();
    g_build(pg[PG_G]);
    for (int i = 0; i < PG_N; i++) {
        dots[i] = lv_obj_create(scr);
        lv_obj_remove_style_all(dots[i]);
        lv_obj_set_size(dots[i], 8, 8);
        lv_obj_set_pos(dots[i], CX - 14 + 20 * i, RND_H - 40);
        lv_obj_set_style_radius(dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dots[i], LV_OPA_COVER, 0);
    }
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
    shown_hdg = drawn_hdg = NAN;
    show_page(at);
}

void rnd_drive_open(void)
{
    lv_scr_load(scr);
}

void rnd_drive_update(const rnd_data_t *d)
{
    if (!scr || lv_scr_act() != scr) return;
    if (at == PG_COMPASS) compass_update(d);
    else g_update(d);
}
