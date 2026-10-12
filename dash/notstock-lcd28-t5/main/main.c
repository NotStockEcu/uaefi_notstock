/* NOT STOCK 2.8" gauge for the VW T5.1 (2.0 TDI CR), on the Waveshare
 * RP2350-Touch-LCD-2.8 (landscape). The NOT STOCK logo fades in and out,
 * then the pages (ui_t5.c) fed by the OBD-II / VW UDS client of the 7" dash
 * over the OBD port's CAN (can_t5.c).
 *
 * Build: see README.md; flash: hold BOOT, plug USB, copy the .uf2 on the
 * drive that shows up. Log: the USB serial port, 115200.
 */
#include <math.h>
#include <stdio.h>

#include "can_t5.h"
#include "hw_l28.h"
#include "lvgl.h"
#include "obd2.h"
#include "pico/stdlib.h"
#include "rusefi_can.h"
#include "ui_t5.h"

#define UPDATE_MS 33
#define LINK_US   1500000          /* the ECU answered within this: live */

LV_IMG_DECLARE(l2_splash);

static void update_cb(lv_timer_t *t)
{
    (void)t;
    t5_view_t v;
    int64_t now = (int64_t)time_us_64();
    v.link = g_dash.last_rx_us != 0 && now - g_dash.last_rx_us < LINK_US;
    v.state = g_obd.state == OBD_NO_ECU ? T5_ST_NO_ECU
            : g_obd.state == OBD_POLLING ? T5_ST_POLLING : T5_ST_SCANNING;
    float n = NAN;
    v.x[T5_BOOST] = v.link ? g_dash.boost : n;
    v.x[T5_EGT]   = v.link ? g_dash.egt : n;
    v.x[T5_OILT]  = v.link ? g_dash.oilt : n;
    v.x[T5_CLT]   = v.link ? g_dash.clt : n;
    v.x[T5_IAT]   = v.link ? g_dash.iat : n;
    v.x[T5_RPM]   = v.link ? g_dash.rpm : n;
    v.x[T5_SPEED] = v.link ? g_dash.speed : n;
    v.x[T5_SOOT]  = v.link ? g_obd.dpf.soot_g : n;
    v.x[T5_DP]    = v.link ? g_obd.dpf.dp_hpa : n;   /* hPa = mbar */
    v.x[T5_KM]    = v.link ? g_obd.dpf.dist_km : n;
    ui_t5_update(&v);
}

static void run_ms(uint32_t ms)
{
    uint32_t end = to_ms_since_boot(get_absolute_time()) + ms;
    while ((int32_t)(end - to_ms_since_boot(get_absolute_time())) > 0) {
        lv_timer_handler();
        can_t5_poll();
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
    printf("NOT STOCK 2.8\" gauge, VW T5.1 OBD, RP2350-Touch-LCD-2.8\n");
    can_t5_start();

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

    ui_t5_create();
    lv_obj_del(logo_scr);
    lv_timer_create(update_cb, UPDATE_MS, NULL);
    lv_refr_now(NULL);
    hw_backlight(100);

    while (1) {
        uint32_t next = lv_timer_handler();
        can_t5_poll();
        if (next > 5) next = 5;          /* the OBD client wants a quick loop */
        if (next < 1) next = 1;
        sleep_ms(next);
    }
}
