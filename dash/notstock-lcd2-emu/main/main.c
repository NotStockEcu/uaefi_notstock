/* NOT STOCK 2" gauge, ECUMaster EMU Black, on the Waveshare
 * RP2350-Touch-LCD-2 (landscape). The NOT STOCK logo fades in and out,
 * then the EMU pages (ui_l2.c) fed from the CAN stream (can_l2.c).
 *
 * Build: see README.md; flash: hold BOOT, plug USB, copy the .uf2 on the
 * drive that shows up. Log: the USB serial port, 115200.
 */
#include <stdio.h>

#include "can_l2.h"
#include "hw_l2.h"
#include "lvgl.h"
#include "pico/stdlib.h"
#include "ui_l2.h"

#define UPDATE_MS 33

LV_IMG_DECLARE(l2_splash);

static void update_cb(lv_timer_t *t)
{
    (void)t;
    static emu_values_t v;
    l2_view_t view;
    can_l2_get(&v);
    l2_view_from(&v, (int64_t)time_us_64(), &view);
    ui_l2_update(&view);
}

static void run_ms(uint32_t ms)
{
    uint32_t end = to_ms_since_boot(get_absolute_time()) + ms;
    while ((int32_t)(end - to_ms_since_boot(get_absolute_time())) > 0) {
        lv_timer_handler();
        can_l2_poll();
        sleep_ms(5);
    }
}

static void fade(int from, int to, uint32_t ms)
{
    const int steps = 25;
    for (int i = 0; i <= steps; i++) {
        hw_backlight((uint8_t)(from + (to - from) * i / steps));
        run_ms(ms / steps);
    }
}

int main(void)
{
    stdio_init_all();
    hw_init();
    printf("NOT STOCK 2\" gauge, EMU Black stream, RP2350-Touch-LCD-2\n");
    can_l2_start();

    /* the logo, out of black and into it */
    lv_obj_t *logo_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(logo_scr, lv_color_black(), 0);
    lv_obj_t *logo = lv_img_create(logo_scr);
    lv_img_set_src(logo, &l2_splash);
    lv_obj_center(logo);
    lv_scr_load(logo_scr);
    lv_refr_now(NULL);
    fade(0, 100, 600);
    run_ms(900);
    fade(100, 0, 400);

    ui_l2_create();
    lv_obj_del(logo_scr);
    lv_timer_create(update_cb, UPDATE_MS, NULL);
    lv_refr_now(NULL);
    hw_backlight(100);

    while (1) {
        uint32_t next = lv_timer_handler();
        can_l2_poll();
        if (next > 10) next = 10;
        if (next < 1) next = 1;
        sleep_ms(next);
    }
}
