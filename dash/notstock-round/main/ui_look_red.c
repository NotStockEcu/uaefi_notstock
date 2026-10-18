/* RED look: the A4 gauge's dials (../../notstock-a4, after the A4 B8 cluster
 * at night) on the round gauge. Pre-rendered per page (tools/gen_faces_red.py:
 * black face, red lit rim, white dashes); live: the numerals (white, red
 * from the limit), red ticks over the white ones past the limit and a red
 * band inside them (finely broken at the limit, the pieces growing until
 * it is solid), the red needle and its black hub, and in the free lower
 * right the value, name and unit. No maker's logo. */
#include "ui_round_int.h"
#include "red.h"

#include <math.h>

LV_FONT_DECLARE(red_num_52);
LV_FONT_DECLARE(red_fis_54);
LV_FONT_DECLARE(red_txt_22);

#define C_VAL    lv_color_hex(0xEEEEEC)
#define C_RED2   lv_color_hex(0xF02A26)
#define C_SMALL  lv_color_hex(0xA0A0A0)
#define C_AMBER  lv_color_hex(0xFFA800)
#define TX       (CX + RND_W * 104 / 466)     /* the lower right's column */
#define TW       150

#define MAX_NUMS   8
#define MAX_TICKS  40
#define MAX_BAND   96
#define CHORD_DEG  4.0f

static lv_obj_t *face, *needle, *hub, *val, *line_l, *name_l, *unit_l, *dot[RND_PAGES];
static lv_obj_t *num[MAX_NUMS], *tick[MAX_TICKS], *band[MAX_BAND], *band_end;
static lv_point_t tick_pt[MAX_TICKS][2], band_pt[MAX_BAND][2];
static const red_scale_t *sc;
static int pg_now;
static float warn_at, needle_at;
static int warn_on, line_on;

static lv_obj_t *label(lv_obj_t *par, const lv_font_t *f, lv_color_t c)
{
    lv_obj_t *l = lv_label_create(par);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, c, 0);
    lv_label_set_text(l, "");
    return l;
}

