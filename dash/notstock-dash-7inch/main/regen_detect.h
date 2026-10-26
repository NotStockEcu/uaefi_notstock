/* Is the particulate filter regenerating? From its surface temperature
 * (ENG105228, DID 0x1044 on the VW 2.0 TDI CR: the ECU's model of it) and
 * the distance since the last regeneration, for every gauge here (7" dash,
 * round gauges, A4, T5 2.8).
 *
 * Start: the temperature alone, held. A long pull uphill heats the filter
 * well over 400 degC without any regeneration (seen on a T5 gauge); an
 * active regeneration holds it near 600 degC for many minutes. So it starts
 * once REGEN_ON_C held for REGEN_ON_MS (seen on the 1.85": 580 degC held a
 * minute came well into a regeneration).
 *
 * End: the ECU sets the distance since the last regeneration back to 0 as
 * it finishes one: the distance falling is the end, at once (the filter
 * then stays hot for kilometres). Under REGEN_OFF_C for REGEN_OFF_MS too, for
 * one broken off (no new distance). After an end, no new start until the
 * filter was under REGEN_OFF_C: it is still hot.
 *
 * Header only, platform-free: the caller gives the time in ms; NAN for what
 * it could not read.
 */
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define REGEN_ON_C     560.0f
#define REGEN_ON_MS    30000u
#define REGEN_OFF_C    500.0f
#define REGEN_OFF_MS   30000u
#define REGEN_KM_DROP  0.5f       /* km: the distance set back, not noise */

typedef struct {
    bool on;
    bool timing;          /* past the threshold, counting */
    bool cool_wait;       /* ended: no start until under REGEN_OFF_C */
    uint32_t since;       /* ms, when it got past */
    float km;             /* the distance last read (NAN: none yet) */
} regen_det_t;

#define REGEN_DET_INIT { false, false, false, 0, NAN }

/* a new reading (NAN: not read, that part changes nothing); true: the state
 * flipped, see r->on */
static inline bool regen_step(regen_det_t *r, float temp_c, float dist_km,
                              uint32_t now_ms)
{
    /* the distance set back to 0: a regeneration finished */
    bool reset = false;
    if (!isnan(dist_km)) {
        if (!isnan(r->km) && dist_km < r->km - REGEN_KM_DROP) reset = true;
        r->km = dist_km;
    }
    if (r->on && reset) {
        r->on = false;
        r->timing = false;
        r->cool_wait = true;
        return true;
    }
    if (isnan(temp_c)) return false;
    if (r->cool_wait) {
        if (temp_c < REGEN_OFF_C) r->cool_wait = false;
        else return false;
    }
    bool past = r->on ? temp_c < REGEN_OFF_C : temp_c >= REGEN_ON_C;
    if (!past) {
        r->timing = false;
        return false;
    }
    if (!r->timing) {
        r->timing = true;
        r->since = now_ms;
    }
    if (now_ms - r->since < (r->on ? REGEN_OFF_MS : REGEN_ON_MS)) return false;
    r->on = !r->on;
    r->timing = false;
    return true;
}
