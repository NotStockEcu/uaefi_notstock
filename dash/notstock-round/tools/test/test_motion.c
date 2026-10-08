/* Host test of nmea.c and gmeter.c.
 * cc -I../../main test_motion.c ../../main/nmea.c ../../main/gmeter.c -lm */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gmeter.h"
#include "nmea.h"

static int fails;
#define NEAR(a, b, e) do { if (fabsf((float)(a) - (float)(b)) > (e)) { \
    printf("FAIL %s = %g, want %g\n", #a, (double)(a), (double)(b)); fails++; } } while (0)

static void feed(nmea_t *n, const char *body)
{
    unsigned char c = 0;
    for (const char *p = body; *p; p++) c ^= (unsigned char)*p;
    char line[128];
    snprintf(line, sizeof line, "$%s*%02X\r\n", body, c);
    nmea_feed(n, line, (int)strlen(line));
}

int main(void)
{
    nmea_t n;
    nmea_init(&n);
    feed(&n, "GNRMC,123519.00,A,4807.038,N,01131.000,E,40.0,247.3,230394,,,A,V");
    NEAR(n.fix, 1, 0); NEAR(n.speed_kmh, 74.08f, 0.01f); NEAR(n.course_deg, 247.3f, 0.01f);
    feed(&n, "GNGGA,123519.00,4807.038,N,01131.000,E,1,11,0.9,545.4,M,46.9,M,,");
    NEAR(n.sats, 11, 0); NEAR(n.alt_m, 545.4f, 0.01f);
    /* a broken checksum changes nothing */
    nmea_feed(&n, "$GNRMC,1,A,1,N,1,E,99.0,10.0,1,,,A,V*00\r\n", 41);
    NEAR(n.course_deg, 247.3f, 0.01f);
    feed(&n, "GNRMC,123520.00,V,,,,,,,230394,,,N,V");
    NEAR(n.fix, 0, 0); NEAR(isnan(n.course_deg), 1, 0);
    /* split across two reads */
    const char *a = "$GPGGA,1,2,N,3,E,1,07,1.0,12.5,M,0,M,,*";
    char line[96];
    unsigned char c = 0;
    for (const char *p = a + 1; *p != '*'; p++) c ^= (unsigned char)*p;
    snprintf(line, sizeof line, "%s%02X\r\n", a, c);
    nmea_feed(&n, line, 20);
    nmea_feed(&n, line + 20, (int)strlen(line) - 20);
    NEAR(n.sats, 7, 0);

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
