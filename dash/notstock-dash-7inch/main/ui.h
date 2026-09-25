#pragma once
#include "lvgl.h"

LV_FONT_DECLARE(dash_num_64);
LV_FONT_DECLARE(dash_num_46);
LV_FONT_DECLARE(dash_num_28);
LV_FONT_DECLARE(dash_num_15);
LV_FONT_DECLARE(dash_lbl_18);
LV_FONT_DECLARE(dash_lbl_13);

extern const lv_img_dsc_t ic_water;
extern const lv_img_dsc_t ic_oiltemp;
extern const lv_img_dsc_t ic_oilpress;
extern const lv_img_dsc_t ic_iat;
extern const lv_img_dsc_t ic_fuel;
extern const lv_img_dsc_t ic_fan;
extern const lv_img_dsc_t ic_flame;

/* Pre-rendered gauge artwork and its geometry, see tools/gen_dials.py */
#include "dials.h"

/* NOT STOCK wordmark, traced from the mockup, see tools/gen_assets.py */
extern const lv_img_dsc_t logo_notstock;

/* Builds the dash screen and starts its refresh timer. */
void ui_create(void);

/* Switches back from the settings menu. */
void ui_show_dash(void);

/* Re-reads g_set and pushes it into the live widgets: thresholds,
 * brightness and the alarm flash. Called by the menu on every
 * change so the effect is visible while adjusting. */
void ui_apply_settings(void);
