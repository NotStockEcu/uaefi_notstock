/* host stub: time is driven by sim.c, not by a clock */
#pragma once
#include <stdint.h>
int64_t esp_timer_get_time(void);
