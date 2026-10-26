/* The 1.85" gauge's way to its Wi-Fi updater (../../notstock-updater185,
 * in the factory partition; the gauge runs from ota_0): AKTUALIZACE in the
 * menu sets "upd" in NVS and boots the updater. */
#include "ui_round.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "nvs.h"

static const char *TAG = "ota";

static const esp_partition_t *updater(void)
{
    const esp_partition_t *p = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    esp_app_desc_t d;
    return p && esp_ota_get_partition_description(p, &d) == ESP_OK ? p : NULL;
}

bool rnd_update_available(void)
{
    return updater() != NULL;
}

void rnd_update_start(void)
{
    const esp_partition_t *p = updater();
    if (!p) return;
    nvs_handle_t h;
    if (nvs_open("ota", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "upd", 1);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "to the updater");
    esp_ota_set_boot_partition(p);
    esp_restart();
}
