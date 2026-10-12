/* NOT STOCK 2.8" gauge for the VW T5.1 (2.0 TDI CR), on the Waveshare
 * RP2350-Touch-LCD-2.8 (landscape). The NOT STOCK logo fades in and out,
 * then the gauges, DPF status and diagnostics (ui_t5.c) fed by the
 * OBD-II / VW UDS client of the 7" dash over the OBD port's CAN
 * (can_t5.c); beeps on the board's speaker (audio_l28.c).
 *
 * Build: see README.md; flash: hold BOOT, plug USB, copy the .uf2 on the
 * drive that shows up. Log: the USB serial port, 115200.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio_l28.h"
#include "can_t5.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hw_l28.h"
#include "lvgl.h"
#include "obd2.h"
#include "pico/stdlib.h"
#include "rusefi_can.h"
#include "ui_t5.h"

#define UPDATE_MS 33
#define LINK_US   1500000          /* the ECU answered within this: live */

LV_IMG_DECLARE(l2_splash);

/* settings: the flash's last sector, far past the program */
#define SET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

static void settings_load(void)
{
    t5_settings_defaults();
    t5_settings_t tmp;
    const uint8_t *f = (const uint8_t *)(XIP_BASE + SET_OFFSET);
    memcpy(&tmp, f, sizeof tmp);
    if (f[0] == 1) {
        /* version 1: no contrast yet, everything else the same */
        struct {
            uint8_t ver, lang, sound, volume, big, small[T5_SMALL];
            float limit[T5_CH_N];
        } v1;
        memcpy(&v1, f, sizeof v1);
        tmp = g_t5_set;
        tmp.lang = v1.lang;
        tmp.sound = v1.sound;
        tmp.volume = v1.volume;
        tmp.big = v1.big;
        memcpy(tmp.small, v1.small, sizeof tmp.small);
        memcpy(tmp.limit, v1.limit, sizeof tmp.limit);
    }
    if (t5_settings_valid(&tmp)) g_t5_set = tmp;
    else printf("settings: none stored, defaults\n");
}

void t5_settings_save(void)
{
    static uint8_t page[FLASH_PAGE_SIZE];
    _Static_assert(sizeof(t5_settings_t) <= FLASH_PAGE_SIZE, "settings page");
    memset(page, 0xFF, sizeof page);
    memcpy(page, &g_t5_set, sizeof g_t5_set);
    /* no flash reads while it is written: interrupts off (the CAN one is
     * in RAM, but would wait the ~50 ms anyway) */
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(SET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(SET_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(irq);
    printf("settings: saved\n");
}

/* the UI's platform hooks */
void t5_beep(int n)
{
    if (g_t5_set.sound != T5_SND_BEEP) return;
    audio_volume(g_t5_set.volume);
    audio_beep(n);
}
void t5_backlight(uint8_t percent) { hw_backlight(percent); }
void t5_dtc_read(void) { obd_dtc_read(); }
void t5_dtc_clear(void) { obd_dtc_clear(); }

static void update_cb(lv_timer_t *t)
{
    (void)t;
    static t5_view_t v;
    int64_t now = (int64_t)time_us_64();
    v.link = g_dash.last_rx_us != 0 && now - g_dash.last_rx_us < LINK_US;
    v.state = g_obd.state == OBD_NO_ECU ? T5_ST_NO_ECU
            : g_obd.state == OBD_POLLING ? T5_ST_POLLING : T5_ST_SCANNING;
    const float n = NAN;
    bool k = v.link;
    v.x[T5_BOOST]     = k ? g_dash.boost : n;
    v.x[T5_EGT]       = k ? g_dash.egt : n;
    v.x[T5_OILT]      = k ? g_dash.oilt : n;
    v.x[T5_CLT]       = k ? g_dash.clt : n;
    v.x[T5_IAT]       = k ? g_dash.iat : n;
    v.x[T5_RPM]       = k ? g_dash.rpm : n;
    v.x[T5_SPEED]     = k ? g_dash.speed : n;
    v.x[T5_SOOT]      = k ? g_obd.dpf.soot_g : n;
    v.x[T5_SOOT_MEAS] = k ? g_obd.dpf.soot_meas_g : n;
    v.x[T5_DP]        = k ? g_obd.dpf.dp_hpa : n;      /* hPa = mbar */
    v.x[T5_KM]        = k ? g_obd.dpf.dist_km : n;
    v.x[T5_DPF_T]     = k ? g_obd.dpf.temp_c : n;
    v.dtc.busy = g_obd.dtc.busy;
    v.dtc.result = g_obd.dtc.result;
    v.dtc.nrc = g_obd.dtc.nrc;
    v.dtc.n = g_obd.dtc.n > T5_DTC_MAX ? T5_DTC_MAX : g_obd.dtc.n;
    v.dtc.more = g_obd.dtc.more;
    v.dtc.seq = g_obd.dtc.seq;
    for (int i = 0; i < v.dtc.n; i++) {
        v.dtc.list[i].code = g_obd.dtc.list[i].code;
        v.dtc.list[i].ecu = g_obd.dtc.list[i].ecu;
        v.dtc.list[i].kind = g_obd.dtc.list[i].kind;
    }
    ui_t5_update(&v);
}

static void run_ms(uint32_t ms)
{
    uint32_t end = to_ms_since_boot(get_absolute_time()) + ms;
    while ((int32_t)(end - to_ms_since_boot(get_absolute_time())) > 0) {
        lv_timer_handler();
        can_t5_poll();
        audio_poll();
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
    settings_load();
    can_t5_start();
    audio_init();

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
        audio_poll();
        if (next > 5) next = 5;          /* the OBD client wants a quick loop */
        if (next < 1) next = 1;
        sleep_ms(next);
    }
}
