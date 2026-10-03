/* CAN / OBD-II for the round gauge, see can_obd.c */
#pragma once
#include "ui_round.h"

void can_obd_start(void);             /* TWAI up, the OBD client running */
void can_obd_fill(rnd_data_t *d);     /* what it read, for ui_round_update */