static lv_obj_t *red_line(lv_obj_t *par, lv_coord_t w)
{
    lv_obj_t *l = lv_line_create(par);
    lv_obj_set_pos(l, 0, 0);
    lv_obj_set_style_line_color(l, C_RED2, 0);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

static void build(lv_obj_t *scr)
{
    face = lv_img_create(scr);
    lv_obj_set_pos(face, 0, 0);
    lv_obj_clear_flag(face, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < MAX_BAND; i++) band[i] = red_line(scr, RED_R_BAND1 - RED_R_BAND0);
    band_end = lv_arc_create(scr);
    lv_obj_remove_style_all(band_end);
    lv_obj_set_size(band_end, 2 * RED_R_BAND1, 2 * RED_R_BAND1);
    lv_obj_center(band_end);
    lv_arc_set_bg_angles(band_end, 0, 0);
    lv_obj_set_style_arc_width(band_end, RED_R_BAND1 - RED_R_BAND0, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(band_end, C_RED2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(band_end, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(band_end, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(band_end, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(band_end, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < MAX_TICKS; i++) tick[i] = red_line(scr, 5);
    for (int i = 0; i < MAX_NUMS; i++) {
        num[i] = label(scr, &red_num_52, C_VAL);
        lv_obj_add_flag(num[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* lower right: the small line (REGEN, NO DATA), the value, name, unit */
    line_l = label(scr, &red_txt_22, C_SMALL);
    lv_obj_set_width(line_l, TW);
    lv_obj_set_style_text_align(line_l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(line_l, TX - 22 - TW / 2, CX + 8);
    val = label(scr, &red_fis_54, C_VAL);
    lv_obj_set_width(val, TW);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(val, TX - TW / 2, CX + 42);
    name_l = label(scr, &red_txt_22, C_VAL);
    lv_obj_set_width(name_l, TW);
    lv_obj_set_style_text_align(name_l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(name_l, TX - TW / 2, CX + 108);
    unit_l = label(scr, &red_txt_22, C_SMALL);
    lv_obj_set_width(unit_l, TW);
    lv_obj_set_style_text_align(unit_l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(unit_l, TX - TW / 2, CX + 136);
    rnd_dots(scr, 0, dot);
    /* the dots under the unit, as on the A4, clear of the bottom numeral */
    lv_obj_align(lv_obj_get_parent(dot[0]), LV_ALIGN_TOP_LEFT, TX - 110, CX + 166);

    needle = lv_img_create(scr);
    lv_img_set_src(needle, &red_needle);
    lv_obj_set_pos(needle, CX, CX - RED_NEEDLE_PY);
    lv_img_set_pivot(needle, 0, RED_NEEDLE_PY);
    lv_img_set_antialias(needle, true);
    lv_img_set_angle(needle, (int16_t)(RED_START_DEG * 10));
    lv_obj_clear_flag(needle, LV_OBJ_FLAG_CLICKABLE);
    hub = lv_img_create(scr);
    lv_img_set_src(hub, &red_cap);
    lv_obj_set_pos(hub, CX - red_cap.header.w / 2, CX - red_cap.header.h / 2);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_CLICKABLE);
}

/* ------------------------------------------- what is red past the limit */
static int band_piece(int n, float a0, float a1)
{
    const float r = (RED_R_BAND0 + RED_R_BAND1) / 2.0f;
    int k = (int)ceilf((a1 - a0) / CHORD_DEG);
    if (k < 1) k = 1;
    for (int j = 0; j < k && n < MAX_BAND; j++, n++) {
        float b0 = (a0 + (a1 - a0) * j / k) * (float)M_PI / 180.0f;
        float b1 = (a0 + (a1 - a0) * (j + 1) / k) * (float)M_PI / 180.0f;
        band_pt[n][0].x = (lv_coord_t)lroundf(CX + r * cosf(b0));
        band_pt[n][0].y = (lv_coord_t)lroundf(CX + r * sinf(b0));
        band_pt[n][1].x = (lv_coord_t)lroundf(CX + r * cosf(b1));
        band_pt[n][1].y = (lv_coord_t)lroundf(CX + r * sinf(b1));
        lv_line_set_points(band[n], band_pt[n], 2);
        lv_obj_clear_flag(band[n], LV_OBJ_FLAG_HIDDEN);
    }
    return n;
}

/* numerals white or red, red ticks past the limit, and the band from the
 * limit to the end: short pieces with gaps at the limit, growing until
 * they close up into a solid band */
static void draw_scale(float warn_frac)
{
    float w = sc->lo + warn_frac * (sc->hi - sc->lo);
    int nb = 0, nt = 0;
    float solid_from = NAN;
    for (int i = 0; i < MAX_NUMS; i++) {
        if (i >= sc->n_nums) {
            lv_obj_add_flag(num[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const red_num_t *nm = &sc->nums[i];
        lv_label_set_text_static(num[i], nm->t);
        lv_obj_set_style_text_color(num[i], nm->v >= w - 1e-4f ? C_RED2 : C_VAL, 0);
        lv_obj_align(num[i], LV_ALIGN_CENTER, (lv_coord_t)lroundf(nm->x - CX),
                     (lv_coord_t)lroundf(nm->y - CX));
        lv_obj_clear_flag(num[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < sc->n_ticks && nt < MAX_TICKS; i++) {
        const red_tick_t *t = &sc->ticks[i];
        if (t->v < w - 1e-4f) continue;
        tick_pt[nt][0].x = (lv_coord_t)lroundf(t->x0);
        tick_pt[nt][0].y = (lv_coord_t)lroundf(t->y0);
        tick_pt[nt][1].x = (lv_coord_t)lroundf(t->x1);
        tick_pt[nt][1].y = (lv_coord_t)lroundf(t->y1);
        lv_line_set_points(tick[nt], tick_pt[nt], 2);
        lv_obj_set_style_line_width(tick[nt], (lv_coord_t)(t->w + 1.5f), 0);
        lv_obj_clear_flag(tick[nt], LV_OBJ_FLAG_HIDDEN);
        nt++;
    }
    if (warn_frac < 1) {
        float a0 = RED_START_DEG + RED_SWEEP_DEG * (warn_frac < 0 ? 0 : warn_frac);
        float a1 = RED_START_DEG + RED_SWEEP_DEG;
        float span = a1 - a0;
        for (float pos = a0; span > 0.3f && pos < a1;) {
            float t = (pos - a0) / span;
            float dash = 1.0f + 9.0f * powf(t, 1.3f);
            float gap = 2.4f * (1.0f - t / 0.8f);
            if (gap < 0.4f) {
                solid_from = pos;
                break;
            }
            nb = band_piece(nb, pos, fminf(pos + dash, a1));
            pos += dash + gap;
        }
    }
    for (int i = nt; i < MAX_TICKS; i++) lv_obj_add_flag(tick[i], LV_OBJ_FLAG_HIDDEN);
    for (int i = nb; i < MAX_BAND; i++) lv_obj_add_flag(band[i], LV_OBJ_FLAG_HIDDEN);
    float end = RED_START_DEG + RED_SWEEP_DEG;
    if (!isnan(solid_from) && end - solid_from >= 1.0f) {
        lv_arc_set_angles(band_end, (uint16_t)lroundf(solid_from) % 360,
                          (uint16_t)lroundf(end) % 360);
        lv_obj_clear_flag(band_end, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(band_end, LV_OBJ_FLAG_HIDDEN);
    }
}

/* the numerals of exhaust and rpm are in hundreds and thousands */
static const char *unit_of(int pg)
{
    if (pg == RND_EXHAUST) return "\xC2\xB0" "C \xC3\x97" "100";
    if (pg == RND_RPM) return TR("rpm \xC3\x97" "1000", "ot/min \xC3\x97" "1000");
    return rnd_unit(pg);
}

static void page(int pg)
{
    pg_now = pg;
    sc = &RED_SCALE[pg];
    lv_img_set_src(face, red_face[pg]);
    lv_label_set_text(name_l, rnd_page_name(pg));
    lv_label_set_text(unit_l, unit_of(pg));
    warn_at = needle_at = NAN;
    warn_on = line_on = -1;
    rnd_dots_set(dot, pg, lv_color_hex(0xD8D8D8), lv_color_hex(0x3A3A3E));
}

static void draw(const rnd_view_t *v)
{
    if (v->warn_frac != warn_at) {
        warn_at = v->warn_frac;
        draw_scale(v->warn_frac);
    }
    float f = v->frac > 1.02f ? 1.02f : v->frac < 0 ? 0 : v->frac;
    if (isnan(needle_at) || fabsf(f - needle_at) > 0.0005f) {
        needle_at = f;
        lv_img_set_angle(needle, (int16_t)lroundf((RED_START_DEG + RED_SWEEP_DEG * f) * 10.0f));
    }
    lv_label_set_text(val, v->text);
    /* over the limit: red, blinking */
    bool blink = v->warn && (lv_tick_get() / 500) % 2;
    int w = v->warn ? (blink ? 2 : 1) : 0;
    if (w != warn_on) {
        warn_on = w;
        lv_obj_set_style_text_color(val, v->warn ? C_RED2 : C_VAL, 0);
        lv_obj_set_style_text_opa(val, w == 2 ? LV_OPA_30 : LV_OPA_COVER, 0);
    }
    /* the small line: the regeneration, else what is wrong with the data */
    int ln = v->regen ? 2 : v->alert ? 1 : 0;
    if (ln != line_on) {
        line_on = ln;
        lv_obj_set_style_text_color(line_l, ln == 2 ? C_AMBER : C_RED2, 0);
    }
    lv_label_set_text(line_l, ln == 2 ? TR("DPF REGEN", "REGENERACE") : ln ? v->peak : "");
}

const rnd_look_t rnd_look_red = { build, page, draw };
