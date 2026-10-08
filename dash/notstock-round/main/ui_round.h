/* Round gauge UI: one value at a time on a round panel, swipe left / right
 * for the next one. Long press anywhere: the menu (gauges, DPF status,
 * diagnostics: the trouble codes, read and cleared over OBD; G-meter, where
 * the board has an accelerometer; settings: look, pages, night level, beep on/off, warn limits, language
 * English / Czech). Double tap:
 * night
 * (backlight down to the night level) and back to day.
 * A particulate filter regeneration pops up over whatever is shown, with a
 * beep. ESP-free, so tools/sim renders it on a PC.
 *
 * Panel size: 480 for the Waveshare ESP32-S3-Touch-LCD-2.1 (default), 466
 * for a 1.32" AMOLED (-DRND_SIZE=466). Everything is laid out from the
 * centre, so either works.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifndef RND_SIZE
#define RND_SIZE 480
#endif
#define RND_W RND_SIZE
#define RND_H RND_SIZE

/* page order; must match PAGES in tools/gen_faces.py */
enum { RND_WATER, RND_OIL, RND_BOOST, RND_INTAKE, RND_EXHAUST, RND_RPM,
       RND_COUNT };
/* MULTI: one more page, several values at once (ui_round_multi.c). Its id
 * is past the gauges and past RND_WARN_SOOT, which shares their numbering */
#define RND_MULTI 7
#define RND_PAGES (RND_COUNT + 1)        /* gauges + MULTI */

/* OBD trouble codes, as the platform's OBD layer has them (the 7" dash's
 * obd2.c: g_obd.dtc, same numbers) */
#define RND_DTC_MAX 24
enum { RND_DTC_STORED = 1, RND_DTC_PENDING = 2 };             /* kind bits */
enum { RND_DTC_IDLE, RND_DTC_READING, RND_DTC_CLEARING };     /* busy */
enum { RND_DTC_NOT_READ, RND_DTC_READ, RND_DTC_NO_ANSWER,     /* result */
       RND_DTC_CLEARED, RND_DTC_REFUSED };
typedef struct {
    uint16_t code;        /* SAE J2012, 0x0401 = P0401 */
    uint8_t  ecu;         /* answered on 0x7E8 + ecu */
    uint8_t  kind;
} rnd_dtc_t;
typedef struct {
    uint8_t  busy, result;
    uint8_t  nrc;         /* why clearing was refused (7F 04 nrc) */
    uint8_t  n, more;     /* codes in list; more: the list overflowed */
    uint16_t seq;         /* counts finished reads */
    rnd_dtc_t list[RND_DTC_MAX];
} rnd_dtc_status_t;

/* live values, NAN where unknown; link false: nothing from the car */
typedef struct {
    float v[RND_COUNT];
    struct {                  /* particulate filter (VW UDS, see the 7" dash) */
        float soot_g;         /* calculated */
        float soot_meas_g;    /* measured */
        float dp_hpa;         /* differential pressure */
        float dist_km;        /* since the last regeneration */
        float temp_c;         /* simulated filter surface temperature */
    } dpf;
    rnd_dtc_status_t dtc;
    bool  link;
    struct {                  /* accelerometer, in the car's frame */
        bool  present;
        bool  zeroed;         /* "up" from a zero, not assumed */
        bool  learnt;         /* "forward" learnt while driving */
        float lon_g;          /* + speeding up, - braking */
        float lat_g;          /* + to the right */
    } g;
} rnd_data_t;

/* Settings. The UI edits g_rnd_set in place; the platform loads it before
 * ui_round_create() (or keeps rnd_settings_defaults()) and stores it when
 * rnd_settings_save() is called, on leaving the settings screens. */
enum { RND_WARN_SOOT = RND_COUNT, RND_WARN_COUNT };  /* after the pages */
enum { RND_LOOK_NOTSTOCK, RND_LOOK_RETRO, RND_LOOK_FUTURO, RND_LOOK_COUNT };
enum { RND_LANG_EN, RND_LANG_CS, RND_LANG_COUNT };
/* what sounds when a regeneration starts and ends: beeps (every board),
 * bell tones, a gong or a voice (the boards with a speaker; the buzzer of
 * the 2.1" beeps for all of them) */
enum { RND_SND_OFF, RND_SND_BEEP, RND_SND_CHIME, RND_SND_GONG, RND_SND_VOICE,
       RND_SND_COUNT };
enum { RND_EV_REGEN_START, RND_EV_REGEN_END };
/* MULTI's slots: 0 the big one, 1..3 the small ones below. Each holds a
 * gauge (RND_WATER..RND_RPM), RND_WARN_SOOT (DPF soot) or RND_MV_NONE */
#define RND_MULTI_SLOTS 4
#define RND_MV_NONE 0xFF

typedef struct {
    uint8_t look;                     /* RND_LOOK_* */
    uint8_t sound;                    /* RND_SND_*: regeneration start / end */
    bool    night;                    /* backlight at night_level */
    uint8_t night_level;              /* % of full, 10..50 */
    uint8_t order[RND_PAGES];         /* the pages in swipe order */
    uint8_t hidden;                   /* bit per page id (RND_*): left out */
    float   warn[RND_WARN_COUNT];     /* red above this: pages, DPF soot g */
    uint8_t lang;                     /* RND_LANG_* */
    uint8_t multi[RND_MULTI_SLOTS];   /* what MULTI shows */
} rnd_settings_t;

extern rnd_settings_t g_rnd_set;
void rnd_settings_defaults(void);

/* provided by the platform */
void rnd_sound(int event);            /* RND_EV_*, in g_rnd_set.sound; must
                                         not block */
void rnd_settings_save(void);         /* store g_rnd_set */
void rnd_backlight(uint8_t percent);  /* 0..100 */
void rnd_dtc_read(void);              /* read the trouble codes (03, 07) */
void rnd_dtc_clear(void);             /* clear them (04), then read again */
void rnd_g_zero(void);                /* G-meter: the car stands, zero it */

/* builds every screen; boot: the NOT STOCK logo first, fading in from black
 * and then into the gauges (RND_BOOT_MS in all), else the gauges at once */
void ui_round_create(bool boot);
#define RND_BOOT_IN_MS    1200    /* logo out of black */
#define RND_BOOT_HOLD_MS  1500
#define RND_BOOT_X_MS     900     /* logo into the gauges */
#define RND_BOOT_MS (RND_BOOT_IN_MS + RND_BOOT_HOLD_MS + RND_BOOT_X_MS)
void ui_round_update(const rnd_data_t *d);   /* call at ~30 Hz */
void ui_round_page(int page);                 /* what a swipe does */
int  ui_round_current(void);
