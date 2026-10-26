/* G-meter: the accelerometer's reading turned into the car's frame,
 * whatever way the gauge is mounted. ESP-free.
 *
 * Zero (standing still): the gravity vector gives "up". Forward is learnt
 * while driving: the horizontal acceleration that goes with the speed
 * (OBD) rising or falling points forward; until enough of that has been seen,
 * forward is the way the screen's back faces (the screen looks at the
 * driver). Lateral: positive to the right.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float up[3];          /* unit vector, board frame */
    float fwd[3];         /* unit vector, horizontal */
    float learn[3];       /* sum of a_h * dv/dt while driving */
    uint16_t learnt;      /* samples in learn */
    uint8_t zeroed;       /* up came from a zero */
    uint8_t version;
} gm_cal_t;

#define GM_CAL_VERSION 1

void gm_cal_default(gm_cal_t *c);
/* zero: the board at rest, its averaged reading in g */
void gm_zero(gm_cal_t *c, const float a[3]);
/* a reading (g) -> longitudinal (+ forward) and lateral (+ right), g */
void gm_project(const gm_cal_t *c, const float a[3], float *lon, float *lat);
/* while driving: the reading averaged over the last speed interval and the
 * change of speed (m/s per s); true when forward moved */
bool gm_learn(gm_cal_t *c, const float a_avg[3], float dvdt);
