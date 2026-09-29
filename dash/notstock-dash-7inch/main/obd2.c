/* OBD-II mode 01 client, see obd2.h.
 *
 * No ESP-IDF in here: frames go out through obd_send(), which rusefi_can.c
 * implements with TWAI and tools/sim with a fake ECU, so the protocol logic
 * is exercised on the PC too.
 */
#include "obd2.h"
#include "rusefi_can.h"

#include <math.h>
#include <string.h>

/* provided by the CAN layer (rusefi_can.c, or tools/sim) */
bool obd_send(uint32_t id, const uint8_t d[8]);

#define REPLY_TIMEOUT_US   150000   /* ECUs answer mode 01 within ~50 ms */
#define GAP_US             15000    /* pause between requests */
#define RESCAN_US          2000000  /* after the ECU went silent */
#define GIVE_UP_TIMEOUTS   6        /* in a row, before NO_ECU */
#define BARO_EVERY         40       /* baro changes slowly: every 40th poll */

volatile obd_status_t g_obd;

/* What the dash wants, in polling order. 0x0B is skipped when 0x87 (the
 * wide-range MAP) is there: plain 0x0B stops at 255 kPa, which a TDI's boost
 * passes. */
static const uint8_t WANT[] = {
    0x0C,   /* rpm */
    0x05,   /* coolant */
    0x87,   /* MAP, wide range */
    0x0B,   /* MAP, 0..255 kPa */
    0x0F,   /* intake air */
    0x0C,   /* rpm again: it is the liveliest proof the link is up */
    0x5C,   /* oil temperature */
    0x78,   /* EGT bank 1 */
    0x0D,   /* speed */
    0x33,   /* barometric pressure (only every BARO_EVERY) */
};
#define N_WANT (sizeof WANT / sizeof WANT[0])

static int64_t s_deadline;        /* reply due by, 0 when nothing in flight */
static int64_t s_next;            /* earliest time for the next request */
static uint8_t s_pending;         /* PID in flight */
static int s_scan_page;           /* 0..7 while scanning: PID 0x00 + 0x20*page */
static int s_poll;                /* index into WANT */
static int s_polls;
static int s_silent;              /* timeouts in a row */

/* ISO-TP reassembly for multi-frame answers */
static uint8_t s_buf[40];
static int s_len, s_have, s_next_sn;

static inline void set_supported_word(int page, uint32_t bits)
{
    g_obd.supported[page] = bits;
}

bool obd_supported(uint8_t pid)
{
    if (pid == 0) return true;
    int i = (pid - 1) / 32;
    int b = 31 - (pid - 1) % 32;
    return (g_obd.supported[i] >> b) & 1;
}

void obd_reset(void)
{
    obd_status_t z;
    memset(&z, 0, sizeof z);
    z.state = OBD_SCANNING;
    for (int i = 0; i < 4; i++) z.egt[i] = NAN;
    memcpy((void *)&g_obd, &z, sizeof z);
    s_deadline = 0;
    s_next = 0;
    s_scan_page = 0;
    s_poll = 0;
    s_polls = 0;
    s_silent = 0;
    s_len = s_have = 0;
}

static void request(uint32_t id, uint8_t pid, int64_t now)
{
    /* single frame: length 2, mode 01, PID, padded like VW testers do */
    const uint8_t d[8] = { 0x02, 0x01, pid, 0x55, 0x55, 0x55, 0x55, 0x55 };
    if (!obd_send(id, d)) {
        s_next = now + GAP_US;
        return;
    }
    g_obd.tx++;
    g_obd.last_pid = pid;
    s_pending = pid;
    s_deadline = now + REPLY_TIMEOUT_US;
    s_len = s_have = 0;
}

static bool want_now(uint8_t pid)
{
    if (!obd_supported(pid)) return false;
    if (pid == 0x0B && obd_supported(0x87)) return false;
    if (pid == 0x33 && g_obd.baro_kpa > 0 && s_polls % BARO_EVERY) return false;
    return true;
}

static void update_boost(void)
{
    float baro = g_obd.baro_kpa > 0 ? g_obd.baro_kpa : 101.3f;
    g_dash.boost = (g_dash.map - baro) / 100.0f;
}

