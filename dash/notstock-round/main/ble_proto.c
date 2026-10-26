/* Bluetooth frames, see ble_proto.h. */
#include "ble_proto.h"

#include <math.h>

static void put(uint8_t *p, int v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

/* scaled, rounded, clamped into int16; NAN or no link: BLE_NONE */
static int s16(float v, float scale, bool link)
{
    if (!link || isnan(v)) return (uint16_t)BLE_NONE;
    float x = roundf(v * scale);
    if (x > 32767) x = 32767;
    if (x < -32767) x = -32767;
    return (int)x;
}

/* the same for values that are never negative, up to 65534 */
static int u16(float v, float scale, bool link)
{
    if (!link || isnan(v)) return (uint16_t)BLE_NONE;
    float x = roundf(v * scale);
    if (x < 0) x = 0;
    if (x > 65534) x = 65534;
    if (x == 0x8000) x = 0x8001;      /* keep clear of "none" */
    return (int)x;
}

size_t ble_pack_gauges(uint8_t out[BLE_GAUGES_LEN], const rnd_data_t *d,
                       uint16_t seq, uint8_t flags)
{
    bool l = d->link;
    out[0] = 1;
    out[1] = flags | (l ? BLE_F_LINK : 0);
    put(out + 2, seq);
    put(out + 4, s16(d->v[RND_WATER], 10, l));
    put(out + 6, s16(d->v[RND_OIL], 10, l));
    put(out + 8, s16(d->v[RND_BOOST], 100, l));
    put(out + 10, s16(d->v[RND_INTAKE], 10, l));
    put(out + 12, s16(d->v[RND_EXHAUST], 1, l));
    put(out + 14, u16(d->v[RND_RPM], 1, l));
    return BLE_GAUGES_LEN;
}

size_t ble_pack_dpf(uint8_t out[BLE_DPF_LEN], const rnd_data_t *d,
                    uint8_t flags, uint16_t regens)
{
    bool l = d->link;
    out[0] = 1;
    out[1] = flags | (l ? BLE_F_LINK : 0);
    put(out + 2, s16(d->dpf.soot_g, 100, l));
    put(out + 4, s16(d->dpf.soot_meas_g, 100, l));
    put(out + 6, s16(d->dpf.dp_hpa, 1, l));
    put(out + 8, u16(d->dpf.dist_km, 10, l));
    put(out + 10, s16(d->dpf.temp_c, 1, l));
    put(out + 12, regens);
    return BLE_DPF_LEN;
}
