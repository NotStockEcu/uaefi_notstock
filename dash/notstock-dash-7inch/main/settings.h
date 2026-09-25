/* Persistent settings, stored in NVS so they survive a power cut.
 *
 * Everything the driver can change from the on-screen menu lives here. The
 * firmware never hard-codes a threshold: ui.c and rusefi_can.c read this
 * struct, so changing a limit takes a tap, not a rebuild.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    /* alarm thresholds. A gauge readout turns red on its own limit; the
     * full-screen flash is the shift light and fires on rpm_flash only.
     * The scales and the rev counter redline are baked into the artwork,
     * see tools/gen_dials.py. */
    uint16_t rpm_flash;         /* rpm, 0 disables */
    uint8_t  clt_warn;          /* deg C */
    uint8_t  iat_warn;          /* deg C */
    uint16_t boost_warn;        /* bar * 100 */
    uint16_t afr_lean_warn;     /* afr * 10, high limit, 0 disables */

    bool     flash_enable;      /* master switch for the shift flash */
    uint8_t  flash_intensity;   /* 0..100, peak opacity of the wash */
    uint8_t  flash_area;        /* FLASH_AREA_* */
    uint8_t  flash_colour;      /* FLASH_COLOUR_* */
    uint16_t flash_period;      /* ms for one on + off cycle */

    uint8_t  brightness;        /* 15..100 */
    bool     night;             /* night mode, long press bottom left */
    uint8_t  night_level;       /* % of the night wash, 20..80 */
    uint8_t  stoich;            /* afr * 10: 147 petrol, 98 E85 */
    uint16_t baro;              /* bar * 100 subtracted from MAP */
    bool     demo;              /* run off the synthetic generator */
    uint8_t  log_mask;          /* LOG screen channels shown, bit per channel */
    uint8_t  look;              /* LOOK_*, see ui_theme.h */
} settings_t;

enum { FLASH_AREA_SCREEN, FLASH_AREA_DIAL };
enum { FLASH_COLOUR_RED, FLASH_COLOUR_WHITE, FLASH_COLOUR_BLUE,
       FLASH_COLOUR_AMBER };

extern settings_t g_set;

void settings_load(void);
void settings_save(void);
void settings_defaults(void);

/* convenience, since most call sites want floats */
static inline float set_boost_warn(void) { return g_set.boost_warn / 100.0f; }
static inline float set_stoich(void)     { return g_set.stoich / 10.0f; }
static inline float set_baro(void)       { return g_set.baro / 100.0f; }
