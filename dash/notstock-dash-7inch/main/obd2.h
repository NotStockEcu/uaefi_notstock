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
 * What mode 01 does not offer is read as a VW measuring value with UDS
 * "read data by identifier" (22 DID -> 62 DID data), same request and answer
 * IDs. The DIDs were found with SNIFF while VCDS read them from a T5.1 CAAC
 * (EDC17): 11BE engine oil temperature (IDE00196), 10FB exhaust gas
 * temperature sensor 1 (IDE02229), both unsigned 16 bit, 0.1 K. The diesel
 * particulate filter, polled every DPF_EVERY-th round since it moves slowly:
 *   14F5  differential pressure        IDE00427  signed 16 bit, 1 hPa
 *   114F  soot mass, calculated        IDE00434  signed 16 bit, 0.01 g
 *   114E  soot mass, measured          IDE00435  signed 16 bit, 0.01 g
 *   1156  distance since regeneration  IDE00436  unsigned 32 bit, 1 m
 *   1044  simulated filter surface temperature  IDE04653  16 bit, 0.1 K
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

/* VW measuring values read with UDS 22, only when the matching mode 01 PID
 * (5C oil, 78 EGT) is missing */
enum { OBD_UDS_OIL, OBD_UDS_EGT,
       OBD_UDS_DPF_DP, OBD_UDS_DPF_SOOT, OBD_UDS_DPF_SOOT_MEAS,
       OBD_UDS_DPF_DIST, OBD_UDS_DPF_TEMP, OBD_UDS_N };
enum { UDS_UNKNOWN, UDS_OK, UDS_REFUSED };
extern const uint16_t obd_uds_did[OBD_UDS_N];

typedef struct {
    obd_state_t state;
    uint32_t supported[8];        /* PIDs 0x01..0x100, see obd_supported */
    uint16_t ecu_id;              /* ID the engine answers on, 0 until seen */
    uint32_t tx, rx, timeouts, negative;
    uint8_t  last_pid;
    float    baro_kpa;            /* from PID 33, 0 until read */
    float    egt[4];              /* PID 78 sensors, NAN when absent */
    uint8_t  uds[OBD_UDS_N];      /* UDS_*, per VW measuring value */
    struct {                      /* particulate filter, NAN until read */
        float dp_hpa;             /* differential pressure */
        float soot_g;             /* soot mass, calculated */
        float soot_meas_g;        /* soot mass, measured */
        float dist_km;            /* since the last regeneration */
        float temp_c;             /* simulated surface temperature */
    } dpf;
} obd_status_t;

extern volatile obd_status_t g_obd;

/* true when the scan found this PID supported */
bool obd_supported(uint8_t pid);

/* true when VW measuring value i (OBD_UDS_*) is read instead of its PID */
bool obd_uds_used(int i);

/* Called by the CAN task: every received frame, and on every loop turn to
 * send the next request when one is due. obd_reset starts over (protocol
 * switched, or the scan is retried). */
void obd_reset(void);
void obd_frame(uint32_t id, const uint8_t *d, int len, int64_t now_us);
void obd_tick(int64_t now_us);
