#include "boot_anim.h"
#include "ui.h"

/* RGB565 blend with 32 levels. Spreading the pixel to 0x07E0F81F puts green
 * in the top half-word with room between the fields, so one multiply blends
 * all three channels at once. */
static inline uint32_t spread(uint16_t c)
{
    return ((uint32_t)c | ((uint32_t)c << 16)) & 0x07E0F81Fu;
}

static inline uint16_t pack(uint32_t x)
{
    return (uint16_t)(x | (x >> 16));
}

static inline uint16_t mix(uint16_t a, uint16_t b, uint32_t level)
{
    uint32_t xa = spread(a), xb = spread(b);
    return pack((xa + (((xb - xa) * level) >> 5)) & 0x07E0F81Fu);
}

static inline uint16_t scale(uint16_t c, uint32_t level)
{
    return pack(((spread(c) * level) >> 5) & 0x07E0F81Fu);
}

int boot_in_level(uint32_t ms)
{
    if (ms >= BOOT_IN_MS) return 32;
    /* ease-in: slow out of the dark, then up to full */
    uint32_t t = ms * 256 / BOOT_IN_MS;            /* 0..255 */
    return (int)(t * t * 32 / (256 * 256));
}

int boot_fade_level(uint32_t ms)
{
    if (ms >= BOOT_FADE_MS) return 32;
    uint32_t t = ms * 256 / BOOT_FADE_MS;          /* 0..255 */
    /* smoothstep: 3t^2 - 2t^3 */
    uint32_t s = (3 * 256 - 2 * t) * t * t / (256 * 256);
    return (int)(s * 32 / 256);
}

#define LOGO_W  ((int)splash_logo.header.w)
#define LOGO_H  ((int)splash_logo.header.h)
#define LOGO_X  ((BOOT_W - LOGO_W) / 2)
#define LOGO_Y  ((BOOT_H - LOGO_H) / 2)

void boot_draw_logo(uint16_t *fb, int level)
{
    const uint16_t *src = (const uint16_t *)splash_logo.data;
    for (int y = 0; y < LOGO_H; y++) {
        uint16_t *d = fb + (LOGO_Y + y) * BOOT_W + LOGO_X;
        const uint16_t *s = src + y * LOGO_W;
        if (level >= 32) {
            for (int x = 0; x < LOGO_W; x++) d[x] = s[x];
        } else {
            for (int x = 0; x < LOGO_W; x++) d[x] = scale(s[x], level);
        }
    }
}

void boot_draw_cross(uint16_t *fb, const uint16_t *dash, int level)
{
    const uint16_t *src = (const uint16_t *)splash_logo.data;
    for (int y = 0; y < BOOT_H; y++) {
        uint16_t *d = fb + y * BOOT_W;
        const uint16_t *b = dash + y * BOOT_W;
        int ly = y - LOGO_Y;
        if (ly < 0 || ly >= LOGO_H) {
            for (int x = 0; x < BOOT_W; x++) d[x] = scale(b[x], level);
            continue;
        }
        const uint16_t *s = src + ly * LOGO_W;
        for (int x = 0; x < LOGO_X; x++) d[x] = scale(b[x], level);
        for (int x = 0; x < LOGO_W; x++)
            d[LOGO_X + x] = mix(s[x], b[LOGO_X + x], level);
        for (int x = LOGO_X + LOGO_W; x < BOOT_W; x++)
            d[x] = scale(b[x], level);
    }
}
