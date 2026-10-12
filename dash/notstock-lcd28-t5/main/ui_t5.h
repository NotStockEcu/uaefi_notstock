/* The 2.8" T5.1 gauge (320 x 240, landscape) in the round gauge's NOTSTOCK
 * look (../notstock-round): on the left one big round dial (pre-rendered
 * face, glowing white value arc, red zone from the limit), swipe for the
 * next of the values chosen for it; on the right three small round dials.
 * Long press: the menu,
 *   STAV DPF     soot, pressure, filter temperature, distance since the
 *                regeneration, measured soot
 *   DIAGNOSTIKA  trouble codes, read and cleared
 *   NASTAVENÍ    LIMITY (each value's red limit), ZOBRAZENÍ (which values
 *                the big dial shows, what the small ones show), KONTRAST
 *                (normal / high), ZVUK (beeps on or off, volume, a test),
 *                JAZYK (Czech / English)
 * Swipe down: night (dimmer), swipe up: day.
 * A regeneration (the filter's surface temperature over 400 degC) beeps
 * three times, brings up STAV DPF and puts REGENERACE DPF on the dial; its
 * end beeps once and goes back to the gauges.
 * Platform-free (tools/sim).
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#define T5_W 320
#define T5_H 240

/* what the platform delivers */
enum { T5_BOOST, T5_EGT, T5_OILT, T5_CLT, T5_IAT, T5_RPM, T5_SPEED,
       T5_SOOT, T5_SOOT_MEAS, T5_DP, T5_KM, T5_DPF_T, T5_N };
enum { T5_ST_POLLING, T5_ST_SCANNING, T5_ST_NO_ECU };

/* trouble codes, as the OBD client has them (obd2.h, same numbers) */
#define T5_DTC_MAX 24
enum { T5_DTC_STORED = 1, T5_DTC_PENDING = 2 };                /* kind bits */
enum { T5_DTC_IDLE, T5_DTC_READING, T5_DTC_CLEARING };         /* busy */
enum { T5_DTC_NOT_READ, T5_DTC_READ, T5_DTC_NO_ANSWER,         /* result */
       T5_DTC_CLEARED, T5_DTC_REFUSED };
typedef struct {
    uint8_t  busy, result, nrc, n, more;
    uint16_t seq;
    struct { uint16_t code; uint8_t ecu, kind; } list[T5_DTC_MAX];
} t5_dtc_t;

/* what the screens show, NAN: no value */
typedef struct {
    float x[T5_N];        /* boost: bar over the barometer; DP mbar */
    bool  link;           /* the ECU answered lately */
    int   state;          /* T5_ST_* */
    t5_dtc_t dtc;
} t5_view_t;

/* the values a dial can show, and that have a limit */
enum { T5_CH_BOOST, T5_CH_EGT, T5_CH_RPM, T5_CH_IAT, T5_CH_OIL, T5_CH_CLT,
       T5_CH_SOOT, T5_CH_SPEED, T5_CH_N };
#define T5_SMALL 3
enum { T5_LANG_CS, T5_LANG_EN };
enum { T5_SND_OFF, T5_SND_BEEP };

/* settings: the UI edits them in place; the platform loads them before
 * ui_t5_create() (or keeps t5_settings_defaults()) and stores them when
 * t5_settings_save() is called */
#define T5_SET_VER 2
typedef struct {
    uint8_t ver;
    uint8_t lang;                 /* T5_LANG_* */
    uint8_t sound;                /* T5_SND_* */
    uint8_t volume;               /* 1..3 */
    uint8_t contrast;             /* 1: high (brighter text, scales) */
    uint8_t big;                  /* bit per T5_CH_*: on the big dial */
    uint8_t small[T5_SMALL];      /* T5_CH_* on the small dials */
    float   limit[T5_CH_N];       /* red from here */
} t5_settings_t;
extern t5_settings_t g_t5_set;
void t5_settings_defaults(void);
bool t5_settings_valid(const t5_settings_t *s);

/* by the platform */
void t5_beep(int n);                    /* n short beeps, if the sound is on
                                           (g_t5_set); must not block */
void t5_backlight(uint8_t percent);
void t5_dtc_read(void);
void t5_dtc_clear(void);
void t5_settings_save(void);

void ui_t5_create(void);                 /* builds the screens, loads the gauges */
void ui_t5_update(const t5_view_t *v);   /* ~30 Hz */
void ui_t5_page(int ch);                 /* the big dial shows T5_CH_* */
enum { T5_SCR_GAUGES, T5_SCR_MENU, T5_SCR_DPF, T5_SCR_DIAG, T5_SCR_SET,
       T5_SCR_LIMITS, T5_SCR_VIEW, T5_SCR_SOUND, T5_SCR_LANG, T5_SCR_N };
void ui_t5_open(int screen);             /* for the simulator */
