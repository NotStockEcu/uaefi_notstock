/* Boot logo straight into the frame buffer, see boot_fb.c. RGB565, the
 * panel's size, 32 levels; timings from ui_round.h (RND_BOOT_*). */
#pragma once
#include <stdint.h>
#include "ui_round.h"

int boot_fb_in_level(uint32_t ms);      /* logo out of black: 0..32 */
int boot_fb_x_level(uint32_t ms);       /* logo into the gauge: 0..32 */
void boot_fb_logo(uint16_t *fb, const lv_img_dsc_t *logo, int level);
void boot_fb_cross(uint16_t *fb, const lv_img_dsc_t *logo,
                   const uint16_t *gauge, int level);
