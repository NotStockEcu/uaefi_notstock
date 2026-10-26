/* The car's diagnostic CAN on the RP2350 (can2040, PIO0), 500 kbit, and the
 * OBD-II / VW UDS client of the 7" dash (../notstock-dash-7inch/main/obd2.c)
 * on it, see can_t5.c.
 * The SN65HVD230 board on the 4-pin UART connector: 3V3, GND, CTX to
 * GPIO0, CRX to GPIO1 (the GPIO numbers count, not the TXD / RXD labels).
 * Build with -DT5_CAN_SWAP=1 for CTX on GPIO1, CRX on GPIO0. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifndef T5_CAN_SWAP
#define T5_CAN_SWAP 0
#endif
#define T5_PIN_CAN_TX (T5_CAN_SWAP ? 1 : 0)
#define T5_PIN_CAN_RX (T5_CAN_SWAP ? 0 : 1)
#define T5_CAN_BITRATE 500000

void can_t5_start(void);
/* from the main loop, every few ms: the frames received go to the OBD
 * client, and it sends its next request */
void can_t5_poll(void);
