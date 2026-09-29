/* CAN sniffer, to find the VW measuring values OBD-II does not give.
 *
 * The dash only listens (it still acknowledges frames, it never sends one)
 * while VCDS reads values from the car through the same OBD port (Y cable).
 * Every frame is printed to the serial console for idf.py monitor, and the
 * UDS "read data by identifier" answers (service 0x22 -> 0x62) are collected
 * per ECU and DID for the SNIFF screen: what VCDS shows as oil or exhaust
 * temperature changes in step with one of those rows, and that DID is what
 * the dash then asks for itself.
 *
 * No ESP-IDF in here, like obd2.c, so the sim can feed it.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SNIFF_IDS   20
#define SNIFF_DIDS  14
#define SNIFF_DATA  12

typedef struct {
    uint16_t ecu;                 /* ID the answer came from */
    uint16_t did;
    uint8_t  len;                 /* data bytes after the DID, all of them */
    uint8_t  data[SNIFF_DATA];    /* the first SNIFF_DATA of those */
    uint32_t count;
    int64_t  last_us;
} sniff_did_t;

typedef struct {
    uint32_t frames;
    uint16_t id[SNIFF_IDS];       /* every CAN ID seen, first come */
    uint32_t id_count[SNIFF_IDS];
    int      n_id;
    sniff_did_t did[SNIFF_DIDS];  /* the least recently updated is reused */
    int      n_did;
    uint32_t uds_req, uds_neg;    /* 0x22 requests, negative answers */
} sniff_status_t;

extern volatile sniff_status_t g_sniff;
extern bool sniff_echo;           /* print every frame (the panel: yes) */

void sniff_reset(void);
void sniff_frame(uint32_t id, const uint8_t *d, int len, int64_t now_us);
