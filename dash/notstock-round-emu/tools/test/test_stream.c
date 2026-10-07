/* Host test of emu_stream.c against frames built by hand from the EMU
 * Black stream table. cc -I../../main test_stream.c ../../main/emu_stream.c -lm */
#include <math.h>
#include <stdio.h>
#include "emu_stream.h"

static int fails;
#define NEAR(a, b) do { if (fabsf((float)(a) - (float)(b)) > 1e-3f) { \
    printf("FAIL %s = %g, want %g\n", #a, (double)(a), (double)(b)); fails++; } } while (0)

int main(void)
{
    emu_values_t v;
    emu_init(&v);
    NEAR(isnan(v.clt), 1);

    /* base+0: 6000 rpm, TPS 100 %, IAT -12, MAP 250 kPa, PW 62 bits */
    uint8_t f0[8] = { 0x70, 0x17, 200, (uint8_t)-12, 250, 0, 62, 0 };
    emu_decode(&v, 0x600, 0x600, f0, 8, 1);
    NEAR(v.rpm, 6000); NEAR(v.tps, 100); NEAR(v.iat, -12); NEAR(v.map_kpa, 250);

    /* base+2: baro 99, oil 110 C, 4.5 bar, fuel 3 bar, CLT -25 */
    uint8_t f2[8] = { 0, 0, 99, 110, 72, 48, 0xE7, 0xFF };
    emu_decode(&v, 0x600, 0x602, f2, 8, 1);
    NEAR(v.baro_kpa, 99); NEAR(v.oilt, 110); NEAR(v.oilp, 4.5f);
    NEAR(v.fuelp, 3); NEAR(v.clt, -25);

    /* base+3: lambda 0.80 = 102.4/128 -> 102 bits, EGT1 850 */
    uint8_t f3[8] = { 20, 60, 102, 200, 0x52, 0x03, 0, 0 };
    emu_decode(&v, 0x600, 0x603, f3, 8, 1);
    NEAR(v.lambda, 102 / 128.0f); NEAR(v.egt1, 850);

    /* base+4: ECU temp -5, battery 500 bits = 13.5 V, ERR_WBO | ERR_MAP */
    uint8_t f4[8] = { 3, (uint8_t)-5, 0xF4, 0x01, 0x0C, 0x00, 0x08, 0 };
    emu_decode(&v, 0x600, 0x604, f4, 8, 1);
    NEAR(v.ecu_temp, -5); NEAR(v.batt, 13.5f);
    NEAR(v.err, EMU_ERR_WBO | EMU_ERR_MAP); NEAR(v.flags1, 8);

    /* another base: 0x600 frames ignored; old 4-byte base+7 accepted */
    NEAR(emu_decode(&v, 0x700, 0x600, f0, 8, 1), 0);
    uint8_t f7[4] = { 0x2C, 0x01, 50, 5 };
    NEAR(emu_decode(&v, 0x600, 0x607, f7, 4, 1), 1);
    NEAR(v.boost_target_kpa, 300);
    NEAR(emu_decode(&v, 0x600, 0x602, f2, 6, 1), 0);   /* too short */

    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
