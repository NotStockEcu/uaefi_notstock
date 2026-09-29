/* CAN sniffer, see sniff.h. */
#include "sniff.h"

#include <stdio.h>
#include <string.h>

volatile sniff_status_t g_sniff;
bool sniff_echo;

/* ISO-TP reassembly, one slot per sending ID */
#define SLOTS 8
static struct {
    uint16_t id;
    int len, have, sn;
    uint8_t buf[64];
} s_tp[SLOTS];
static int s_ntp;

void sniff_reset(void)
{
    memset((void *)&g_sniff, 0, sizeof g_sniff);
    memset(s_tp, 0, sizeof s_tp);
    s_ntp = 0;
}

static void count_id(uint32_t id)
{
    for (int i = 0; i < g_sniff.n_id; i++) {
        if (g_sniff.id[i] == id) { g_sniff.id_count[i]++; return; }
    }
    if (g_sniff.n_id < SNIFF_IDS) {
        g_sniff.id[g_sniff.n_id] = (uint16_t)id;
        g_sniff.id_count[g_sniff.n_id] = 1;
        g_sniff.n_id++;
    }
}

static void positive(uint16_t ecu, const uint8_t *m, int n, int64_t now)
{
    if (n < 3) return;
    uint16_t did = (uint16_t)(m[1] << 8 | m[2]);
    volatile sniff_did_t *e = NULL;
    for (int i = 0; i < g_sniff.n_did; i++) {
        if (g_sniff.did[i].ecu == ecu && g_sniff.did[i].did == did) {
            e = &g_sniff.did[i];
            break;
        }
    }
    if (!e) {
        if (g_sniff.n_did < SNIFF_DIDS) {
            e = &g_sniff.did[g_sniff.n_did++];
        } else {
            e = &g_sniff.did[0];
            for (int i = 1; i < SNIFF_DIDS; i++) {
                if (g_sniff.did[i].last_us < e->last_us) e = &g_sniff.did[i];
            }
        }
        e->ecu = ecu;
        e->did = did;
        e->count = 0;
    }
    int k = n - 3;
    e->len = (uint8_t)k;
    if (k > SNIFF_DATA) k = SNIFF_DATA;
    for (int i = 0; i < k; i++) e->data[i] = m[3 + i];
    e->count++;
    e->last_us = now;
}

/* one complete UDS message from ID */
static void message(uint32_t id, const uint8_t *m, int n, int64_t now)
{
    if (n < 1) return;
    if (m[0] == 0x22) g_sniff.uds_req++;
    else if (m[0] == 0x62) positive((uint16_t)id, m, n, now);
    else if (m[0] == 0x7F && n >= 2 && m[1] == 0x22) g_sniff.uds_neg++;
}

static int slot(uint32_t id)
{
    for (int i = 0; i < s_ntp; i++) if (s_tp[i].id == id) return i;
    if (s_ntp < SLOTS) { s_tp[s_ntp].id = (uint16_t)id; return s_ntp++; }
    return -1;
}

void sniff_frame(uint32_t id, const uint8_t *d, int len, int64_t now)
{
    g_sniff.frames++;
    count_id(id);

    if (sniff_echo) {
        char line[64];
        int n = snprintf(line, sizeof line, "SNF %lld.%03d %03X %d",
                         (long long)(now / 1000000),
                         (int)(now / 1000 % 1000), (unsigned)id, len);
        for (int i = 0; i < len && i < 8; i++) {
            n += snprintf(line + n, sizeof line - n, " %02X", d[i]);
        }
        puts(line);
    }

    /* diagnostic IDs only: ISO-TP frames of 8 bytes */
    if (id < 0x600 || id > 0x7FF || len < 2) return;
    int s = slot(id);
    if (s < 0) return;
    uint8_t type = d[0] >> 4;
    if (type == 0) {
        int n = d[0] & 0x0F;
        if (n > len - 1) n = len - 1;
        message(id, d + 1, n, now);
    } else if (type == 1) {
        int n = ((d[0] & 0x0F) << 8) | d[1];
        if (n > (int)sizeof s_tp[s].buf) n = sizeof s_tp[s].buf;
        int have = len - 2;
        if (have > n) have = n;
        memcpy(s_tp[s].buf, d + 2, have);
        s_tp[s].len = n;
        s_tp[s].have = have;
        s_tp[s].sn = 1;
    } else if (type == 2 && s_tp[s].len > 0) {
        if ((d[0] & 0x0F) != (s_tp[s].sn & 0x0F)) { s_tp[s].len = 0; return; }
        s_tp[s].sn++;
        int n = len - 1;
        if (n > s_tp[s].len - s_tp[s].have) n = s_tp[s].len - s_tp[s].have;
        memcpy(s_tp[s].buf + s_tp[s].have, d + 1, n);
        s_tp[s].have += n;
        if (s_tp[s].have >= s_tp[s].len) {
            message(id, s_tp[s].buf, s_tp[s].len, now);
            s_tp[s].len = 0;
        }
    }
}
