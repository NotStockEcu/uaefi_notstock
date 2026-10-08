/* G-meter frame, see gmeter.h */
#include "gmeter.h"

#include <math.h>
#include <string.h>

#define LEARN_MIN_DVDT  0.7f     /* m/s per s: clearly speeding up / braking */
#define LEARN_ENOUGH    6        /* samples before the learnt axis is used */

static float dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static bool norm(float v[3])
{
    float l = sqrtf(dot(v, v));
    if (l < 1e-4f) return false;
    for (int i = 0; i < 3; i++) v[i] /= l;
    return true;
}

/* v minus its component along up */
static void horiz(const float up[3], const float v[3], float out[3])
{
    float k = dot(v, up);
    for (int i = 0; i < 3; i++) out[i] = v[i] - k * up[i];
}

/* the default forward: out of the board's back (-Z), else -Y if the board
 * lies flat */
static void default_fwd(gm_cal_t *c)
{
    const float back[3] = { 0, 0, -1 }, down_y[3] = { 0, -1, 0 };
    horiz(c->up, back, c->fwd);
    if (!norm(c->fwd)) {
        horiz(c->up, down_y, c->fwd);
        norm(c->fwd);
    }
}

void gm_cal_default(gm_cal_t *c)
{
    memset(c, 0, sizeof *c);
    c->version = GM_CAL_VERSION;
    /* upright, screen to the driver: up is the board's +Y */
    c->up[1] = 1;
    default_fwd(c);
}

void gm_zero(gm_cal_t *c, const float a[3])
{
    float up[3] = { a[0], a[1], a[2] };
    if (!norm(up)) return;
    memcpy(c->up, up, sizeof up);
    c->zeroed = 1;
    /* a new up: what was learnt about forward, back to the horizontal */
    if (c->learnt >= LEARN_ENOUGH) {
        float f[3];
        horiz(c->up, c->learn, f);
        if (norm(f)) {
            memcpy(c->fwd, f, sizeof f);
            return;
        }
    }
    default_fwd(c);
}

void gm_project(const gm_cal_t *c, const float a[3], float *lon, float *lat)
{
    float h[3], right[3];
    horiz(c->up, a, h);
    /* right = forward x up */
    right[0] = c->fwd[1] * c->up[2] - c->fwd[2] * c->up[1];
    right[1] = c->fwd[2] * c->up[0] - c->fwd[0] * c->up[2];
    right[2] = c->fwd[0] * c->up[1] - c->fwd[1] * c->up[0];
    *lon = dot(h, c->fwd);
    *lat = dot(h, right);
}

bool gm_learn(gm_cal_t *c, const float a_avg[3], float dvdt)
{
    if (fabsf(dvdt) < LEARN_MIN_DVDT) return false;
    float h[3];
    horiz(c->up, a_avg, h);
    for (int i = 0; i < 3; i++) c->learn[i] += h[i] * dvdt;
    if (c->learnt < 60000) c->learnt++;
    if (c->learnt < LEARN_ENOUGH) return false;
    float f[3];
    horiz(c->up, c->learn, f);
    if (!norm(f)) return false;
    memcpy(c->fwd, f, sizeof f);
    return true;
}
