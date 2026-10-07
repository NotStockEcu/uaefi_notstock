/* ECUMaster EMU Black "CAN stream": the ECU's broadcast, decoded.
 *
 * Eight frames from the base ID (EMU Black: CAN-Bus -> EMU CAN stream,
 * default 0x600), 8 bytes, little-endian, sent by the ECU on its own: the
 * gauge only listens. Layout from ECUMaster's EMU Black help
 * (emuCANStream). 11-bit IDs; the bit rate is set in the EMU (125 k to
 * 1 M), emu_can.c finds it. ESP-free, so tools/sim and tools/test build it on a PC.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define EMU_BASE_ID   0x600
#define EMU_FRAMES    8

/* ERRFLAG bits (base+4, bytes 4-5) */
#define EMU_ERR_CLT   (1u << 0)
#define EMU_ERR_IAT   (1u << 1)
#define EMU_ERR_MAP   (1u << 2)
#define EMU_ERR_WBO   (1u << 3)
#define EMU_ERR_EGT1  (1u << 4)
#define EMU_ERR_EGT2  (1u << 5)
#define EMU_EGT_ALARM (1u << 6)
#define EMU_KNOCKING  (1u << 7)

/* OUTFLAGS4 bits (base+6, byte 7) */
#define EMU_OUT4_FUEL_PUMP (1u << 0)
#define EMU_OUT4_FAN       (1u << 1)     /* coolant fan */

/* live values, NAN until their frame has come */
typedef struct {
    float rpm;          /* base+0 */
    float tps;          /* % */
    float iat;          /* degC */
    float map_kpa;
    float baro_kpa;     /* base+2 */
    float oilt, oilp;   /* degC, bar */
    float fuelp;        /* bar */
    float clt;          /* degC */
    float lambda;       /* base+3 */
    float egt1, egt2;   /* degC */
    float batt;         /* base+4, V */
    float ecu_temp;     /* degC */
    uint16_t err;       /* ERRFLAG, EMU_ERR_* */
    uint8_t flags1;     /* gearcut, ALS, launch, idle, ... */
    uint8_t outflags[4];     /* base+6, bytes 4-7 */
    float boost_target_kpa;  /* base+7 */
    int64_t frame_us[EMU_FRAMES];   /* when each frame last came, 0: never */
} emu_values_t;

void emu_init(emu_values_t *v);
/* one received frame; false: not a stream frame (or too short) */
bool emu_decode(emu_values_t *v, uint32_t base, uint32_t id,
                const uint8_t *d, int len, int64_t now_us);
