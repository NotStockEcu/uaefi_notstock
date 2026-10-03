/* NOT STOCK round gauge on the Waveshare ESP32-S3-Touch-LCD-2.1, OBD-II.
 *
 * Build: idf.py set-target esp32s3 && idf.py build flash monitor
 * Flash and monitor over the board's "UART" USB-C. CAN is on GPIO19/20, the
 * native USB's pins (12-pin D-/D+), so the "USB" USB-C is out of use.
 *
 * Here: the platform side of ui_round.h (settings in NVS, backlight, buzzer,
 * trouble codes) and the loop that feeds the UI 30 times a second.
 */
#include <string.h>

#include "board_round.h"
#include "can_obd.h"
#include "hw.h"
#include "ui_round.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "round";

#define UPDATE_MS 33

/* ------------------------------------------------------------ settings */
#define NS  "round"
#define KEY "set"
#define VER 2        /* bump whenever rnd_settings_t changes */

static void settings_load(void)
{
    rnd_settings_defaults();
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    nvs_handle_t h;
    if (e != ESP_OK || nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no saved settings, defaults");
        return;
    }
    uint8_t ver = 0;
    rnd_settings_t tmp;
    size_t len = sizeof tmp;
    if (nvs_get_u8(h, "ver", &ver) == ESP_OK && ver == VER &&
        nvs_get_blob(h, KEY, &tmp, &len) == ESP_OK && len == sizeof tmp &&
        tmp.look < RND_LOOK_COUNT && tmp.lang < RND_LANG_COUNT) {
        g_rnd_set = tmp;
        ESP_LOGI(TAG, "settings loaded");
    } else {
        /* the struct changed with a firmware update: defaults, rather than
         * old bytes read as new fields */
        ESP_LOGW(TAG, "stored settings stale, defaults");
    }
    nvs_close(h);
}

void rnd_settings_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "ver", VER);
    nvs_set_blob(h, KEY, &g_rnd_set, sizeof g_rnd_set);
    nvs_commit(h);
    nvs_close(h);
}

/* ---------------------------------------------------------- platform */
void rnd_beep(int n)
{
    hw_beep(n);
}

void rnd_backlight(uint8_t percent)
{
    hw_backlight(percent);
}

/* ------------------------------------------------------------- feeding */
static void update_cb(lv_timer_t *t)
{
    (void)t;
    static rnd_data_t d;          /* the trouble code list is kept between */
    can_obd_fill(&d);
    ui_round_update(&d);
}

void app_main(void)
{
    ESP_LOGI(TAG, "NOT STOCK round gauge, %s", BOARD_NAME);
    settings_load();
    hw_init();
    can_obd_start();

    /* the gauges first, then the logo in front of them out of black and
     * into them (hw_boot, straight into the frame buffer; the LVGL fade of
     * ui_round_create(true) is what the PC simulator shows) */
    ui_round_create(false);
    lv_timer_create(update_cb, UPDATE_MS, NULL);
    extern const lv_img_dsc_t *const boot_logo[];
    hw_boot(boot_logo[0]);

    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    ESP_LOGI(TAG, "LVGL heap %d%% used; internal RAM %u B free, PSRAM %u B",
             mon.used_pct,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    while (1) {
        uint32_t next = lv_timer_handler();
        if (next == LV_NO_TIMER_READY || next > 20) next = 20;
        if (next < 2) next = 2;
        vTaskDelay(pdMS_TO_TICKS(next));
    }
}
