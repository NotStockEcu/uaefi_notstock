#pragma once
#include <stdbool.h>
#include <stdint.h>

/* ---------------------------------------------------------------- tuning */
#define CAN_BASE_ID     0x200   /* TunerStudio: rusEFI CAN data base address */
#define CAN_BITRATE_500 1       /* 1 = 500 kbit, 0 = 250 kbit */
/* Stoichiometric AFR and the barometric offset are runtime settings now, see
 * settings.h and the on-screen menu. */

/* ALS is not part of the rusEFI verbose broadcast. Send it from a Lua script
 * on a spare ID (byte 0, bit 0) and set this to that ID, or 0 to disable. */
#define CAN_ALS_ID      0x000

#define LINK_TIMEOUT_US 1500000

typedef struct {
    float rpm, speed, boost, map, afr, lambda;
    float clt, iat, oilt, oilp, fuelp, vbat, timing, injduty, tps;
    bool fan, cel, revlimit, als;
    int64_t last_rx_us;
} dash_data_t;

extern volatile dash_data_t g_dash;

void rusefi_can_start(void);
bool rusefi_can_link_ok(void);
