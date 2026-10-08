/* Host test of gmeter.c.
 * cc -I../../main test_motion.c ../../main/gmeter.c -lm */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gmeter.h"

static int fails;
#define NEAR(a, b, e) do { if (fabsf((float)(a) - (float)(b)) > (e)) { \
    printf("FAIL %s = %g, want %g\n", #a, (double)(a), (double)(b)); fails++; } } while (0)

int main(void)
{
    /* G-meter: board upright, screen to the driver: gravity on +Y */
    gm_cal_t cal;
    gm_cal_default(&cal);
    float rest[3] = { 0, 1, 0 };
    gm_zero(&cal, rest);
    float lon, lat;
    /* car speeds up: the reading leans backwards of the car = -Z of the
     * board... the default forward is -Z, so +0.3 g forward is z = -0.3 */
    float acc[3] = { 0, 1, -0.3f };
    gm_project(&cal, acc, &lon, &lat);
    NEAR(lon, 0.3f, 1e-4f); NEAR(lat, 0, 1e-4f);
    /* mounted the other way round: learning turns forward over */
    gm_cal_t c2;
    gm_cal_default(&c2);
    gm_zero(&c2, rest);
    float push[3] = { 0, 1, 0.25f };           /* real forward is +Z */
    for (int i = 0; i < 8; i++) gm_learn(&c2, push, 2.0f);
    gm_project(&c2, push, &lon, &lat);
    NEAR(lon, 0.25f, 1e-3f);
    /* tilted mount: gravity partly on Z; a sideways push shows as lateral */
    gm_cal_t c3;
    gm_cal_default(&c3);
    float tilt[3] = { 0, 0.866f, 0.5f };
    gm_zero(&c3, tilt);
    float side[3] = { 0.4f, 0.866f, 0.5f };
    gm_project(&c3, side, &lon, &lat);
    NEAR(lon, 0, 1e-3f); NEAR(fabsf(lat), 0.4f, 1e-3f);

    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
