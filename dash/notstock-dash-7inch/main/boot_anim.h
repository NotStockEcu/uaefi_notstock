/* Boot screen: the logo rises out of black, holds, then crossfades into the
 * dash.
 *
 * This runs outside LVGL on purpose. Blending a 440x440 image, and then the
 * whole 800x480 screen, through LVGL's 40-line draw buffer every frame was
 * far too slow on the ESP32 and the fade stuttered. These functions write
 * straight into an RGB565 frame buffer with a 32-level integer blend, which
 * keeps up with the panel.
 *
 * No ESP-IDF dependencies, so tools/sim renders the same frames.
 */
#pragma once
#include <stdint.h>

#define BOOT_IN_MS    800      /* logo rises out of black */
#define BOOT_HOLD_MS  1200     /* logo at full brightness */
#define BOOT_FADE_MS  700      /* crossfade into the dash */

#define BOOT_W 800
#define BOOT_H 480

/* 0..32 brightness of the logo at ms after start, eased in. */
int boot_in_level(uint32_t ms);

/* 0..32 share of the dash at ms into the crossfade, eased in and out. */
int boot_fade_level(uint32_t ms);

/* Logo at the given brightness, centred. Only the logo square is written;
 * the rest of fb is expected to be black already. */
void boot_draw_logo(uint16_t *fb, int level);

/* Logo on black mixed with a full dash frame: 0 = logo only, 32 = dash
 * only. Writes every pixel of fb. */
void boot_draw_cross(uint16_t *fb, const uint16_t *dash, int level);