/* One complete mode 01 answer: d[0] = 0x41, d[1] = PID, data after. */
static void answer(const uint8_t *d, int n, int64_t now)
{
    if (n < 2 || d[0] != 0x41) return;
    uint8_t pid = d[1];
    const uint8_t *a = d + 2;
    int na = n - 2;

    if (pid % 0x20 == 0 && pid <= 0xE0 && na >= 4) {
        uint32_t bits = ((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16) |
                        ((uint32_t)a[2] << 8) | a[3];
        set_supported_word(pid / 0x20, bits);
    } else {
        switch (pid) {
        case 0x05: if (na >= 1) g_dash.clt = a[0] - 40.0f; break;
        case 0x0F: if (na >= 1) g_dash.iat = a[0] - 40.0f; break;
        case 0x5C: if (na >= 1) g_dash.oilt = a[0] - 40.0f; break;
        case 0x0D: if (na >= 1) g_dash.speed = a[0]; break;
        case 0x0C:
            if (na >= 2) g_dash.rpm = ((a[0] << 8) | a[1]) / 4.0f;
            break;
        case 0x0B:
            if (na >= 1) { g_dash.map = a[0]; update_boost(); }
            break;
        case 0x87:
            /* A: which sensors; B,C: sensor A in 1/32 kPa */
            if (na >= 3 && (a[0] & 0x01)) {
                g_dash.map = ((a[1] << 8) | a[2]) / 32.0f;
                update_boost();
            }
            break;
        case 0x33:
            if (na >= 1) { g_obd.baro_kpa = a[0]; update_boost(); }
            break;
        case 0x78: {
            /* A: which of 4 sensors; then 2 bytes each, 0.1 degC - 40 */
            float hottest = NAN;
            for (int i = 0; i < 4; i++) {
                float v = NAN;
                if ((a[0] >> i) & 1 && na >= 3 + 2 * i) {
                    v = ((a[1 + 2 * i] << 8) | a[2 + 2 * i]) / 10.0f - 40.0f;
                    if (isnan(hottest) || v > hottest) hottest = v;
                }
                g_obd.egt[i] = v;
            }
            g_dash.egt = hottest;
            break;
        }
        default:
            break;
        }
    }

    g_obd.rx++;
    g_dash.last_rx_us = now;
    s_silent = 0;
    if (pid == s_pending) {
        s_deadline = 0;
        s_next = now + GAP_US;
    }
}

void obd_frame(uint32_t id, const uint8_t *d, int len, int64_t now_us)
{
    if (g_obd.state == OBD_IDLE) return;
    if (id < OBD_RESP_BASE || id > OBD_RESP_BASE + 7 || len < 2) return;
    /* The engine answers on the lowest ID (0x7E8). During the scan another
     * ECU (gearbox, 0x7E9) may beat it to the first answer; move down to the
     * lower ID as soon as it shows up, it overwrites what the other said. */
    if (g_obd.ecu_id == 0 ||
        (g_obd.state == OBD_SCANNING && id < g_obd.ecu_id)) {
        g_obd.ecu_id = (uint16_t)id;
    }
    if (id != g_obd.ecu_id) return;

    uint8_t type = d[0] >> 4;
    if (type == 0) {                               /* single frame */
        int n = d[0] & 0x0F;
        if (n > len - 1) n = len - 1;
        if (n >= 3 && d[1] == 0x7F) {              /* negative response */
            g_obd.negative++;
            s_deadline = 0;
            s_next = now_us + GAP_US;
            return;
        }
        answer(d + 1, n, now_us);
    } else if (type == 1) {                        /* first frame */
        s_len = ((d[0] & 0x0F) << 8) | d[1];
        if (s_len > (int)sizeof s_buf) s_len = sizeof s_buf;
        s_have = len - 2;
        if (s_have > s_len) s_have = s_len;
        memcpy(s_buf, d + 2, s_have);
        s_next_sn = 1;
        /* flow control: go ahead, no block limit, no gap */
        const uint8_t fc[8] = { 0x30, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55, 0x55 };
        obd_send(id - 8, fc);
        s_deadline = now_us + REPLY_TIMEOUT_US;
    } else if (type == 2 && s_len > 0) {           /* consecutive frame */
        if ((d[0] & 0x0F) != (s_next_sn & 0x0F)) { s_len = 0; return; }
        s_next_sn++;
        int n = len - 1;
        if (n > s_len - s_have) n = s_len - s_have;
        memcpy(s_buf + s_have, d + 1, n);
        s_have += n;
        if (s_have >= s_len) {
            answer(s_buf, s_len, now_us);
            s_len = 0;
        }
    }
}

void obd_tick(int64_t now)
{
    if (g_obd.state == OBD_IDLE) return;

    if (s_deadline) {
        if (now < s_deadline) return;
        /* no answer in time */
        s_deadline = 0;
        g_obd.timeouts++;
        s_silent++;
        if (g_obd.state == OBD_SCANNING && s_scan_page > 0) {
            s_scan_page--;           /* ask for the same page again */
        }
        s_next = now + GAP_US;
        if (s_silent >= GIVE_UP_TIMEOUTS) {
            g_obd.state = OBD_NO_ECU;
            s_next = now + RESCAN_US;
        }
    }
    if (now < s_next) return;

    if (g_obd.state == OBD_NO_ECU) {
        uint32_t tx = g_obd.tx, rx = g_obd.rx, to = g_obd.timeouts;
        obd_reset();
        g_obd.tx = tx;               /* keep the counters across rescans */
        g_obd.rx = rx;
        g_obd.timeouts = to;
    }

    if (g_obd.state == OBD_SCANNING) {
        /* page 0 always; the next page only if the last bit of the previous
         * one says it exists */
        if (s_scan_page > 0 &&
            (s_scan_page >= 8 || !(g_obd.supported[s_scan_page - 1] & 1))) {
            g_obd.state = OBD_POLLING;
        } else {
            request(g_obd.ecu_id ? g_obd.ecu_id - 8u : OBD_REQ_FUNC,
                    (uint8_t)(s_scan_page * 0x20), now);
            s_scan_page++;
            return;
        }
    }

    /* polling: next wanted PID that is supported */
    for (int tries = 0; tries < (int)N_WANT; tries++) {
        uint8_t pid = WANT[s_poll];
        s_poll = (s_poll + 1) % N_WANT;
        if (s_poll == 0) s_polls++;
        if (want_now(pid)) {
            request(g_obd.ecu_id ? g_obd.ecu_id - 8u : OBD_REQ_ENGINE, pid, now);
            return;
        }
    }
    s_next = now + GAP_US * 10;     /* nothing useful supported */
}
