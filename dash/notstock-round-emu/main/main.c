/* NOT STOCK round gauge, ECUMaster EMU Black. The Waveshare
 * ESP32-S3-Touch-LCD-2.1 of ../notstock-round (its hw.c, boot logo and
 * fonts), reading the EMU's CAN stream instead of OBD-II: one page, BOOST,
 * AFR and CLT.
 *
 * Build: idf.py set-target esp32s3 && idf.py build flash monitor
 * Flash and monitor over the board's "UART" USB-C; CAN on GPIO19/20 as on
 * the OBD gauge.
 */
#include "board_round.h"
#include "emu_can.h"
#include "hw.h"
#include "ui_emu.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs_flash.h"

static const char *TAG = "emu-gauge";

#define UPDATE_MS 33

static void update_cb(lv_timer_t *t)
{
    (void)t;
    static emu_values_t v;
    emu_view_t view;
    emu_can_get(&v);
    emu_view_from(&v, esp_timer_get_time(), &view);
    ui_emu_update(&view);
}

void app_main(void)
{
    ESP_LOGI(TAG, "NOT STOCK round gauge, EMU Black stream, %s", BOARD_NAME);
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    hw_init();
    hw_backlight(100);
    emu_can_start();

    /* the page first, then the NOT STOCK logo in front of it, out of black
     * and into it (hw_boot, straight into the frame buffer) */
    ui_emu_create();
    lv_timer_create(update_cb, UPDATE_MS, NULL);
    extern const lv_img_dsc_t *const boot_logo[];
    hw_boot(boot_logo[0]);

    while (1) {
        uint32_t next = lv_timer_handler();
        if (next == LV_NO_TIMER_READY || next > 20) next = 20;
        if (next < 2) next = 2;
        vTaskDelay(pdMS_TO_TICKS(next));
    }
}
