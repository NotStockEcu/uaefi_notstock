/* LVGL 8.4 for the RP2350 gauge: what differs from LVGL's defaults
 * (lv_conf_internal.h fills in the rest). */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   0        /* 16-bit SPI frames: no swap needed */
#define LV_MEM_SIZE        (64U * 1024U)

#define LV_TICK_CUSTOM     1
#define LV_TICK_CUSTOM_INCLUDE "pico/time.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (to_ms_since_boot(get_absolute_time()))

#define LV_DISP_DEF_REFR_PERIOD  20
#define LV_INDEV_DEF_READ_PERIOD 20

#define LV_USE_LOG         0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_PERF_MONITOR 0

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#endif
