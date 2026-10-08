/* NMEA 0183 for the GPS: RMC (fix, speed, course) and GGA (satellites,
 * altitude), any talker (GP, GN, GL, ...). Bytes in as they come, whole
 * sentences checked against their checksum. ESP-free. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool  fix;            /* RMC status A */
    float speed_kmh;      /* RMC, NAN before the first */
    float course_deg;     /* RMC, true course over ground, NAN: none */
    int   sats;           /* GGA, satellites used, -1 before the first */
    float alt_m;          /* GGA, NAN before the first */
    uint32_t sentences;   /* good sentences so far */
    uint32_t rmc_count;   /* RMCs so far: a new one = a new speed */
} nmea_t;

void nmea_init(nmea_t *n);
void nmea_feed(nmea_t *n, const char *buf, int len);
