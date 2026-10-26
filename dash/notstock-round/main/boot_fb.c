/* The boot logo drawn straight into the panel's frame buffer, see boot_fb.h.
 * Going through LVGL redrew the whole screen for every step of the fade,
 * which the board does at a few frames a second: the fade stuttered. Here a
 * step is one pass over the pixels.
 */
#include "boot_fb.h"

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

int boot_fb_in_level(uint32_t ms)
{
    if (ms >= RND_BOOT_IN_MS) return 32;
    /* ease-in: slow out of the dark, like it is being lit */
    uint32_t t = ms * 256 / RND_BOOT_IN_MS;
    return (int)(t * t * 32 / (256 * 256));
}

int boot_fb_x_level(uint32_t ms)
{
    if (ms >= RND_BOOT_X_MS) return 32;
    uint32_t t = ms * 256 / RND_BOOT_X_MS;
    uint32_t s = (3 * 256 - 2 * t) * t * t / (256 * 256);   /* smoothstep */
    return (int)(s * 32 / 256);
}

#define LOGO_W ((int)logo->header.w)
#define LOGO_H ((int)logo->header.h)
#define LOGO_X ((RND_W - LOGO_W) / 2)
#define LOGO_Y ((RND_H - LOGO_H) / 2)

void boot_fb_logo(uint16_t *fb, const lv_img_dsc_t *logo, int level)
{
    const uint16_t *src = (const uint16_t *)logo->data;
    for (int y = 0; y < LOGO_H; y++) {
        uint16_t *d = fb + (LOGO_Y + y) * RND_W + LOGO_X;
        const uint16_t *s = src + y * LOGO_W;
        if (level >= 32) {
            for (int x = 0; x < LOGO_W; x++) d[x] = s[x];
        } else {
            for (int x = 0; x < LOGO_W; x++) d[x] = scale(s[x], (uint32_t)level);
        }
    }
}

void boot_fb_cross(uint16_t *fb, const lv_img_dsc_t *logo,
                   const uint16_t *gauge, int level)
{
    const uint16_t *src = (const uint16_t *)logo->data;
    for (int y = 0; y < RND_H; y++) {
        uint16_t *d = fb + y * RND_W;
        const uint16_t *g = gauge + y * RND_W;
        int ly = y - LOGO_Y;
        if (ly < 0 || ly >= LOGO_H) {
            for (int x = 0; x < RND_W; x++) d[x] = scale(g[x], (uint32_t)level);
            continue;
        }
        const uint16_t *s = src + ly * LOGO_W;
        for (int x = 0; x < LOGO_X; x++) d[x] = scale(g[x], (uint32_t)level);
        for (int x = 0; x < LOGO_W; x++) {
            d[LOGO_X + x] = mix(s[x], g[LOGO_X + x], (uint32_t)level);
        }
        for (int x = LOGO_X + LOGO_W; x < RND_W; x++) {
            d[x] = scale(g[x], (uint32_t)level);
        }
    }
}
