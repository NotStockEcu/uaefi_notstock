/* Is the particulate filter regenerating? From its surface temperature
 * (ENG105228, DID 0x1044 on the VW 2.0 TDI CR: the ECU's model of it), for
 * every gauge here (7" dash, round gauges, A4, T5 2.8).
 *
 * The temperature alone is not enough: a long pull uphill heats the filter
 * well over 400 degC without any regeneration (seen on a T5 gauge). An
 * active regeneration holds it at about 600 degC for many minutes; load
 * gets it there only now and then. So it counts as regenerating only after
 * REGEN_ON_C held for REGEN_ON_MS, and as over only after under REGEN_OFF_C
 * for REGEN_OFF_MS. Estimates, not measured on a regeneration yet: the
 * ECU's own regeneration status (to be sniffed from VCDS) would replace
 * all this.
 *
 * Header only, platform-free: the caller gives the time in ms.
 */
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define REGEN_ON_C    580.0f
#define REGEN_ON_MS   60000u
#define REGEN_OFF_C   500.0f
#define REGEN_OFF_MS  30000u

typedef struct {
    bool on;
    bool timing;          /* past the threshold, counting */
    uint32_t since;       /* ms, when it got past */
} regen_det_t;

/* a new temperature (NAN: not read, nothing changes); true: the state
 * flipped, see r->on */
static inline bool regen_step(regen_det_t *r, float temp_c, uint32_t now_ms)
{
    if (isnan(temp_c)) return false;
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
