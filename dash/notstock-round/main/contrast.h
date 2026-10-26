/* HIGH CONTRAST (LOOK -> HIGH CONTRAST), for a panel in daylight: every
 * pixel on its way to the panel, its brightness (the brightest of R, G, B)
 * lifted on a curve that takes the mid tones up (grey print near white, a
 * dark red zone to a clear one) and the darkest to black, its colour kept
 * (all three channels by the same factor). Header only, platform-free:
 * hc_init() once, then hc565() per pixel, RGB565 in native order. */
#pragma once
#include <math.h>
#include <stdint.h>

static uint16_t hc_gain[64];      /* Q8 factor, by brightness 0..63 */

static inline void hc_init(void)
{
    for (int l = 1; l < 64; l++) {
        float v = l / 63.0f;
        float o = 1.6f * powf(v, 0.8f) - 0.1f;
        o = o < 0 ? 0 : o > 1 ? 1 : o;
        hc_gain[l] = (uint16_t)lroundf(o / v * 256.0f);
    }
}

static inline uint16_t hc565(uint16_t c)
{
    uint32_t r = c >> 11, g = (c >> 5) & 63, b = c & 31;
    uint32_t r6 = r << 1 | r >> 4, b6 = b << 1 | b >> 4;
    uint32_t l = r6 > g ? r6 : g;
    if (b6 > l) l = b6;
    uint32_t k = hc_gain[l];
    r6 = (r6 * k) >> 8;
    g = (g * k) >> 8;
    b6 = (b6 * k) >> 8;
    if (r6 > 63) r6 = 63;
    if (g > 63) g = 63;
    if (b6 > 63) b6 = 63;
    return (uint16_t)((r6 >> 1) << 11 | g << 5 | b6 >> 1);
}
