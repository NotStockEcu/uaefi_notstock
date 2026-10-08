/* NMEA parser, see nmea.h */
#include "nmea.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX 100

static char s_line[LINE_MAX];
static int s_len;

void nmea_init(nmea_t *n)
{
    memset(n, 0, sizeof *n);
    n->speed_kmh = n->course_deg = n->alt_m = NAN;
    n->sats = -1;
    s_len = 0;
}

static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* splits at commas in place; empty fields stay as "" */
static int fields(char *s, char *f[], int max)
{
    int n = 0;
    f[n++] = s;
    for (; *s && n < max; s++) {
        if (*s == ',') {
            *s = 0;
            f[n++] = s + 1;
        }
    }
    return n;
}

static void sentence(nmea_t *n, char *s)
{
    /* $ttSSS,...*hh */
    char *star = strrchr(s, '*');
    if (s[0] != '$' || !star || hex(star[1]) < 0 || hex(star[2]) < 0) return;
    uint8_t sum = 0;
    for (char *p = s + 1; p < star; p++) sum ^= (uint8_t)*p;
    if (sum != (uint8_t)(hex(star[1]) * 16 + hex(star[2]))) return;
    *star = 0;
    if (strlen(s) < 6) return;
    const char *type = s + 3;
    char *f[24];
    int nf = fields(s, f, 24);
    n->sentences++;

    if (!strncmp(type, "RMC", 3) && nf >= 9) {
        /* 1 time 2 status 3 lat 4 N 5 lon 6 E 7 knots 8 course */
        n->fix = f[2][0] == 'A';
        n->speed_kmh = f[7][0] ? strtof(f[7], NULL) * 1.852f : NAN;
        n->course_deg = n->fix && f[8][0] ? strtof(f[8], NULL) : NAN;
        if (!n->fix) n->speed_kmh = NAN;
        n->rmc_count++;
    } else if (!strncmp(type, "GGA", 3) && nf >= 10) {
        /* 6 quality 7 satellites 8 hdop 9 altitude */
        n->sats = f[7][0] ? atoi(f[7]) : 0;
        n->alt_m = f[6][0] && f[6][0] != '0' && f[9][0]
                 ? strtof(f[9], NULL) : NAN;
    }
}

void nmea_feed(nmea_t *n, const char *buf, int len)
{
    for (int i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '$') s_len = 0;
        if (c == '\r' || c == '\n' || c == 0) {
            if (s_len > 0) {
                s_line[s_len] = 0;
                sentence(n, s_line);
            }
            s_len = 0;
            continue;
        }
        if (s_len < LINE_MAX - 1) s_line[s_len++] = c;
        else s_len = 0;                  /* too long: not NMEA, drop it */
    }
}
