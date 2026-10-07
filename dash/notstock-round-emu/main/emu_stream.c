/* EMU Black CAN stream decoder, see emu_stream.h. Per frame (byte: value,
 * scaling), all little-endian:
 *   base+0  0-1 RPM  2 TPS 0.5 %  3 IAT int8 degC  4-5 MAP kPa
 *           6-7 injector pulse 0.016129 ms
 *   base+1  analog inputs 1-4, 0.0048828125 V
 *   base+2  0-1 speed km/h  2 BARO kPa  3 oil temp degC  4 oil pressure
 *           0.0625 bar  5 fuel pressure 0.0625 bar  6-7 CLT int16 degC
 *   base+3  0 ignition int8 0.5 deg  1 dwell 0.05 ms  2 lambda 1/128
 *           3 lambda correction 0.5 %  4-5 EGT1 degC  6-7 EGT2 degC
 *   base+4  0 gear  1 ECU temp int8  2-3 battery 0.027 V  4-5 error flags
 *           6 flags1  7 ethanol %
 *   base+5  DBW, traction control
 *   base+6  analog inputs 5-6, output flags 1-4 (4: bit 1 coolant fan)
 *   base+7  0-1 boost target kPa  2 PWM1 %  3 DSG mode ...
 */
#include "emu_stream.h"

#include <math.h>
#include <string.h>

static inline unsigned u16(const uint8_t *d, int i)
{
    return (unsigned)d[i] | ((unsigned)d[i + 1] << 8);
}

static inline int s16(const uint8_t *d, int i)
{
    return (int16_t)u16(d, i);
}

void emu_init(emu_values_t *v)
{
    memset(v, 0, sizeof *v);
    v->rpm = v->tps = v->iat = v->map_kpa = NAN;
    v->baro_kpa = v->oilt = v->oilp = v->fuelp = v->clt = NAN;
    v->lambda = v->egt1 = v->egt2 = v->batt = v->ecu_temp = NAN;
    v->boost_target_kpa = NAN;
}

bool emu_decode(emu_values_t *v, uint32_t base, uint32_t id,
                const uint8_t *d, int len, int64_t now_us)
{
    if (id < base || id >= base + EMU_FRAMES) return false;
    int n = (int)(id - base);
    /* base+7 was 4 bytes before firmware 1.43; the rest are 8 */
    if (len < (n == 7 ? 4 : 8)) return false;

    switch (n) {
    case 0:
        v->rpm = (float)u16(d, 0);
        v->tps = d[2] * 0.5f;
        v->iat = (float)(int8_t)d[3];
        v->map_kpa = (float)u16(d, 4);
        break;
    case 2:
        v->baro_kpa = (float)d[2];
        v->oilt = (float)d[3];
        v->oilp = d[4] * 0.0625f;
        v->fuelp = d[5] * 0.0625f;
        v->clt = (float)s16(d, 6);
        break;
    case 3:
        v->lambda = d[2] / 128.0f;
        v->egt1 = (float)u16(d, 4);
        v->egt2 = (float)u16(d, 6);
        break;
    case 4:
        v->ecu_temp = (float)(int8_t)d[1];
        v->batt = u16(d, 2) * 0.027f;
        v->err = (uint16_t)u16(d, 4);
        v->flags1 = d[6];
        break;
    case 6:
        for (int i = 0; i < 4; i++) v->outflags[i] = d[4 + i];
        break;
    case 7:
        v->boost_target_kpa = (float)u16(d, 0);
        break;
    default:
        break;      /* 1, 5: nothing the gauge shows (yet) */
    }
    v->frame_us[n] = now_us;
    return true;
}
