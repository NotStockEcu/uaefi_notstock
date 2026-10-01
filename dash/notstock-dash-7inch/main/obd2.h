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
 *
 * Trouble codes, on request (obd_dtc_read / obd_dtc_clear): polling pauses,
 * mode 03 (stored codes) and then 07 (pending: seen once, not confirmed
 * yet) go to 0x7DF, and every ECU that answers within DTC_WINDOW is
 * collected, multi-frame answers included. Clearing is mode 04 to 0x7DF,
 * then the codes are read again. ECUs refuse 04 with the engine running
 * (7F 04 22): ignition on, engine off. Mode 04 also resets the readiness
 * monitors and freeze frames, as with any OBD tester.
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

/* trouble codes */
#define OBD_DTC_MAX 24
enum { DTC_STORED = 1, DTC_PENDING = 2 };            /* obd_dtc_t.kind bits */
typedef struct {
    uint16_t code;        /* SAE J2012: 2 bits letter, 14 bits number */
    uint8_t  ecu;         /* answered on 0x7E8 + ecu */
    uint8_t  kind;        /* DTC_STORED | DTC_PENDING */
} obd_dtc_t;
typedef enum { DTC_IDLE, DTC_READING, DTC_CLEARING } obd_dtc_busy_t;
typedef enum {
    DTC_NOT_READ,         /* nothing asked yet */
    DTC_READ,             /* list[] is what the ECUs said */
    DTC_NO_ANSWER,        /* nobody answered */
    DTC_CLEARED,          /* cleared, list[] read again after */
    DTC_CLEAR_REFUSED,    /* an ECU said no, nrc says why */
} obd_dtc_result_t;

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
    struct {                      /* kept over obd_reset */
        uint8_t  busy;            /* obd_dtc_busy_t */
        uint8_t  result;          /* obd_dtc_result_t */
        uint8_t  nrc;             /* negative response code of a refusal */
        uint8_t  ecus;            /* bit per ECU (0x7E8 + bit) that answered */
        uint8_t  n;               /* codes in list */
        uint8_t  more;            /* there were more than OBD_DTC_MAX */
        uint16_t seq;             /* counts finished reads, for the UI */
        obd_dtc_t list[OBD_DTC_MAX];
    } dtc;
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

/* From the UI: read the trouble codes, or clear them (and read again).
 * Ignored while one is running; g_obd.dtc says how it went. */
void obd_dtc_read(void);
void obd_dtc_clear(void);

/* "P0401" from a code */
void obd_dtc_name(uint16_t code, char out[6]);
