/* OBD-II mode 01 client, for cars that do not broadcast what the dash needs.
 *
 * Built for a VW T5.1 (2.0 TDI CR, CAAC, EDC17) on the OBD port: diagnostic
 * CAN on pins 6 (H) / 14 (L), 500 kbit, 11-bit IDs. The dash asks, the
 * engine ECU answers: requests go to 0x7DF (functional, used for the scan)
 * or 0x7E0 (engine, used for polling), answers come from 0x7E8.
 *
 * First the ECU is asked which PIDs it supports (01 00, 01 20, ...), then
 * the supported ones among those the dash wants are polled round robin, one
 * request in flight at a time. Answers longer than one frame (PID 78, EGT)
 * are reassembled with ISO-TP flow control.
 *
 * Decoded values go into g_dash like the rusEFI decoder's, so the LOG screen
 * and the link state work unchanged.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define OBD_REQ_FUNC   0x7DF      /* all emission ECUs */
#define OBD_REQ_ENGINE 0x7E0      /* engine ECU, physical */
#define OBD_RESP_BASE  0x7E8      /* answers come from 0x7E8..0x7EF */

typedef enum {
    OBD_IDLE,          /* protocol not selected */
    OBD_SCANNING,      /* asking which PIDs are supported */
    OBD_POLLING,       /* reading values */
    OBD_NO_ECU,        /* nobody answered; the scan is retried */
} obd_state_t;

typedef struct {
    obd_state_t state;
    uint32_t supported[8];        /* PIDs 0x01..0x100, see obd_supported */
    uint16_t ecu_id;              /* ID the engine answers on, 0 until seen */
    uint32_t tx, rx, timeouts, negative;
    uint8_t  last_pid;
    float    baro_kpa;            /* from PID 33, 0 until read */
    float    egt[4];              /* PID 78 sensors, NAN when absent */
} obd_status_t;

extern volatile obd_status_t g_obd;

/* true when the scan found this PID supported */
bool obd_supported(uint8_t pid);

/* Called by the CAN task: every received frame, and on every loop turn to
 * send the next request when one is due. obd_reset starts over (protocol
 * switched, or the scan is retried). */
void obd_reset(void);
void obd_frame(uint32_t id, const uint8_t *d, int len, int64_t now_us);
void obd_tick(int64_t now_us);
