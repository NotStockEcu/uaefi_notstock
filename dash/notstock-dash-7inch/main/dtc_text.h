/* What a trouble code means, in English or Czech (cs != 0), upper case.
 * ESP-free; the round gauge uses the same file. */
#pragma once
#include <stdint.h>

/* the generic meaning, NULL when not in the list */
const char *dtc_text(uint16_t code, int cs);
/* the code's group (fuel and air, emissions, the maker's code, ...) */
const char *dtc_group(uint16_t code, int cs);
/* why an ECU refused to clear, from its negative response code */
const char *dtc_nrc_text(uint8_t nrc, int cs);
