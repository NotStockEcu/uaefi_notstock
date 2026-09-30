/* What the round gauge sends over Bluetooth LE (docs/BLE.md), packed here
 * without ESP-IDF so the sim and a test can use it; the NimBLE service that
 * notifies it lives with the platform.
 *
 * Service   6e6f7473-746f-636b-0000-000000000000
 *  GAUGES   ...0001  notify, 10 Hz, 16 bytes
 *  DPF      ...0002  notify, 1 Hz, 14 bytes
 *  INFO     ...0003  read, text: "NOTSTOCK round <version>"
 *
 * Both frames: little endian; a value that is not there is 0x8000.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ui_round.h"

#define BLE_SVC_UUID    "6e6f7473-746f-636b-0000-000000000000"
#define BLE_GAUGES_LEN  16
#define BLE_DPF_LEN     14
#define BLE_NONE        ((int16_t)0x8000)

/* flags, byte 1 of both frames */
#define BLE_F_LINK      0x01      /* data from the car */
#define BLE_F_REGEN     0x02      /* particulate filter regenerating */
#define BLE_F_NIGHT     0x04

/* GAUGES: 0 version (1), 1 flags, 2-3 sequence, then int16 each:
 *   4 water  0.1 C     6 oil 0.1 C      8 boost 0.01 bar
 *  10 intake 0.1 C    12 exhaust 1 C   14 rpm 1 (uint16)
 * DPF: 0 version (1), 1 flags, then int16 each:
 *   2 soot 0.01 g      4 soot measured 0.01 g   6 diff pressure 1 hPa
 *   8 since regeneration 0.1 km (uint16)       10 filter temp 1 C
 *  12 regenerations counted since power-up (uint16) */
size_t ble_pack_gauges(uint8_t out[BLE_GAUGES_LEN], const rnd_data_t *d,
                       uint16_t seq, uint8_t flags);
size_t ble_pack_dpf(uint8_t out[BLE_DPF_LEN], const rnd_data_t *d,
                    uint8_t flags, uint16_t regens);
