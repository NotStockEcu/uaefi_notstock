/* EMU Black CAN stream on the RP2350 (can2040, PIO0), see can_l2.c.
 * The SN65HVD230 board: CTX to GPIO2 (header pin 7), CRX to GPIO3
 * (header pin 9), 3V3 (pin 1), GND (pin 2). */
#pragma once
#include "emu_stream.h"

#define L2_PIN_CAN_TX 2
#define L2_PIN_CAN_RX 3

void can_l2_start(void);              /* starts looking for the bit rate */
void can_l2_poll(void);               /* from the main loop, every few ms */
void can_l2_get(emu_values_t *out);   /* the latest values, a copy */
int  can_l2_kbit(void);               /* bit rate found, 0: still looking */
