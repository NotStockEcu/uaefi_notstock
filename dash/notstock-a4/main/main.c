/* NOT STOCK A4 gauge: the Audi A4 B8 2.0 TDI over OBD-II, on the Waveshare
 * ESP32-S3-Touch-AMOLED-1.32. The NOT STOCK logo, the needle sweep, then
 * the dials (ui_a4.c) fed by the round gauge's OBD client (can_obd.c).
 *
 * Build: idf.py set-target esp32s3 && idf.py build flash monitor
 * Flash and monitor over the board's USB-C; CAN on the 12-pin header,
 * GPIO1 (TX) and GPIO2 (RX), see board_a132.h.
 */
#include "board_a132.h"
#include "can_obd.h"
#include "hw.h"
#include "ui_a4.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "a4";

#define UPDATE_MS   33
#define NS          "a4"
#define SAVE_MS     3000        /* a page kept this long is remembered */
#define VOLUME      70          /* % */

/* the round gauge's settings, which its shared code (motion.c, sounds) may
 * read; this gauge keeps its own in NVS "a4" */
rnd_settings_t g_rnd_set;

void a4_backlight(uint8_t percent)
{
    hw_backlight(percent);
}

void a4_regen_sound(bool start)
{
    /* beeps on the speaker: three at the start, one at the end */
    hw_volume(VOLUME);
    hw_beep(start ? 3 : 1);
}

void a4_flip(void)
{
    hw_flip_begin();
}

/* a4_settings_t changed: back to defaults. 2: the DPF limit's default went
 * from 24 to 22.29 g; a version 1 store is kept but takes that. */
#define SET_VER 2

static void settings_load(void)
{
    a4_settings_defaults();
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return;
    uint8_t ver = 0;
    a4_settings_t tmp;
    size_t len = sizeof tmp;
    if (nvs_get_u8(h, "ver", &ver) == ESP_OK && (ver == SET_VER || ver == 1) &&
        nvs_get_blob(h, "set", &tmp, &len) == ESP_OK && len == sizeof tmp) {
        bool ok = true, seen[A4_PAGES] = { false };
        for (int i = 0; i < A4_PAGES; i++) {
            if (tmp.order[i] >= A4_PAGES || seen[tmp.order[i]]) ok = false;
            else seen[tmp.order[i]] = true;
            if (!(tmp.warn[i] >= A4_LIMIT[i].lo && tmp.warn[i] <= A4_LIMIT[i].hi)) ok = false;
        }
        if ((tmp.hidden & 0x3F) == 0x3F) ok = false;
        if (ok && ver == 1) tmp.warn[A4_DPF] = A4_LIMIT[A4_DPF].def;
        if (ok) g_a4_set = tmp;
    }
    nvs_close(h);
}

void a4_settings_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "ver", SET_VER);
    nvs_set_blob(h, "set", &g_a4_set, sizeof g_a4_set);
    nvs_commit(h);
    nvs_close(h);
}

static int page_load(void)
{
    nvs_handle_t h;
    uint8_t p = 0xFF;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, "page", &p);
        nvs_close(h);
    }
    return p;
}

static void page_save(int p)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "page", (uint8_t)p);
    nvs_commit(h);
    nvs_close(h);
}

static void update_cb(lv_timer_t *t)
{
    (void)t;
    static rnd_data_t d;
    static int saved = -1, seen = -1;
    static int64_t seen_at;
    can_obd_fill(&d);
    ui_a4_update(&d);

    int p = ui_a4_current();
    int64_t now = esp_timer_get_time();
    if (p != seen) {
        seen = p;
        seen_at = now;
    } else if (p != saved && now - seen_at > SAVE_MS * 1000LL) {
        page_save(p);
        saved = p;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "NOT STOCK A4 gauge, OBD-II, %s", BOARD_NAME);
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    hw_init();
    can_obd_start();

    /* the dial first, then the NOT STOCK logo in front of it, out of black
     * and into it (hw_boot, straight into the frame buffer); then the
     * needle sweeps to full scale and back, as the cluster does */
    settings_load();
    ui_a4_create();
    int p = page_load();             /* the dial last looked at, if shown */
    if (p < A4_PAGES && !(g_a4_set.hidden >> p & 1)) ui_a4_page(p);
    lv_timer_create(update_cb, UPDATE_MS, NULL);
    extern const lv_img_dsc_t *const boot_logo[];
    hw_boot(boot_logo[0]);
    hw_backlight(100);
    ui_a4_sweep();

    while (1) {
        uint32_t next = lv_timer_handler();
        hw_flip_end();               /* a changed screen: out in one go */
        if (next == LV_NO_TIMER_READY || next > 20) next = 20;
        if (next < 2) next = 2;
        vTaskDelay(pdMS_TO_TICKS(next));
    }
}
