/* TWAI for the EMU Black CAN stream, see emu_can.c */
#pragma once
#include "emu_stream.h"

void emu_can_start(void);             /* finds the bit rate, then listens */
void emu_can_get(emu_values_t *out);  /* the latest values, a copy */
int  emu_can_kbit(void);              /* bit rate found, 0: still looking */
