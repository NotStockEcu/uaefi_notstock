/* Shared between the round gauge UI files, not for the platform. */
#pragma once
#include "ui_round.h"
#include "faces.h"

LV_FONT_DECLARE(rnd_112);
LV_FONT_DECLARE(rnd_84);
LV_FONT_DECLARE(rnd_56);
LV_FONT_DECLARE(rnd_26);
LV_FONT_DECLARE(rnd_18);
LV_FONT_DECLARE(rnd_barlow_46);
LV_FONT_DECLARE(rnd_barlow_23);
LV_FONT_DECLARE(rnd_barlow_20);

/* the text in the language picked in SETTINGS; the fonts carry the Czech
 * capitals (tools/gen_fonts.sh), so Czech text is upper case */
#define TR(en, cs) (g_rnd_set.lang == RND_LANG_CS ? (cs) : (en))
void rnd_lang_apply(void);        /* g_rnd_set.lang changed: rebuild */

#define C_W      lv_color_hex(0xFFFFFF)
#define C_GREY   lv_color_hex(0x8A9096)
#define C_DIM    lv_color_hex(0x5A5F66)
#define C_DOT    lv_color_hex(0x3A3F45)
#define C_RED    lv_color_hex(0xFF3030)
#define C_REGEN  lv_color_hex(0xFF9A1F)
#define C_PANEL  lv_color_hex(0x16191C)
#define C_EDGE   lv_color_hex(0x2A2D31)

/* FUTURO: neon on the hex background */
#define C_ICE       lv_color_hex(0xC8FBFF)
#define C_CYAN      lv_color_hex(0x00E5FF)
#define C_MAGENTA   lv_color_hex(0xFF2BD6)
#define C_TEAL_DIM  lv_color_hex(0x2A8C9C)
#define C_SEG_OFF   lv_color_hex(0x0C2830)
#define C_SEG_ZONE  lv_color_hex(0x4A1024)
#define C_SEG_WARN  lv_color_hex(0xFF3040)
/* VDO STYLE (RETRO): print on the dial, the red needle and band */
#define C_INK_DIM   lv_color_hex(0xA0A09C)
#define C_NEEDLE    lv_color_hex(0xE8261E)
#define C_BAND      lv_color_hex(0xC81E1E)

#define CX       (RND_W / 2)
#define ARC_MAX  1000        /* arc range: fraction of the scale * 1000 */

/* the value arc with its glow, on the faces' groove geometry */
#define N_ARC 3
void rnd_arcs(lv_obj_t *par, lv_obj_t *out[N_ARC]);
void rnd_arcs_set(lv_obj_t *a[N_ARC], float frac_1000, lv_color_t c);

lv_obj_t *rnd_label(lv_obj_t *par, const lv_font_t *f, lv_color_t c,
                    lv_coord_t y);

/* warn limits: what the settings screen may set them to */
typedef struct {
    float lo, hi, step, def;
    int dec;
} rnd_limit_t;
extern const rnd_limit_t RND_LIMIT[RND_WARN_COUNT];

/* the red zone above a warn limit, on the faces' groove */
lv_obj_t *rnd_zone(lv_obj_t *par);
lv_obj_t *rnd_zone_at(lv_obj_t *par, int r, int w, lv_color_t c);
void rnd_zone_set(lv_obj_t *zone, float frac);   /* 0..1 of the sweep */

/* ---- looks: how the gauge pages are drawn (SETTINGS -> LOOK) ----------
 * ui_round.c does the pages, the smoothing and the peaks, and hands each
 * frame to the look as a rnd_view_t. A look builds its widgets on the empty
 * gauge screen; switching looks cleans the screen and builds the new one. */
typedef struct {
    int   page;
    bool  valid;          /* there is a value */
    float frac;           /* smoothed position on the scale, 0..1 */
    float peak_frac;      /* the peak's position, NAN: none (or not kept) */
    float warn_frac;      /* red zone from here, >= 1: none */
    bool  warn;           /* over the limit */
    const char *text;     /* the readout, "--" without a value */
    const char *peak;     /* "MAX 104", "NOT READ", "NO DATA" or "" */
    bool  alert;          /* peak line is NOT READ / NO DATA */
    bool  big;            /* readout fits the big font (3 digits or less) */
    bool  regen;          /* the particulate filter is regenerating */
} rnd_view_t;

typedef struct {
    void (*build)(lv_obj_t *scr);
    void (*page)(int page);                 /* after a page change */
    void (*draw)(const rnd_view_t *v);      /* every frame */
} rnd_look_t;

extern const rnd_look_t rnd_look_notstock, rnd_look_retro, rnd_look_futuro;
void rnd_look_apply(void);                  /* g_rnd_set.look changed */

/* page names (WATER, OIL, ...) and units, in the current language; the
 * limits' names too (RND_WARN_SOOT: the DPF soot) */
const char *rnd_page_name(int page);
const char *rnd_unit(int page);

/* the shown pages in order: how many, and where a page is among them */
int rnd_pages_shown(void);
int rnd_page_pos(int page);                 /* -1: hidden */
void rnd_pages_changed(void);               /* order or hidden edited */
bool rnd_page_valid(int page);              /* a gauge or RND_MULTI */

/* MULTI (ui_round_multi.c): a layer over the look on the gauge screen, and
 * its editor screen (SETTINGS -> PAGES -> MULTI -> EDIT) */
void rnd_multi_build(lv_obj_t *gauge_scr);
void rnd_multi_show(bool on);
void rnd_multi_update(const rnd_data_t *d);
void rnd_multi_edit_create(void);
void rnd_multi_edit_open(void);
void rnd_multi_icon(lv_obj_t *par, lv_coord_t y, lv_obj_t **out);

/* page dots, shared by the looks: one per shown page, in order */
void rnd_dots(lv_obj_t *par, lv_coord_t y, lv_obj_t *out[RND_PAGES]);
void rnd_dots_set(lv_obj_t *d[RND_PAGES], int page, lv_color_t on,
                  lv_color_t off);

/* screens */
lv_obj_t *rnd_gauge_screen(void);
void rnd_menu_create(void);
void rnd_menu_open(void);
void rnd_dpf_create(void);
lv_obj_t *rnd_dpf_screen(void);
void rnd_dpf_update(const rnd_data_t *d);
void rnd_set_create(void);
void rnd_diag_create(void);
void rnd_diag_open(void);
lv_obj_t *rnd_diag_screen(void);
void rnd_diag_update(const rnd_data_t *d);
void rnd_set_open(void);
void rnd_pages_open(void);              /* SETTINGS -> PAGES */
void rnd_limits_changed(void);        /* redraw the zones */

/* day / night: a double tap on the gauges or the DPF screen */
void rnd_tap_cb(lv_event_t *e);           /* on LV_EVENT_SHORT_CLICKED */

/* a long press that leaves the screen: cb runs, and the finger, still down
 * on the next screen, counts for nothing until it is lifted (else lifting it
 * taps whatever lies under it there) */
void rnd_on_long(lv_obj_t *obj, lv_event_cb_t cb);
void rnd_swiped(void);                    /* from a gesture handler */
void rnd_backlight_apply(void);           /* from g_rnd_set */
void rnd_night_toggle(void);

/* regeneration: popup, beep, and the flag the gauges show */
#define REGEN_TEMP 400.0f    /* filter hotter than this: regenerating */
void rnd_regen_watch(const rnd_data_t *d);
bool rnd_regen_active(void);
