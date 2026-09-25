#pragma once
#include "lvgl.h"

LV_FONT_DECLARE(dash_lbl_18);
LV_FONT_DECLARE(dash_lbl_13);
/* Orbitron, see tools/gen_fonts.sh */
LV_FONT_DECLARE(dash_speed_56);
LV_FONT_DECLARE(dash_orb_40);
LV_FONT_DECLARE(dash_orb_18);
LV_FONT_DECLARE(dash_orb_30);
LV_FONT_DECLARE(dash_orb_14);

extern const lv_img_dsc_t ic_water;
extern const lv_img_dsc_t ic_oiltemp;
extern const lv_img_dsc_t ic_oilpress;
extern const lv_img_dsc_t ic_iat;
extern const lv_img_dsc_t ic_fuel;
extern const lv_img_dsc_t ic_fan;
extern const lv_img_dsc_t ic_flame;

/* Pre-rendered gauge artwork and its geometry, see tools/gen_dials.py */
#include "dials.h"
/* artwork of the other looks, see tools/gen_themes.py */
#include "theme_art.h"

/* boot screen logo, see tools/gen_splash.py */
extern const lv_img_dsc_t splash_logo;

/* Builds the dash screen and starts its refresh timer. */
void ui_create(void);

/* Switches back from the settings menu. */
void ui_show_dash(void);

/* Re-reads g_set and pushes it into the live widgets: thresholds,
 * brightness and the alarm flash. Called by the menu on every
 * change so the effect is visible while adjusting. */
void ui_apply_settings(void);
