#include "settings.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "set";
#define NS  "dash"
#define KEY "cfg"
#define VER 5     /* bump whenever settings_t changes */

settings_t g_set;

void settings_defaults(void)
{
    g_set = (settings_t){
        .rpm_flash       = 7000,
        .clt_warn        = 105,
        .iat_warn        = 60,
        .boost_warn      = 120,     /* 1.20 bar */
        .afr_lean_warn   = 160,     /* 16.0 */
        .flash_enable    = true,
        .flash_intensity = 80,
        .flash_area      = FLASH_AREA_SCREEN,
        .flash_colour    = FLASH_COLOUR_RED,
        .flash_period    = 200,     /* 5 Hz */
        .brightness      = 100,
        .night           = false,
        .night_level     = 55,
        .stoich          = 147,
        .baro            = 100,
        .demo            = false,
        .log_mask        = 0x13,    /* RPM, MAP, AFR */
        .look            = 0,       /* NOTSTOCK */
    };
}

void settings_load(void)
{
    settings_defaults();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs unavailable, using defaults");
        return;
    }

    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no saved settings, using defaults");
        return;
    }

    uint8_t ver = 0;
    size_t len = sizeof(settings_t);
    settings_t tmp;
    if (nvs_get_u8(h, "ver", &ver) == ESP_OK && ver == VER &&
        nvs_get_blob(h, KEY, &tmp, &len) == ESP_OK &&
        len == sizeof(settings_t)) {
        g_set = tmp;
        ESP_LOGI(TAG, "settings loaded");
    } else {
        /* a firmware update changed the struct: keep the defaults rather
         * than reinterpreting old bytes as new fields */
        ESP_LOGW(TAG, "stored settings are stale, using defaults");
    }
    nvs_close(h);
}

void settings_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "cannot open nvs for writing");
        return;
    }
    nvs_set_u8(h, "ver", VER);
    nvs_set_blob(h, KEY, &g_set, sizeof(settings_t));
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "settings saved");
}
