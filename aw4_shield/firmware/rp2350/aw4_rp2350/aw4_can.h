// CAN z rusEFI (broadcast „CAN verbose“): příjem přes can2040 (PIO), dekódování podle rusEFI_CAN_verbose.dbc.
// rusEFI: CAN broadcast zapnutý, základní adresa 512 (0x200), 11bit ID, 500 kbit/s (nastavitelné níže).
//   base+1 (0x201): otáčky (u16), rychlost km/h (u8, bajt 6)
//   base+2 (0x202): pedál, TPS1 (i16 ×0,01 %)
//   base+3 (0x203): MAP (u16 /30 kPa), teplota chladicí kapaliny (u8 −40 °C)
//   base+4 (0x204): napětí (u16 ×0,001 V)
//   base+11 (0x20B): bit 0 = brzdový pedál
// Knihovna can2040 (Kevin O'Connor, GPLv3) je přiložená ve složce sketche (can2040.c/.h, can2040_COPYING.txt).
#pragma once
#include <Arduino.h>

struct EcuData {
  uint16_t rpm = 0;
  uint8_t kph = 0;
  float tps = 0, pedal = 0, map = 0, clt = 0, vbat = 0;
  bool brake = false;
  uint32_t lastMs = 0;     // čas poslední přijaté zprávy z jednotky
  uint32_t frames = 0;
} ecu;

const uint32_t CAN_BITRATE = 500000;
const uint32_t RUSEFI_BASE = 0x200;
const uint32_t ECU_TIMEOUT_MS = 500;      // bez zpráv déle než 0,5 s = data z jednotky neplatná
const uint8_t PIN_CAN_TX = 1, PIN_CAN_RX = 0;

static inline uint16_t le16(const uint8_t *d) { return d[0] | (d[1] << 8); }
static inline int16_t les16(const uint8_t *d) { return (int16_t)le16(d); }

// Dekóduje jednu zprávu rusEFI (volá se mimo přerušení).
inline void ecuDecode(uint32_t id, const uint8_t *d, uint8_t dlc, uint32_t nowMs) {
  if (id < RUSEFI_BASE || id > RUSEFI_BASE + 11 || dlc < 1) return;
  switch (id - RUSEFI_BASE) {
    case 1: if (dlc >= 7) { ecu.rpm = le16(d); ecu.kph = d[6]; } break;
    case 2: if (dlc >= 4) { ecu.pedal = les16(d) * 0.01f; ecu.tps = les16(d + 2) * 0.01f; } break;
    case 3: if (dlc >= 3) { ecu.map = le16(d) / 30.0f; ecu.clt = d[2] - 40.0f; } break;
    case 4: if (dlc >= 8) { ecu.vbat = le16(d + 6) * 0.001f; } break;
    case 11: ecu.brake = d[0] & 1; break;
    default: return;
  }
  ecu.lastMs = nowMs; ecu.frames++;
}

inline bool ecuValid(uint32_t nowMs) { return ecu.frames && nowMs - ecu.lastMs < ECU_TIMEOUT_MS; }

#if defined(ARDUINO_ARCH_RP2040)
extern "C" {
#include "can2040.h"
}
#include "hardware/irq.h"
#include "hardware/clocks.h"

static struct can2040 cbus;
// kruhový buffer: přerušení zapisuje, smyčka čte
struct CanRx { uint32_t id; uint8_t dlc; uint8_t data[8]; };
static volatile CanRx canBuf[32];
static volatile uint8_t canHead = 0, canTail = 0;

static void canCb(struct can2040 *, uint32_t notify, struct can2040_msg *msg) {
  if (notify != CAN2040_NOTIFY_RX) return;
  uint8_t n = (canHead + 1) & 31;
  if (n == canTail) return;   // plno – zprávu zahodit
  volatile CanRx &r = canBuf[canHead];
  r.id = msg->id & 0x1FFFFFFF; r.dlc = msg->dlc > 8 ? 8 : msg->dlc;
  for (int i = 0; i < 8; i++) r.data[i] = msg->data[i];
  canHead = n;
}
static void canIrq() { can2040_pio_irq_handler(&cbus); }

inline void canBegin() {
  can2040_setup(&cbus, 1);                       // PIO1 (PIO0 nechat volné pro core)
  can2040_callback_config(&cbus, canCb);
  irq_set_exclusive_handler(PIO1_IRQ_0, canIrq);
  irq_set_priority(PIO1_IRQ_0, 1);
  irq_set_enabled(PIO1_IRQ_0, true);
  can2040_start(&cbus, clock_get_hz(clk_sys), CAN_BITRATE, PIN_CAN_RX, PIN_CAN_TX);
}
inline void canPoll() {
  while (canTail != canHead) {
    CanRx r; volatile CanRx &s = canBuf[canTail];
    r.id = s.id; r.dlc = s.dlc; for (int i = 0; i < 8; i++) r.data[i] = s.data[i];
    canTail = (canTail + 1) & 31;
    ecuDecode(r.id, r.data, r.dlc, millis());
  }
}
#else
inline void canBegin() {}
inline void canPoll() {}
#endif
