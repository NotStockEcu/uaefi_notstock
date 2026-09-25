#pragma once
#include "lvgl.h"

/* Bump on every flashed build. Shown bottom right of the settings screen next
 * to the build date, so a panel in the car can be identified without a
 * laptop. */
#define DASH_VERSION "2.0"

void ui_menu_create(void);
void ui_menu_refresh(void);
lv_obj_t *ui_menu_screen(void);
