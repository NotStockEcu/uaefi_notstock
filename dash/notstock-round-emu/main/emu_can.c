/* The EMU Black's CAN stream on TWAI.
 *
 * The bit rate is whatever the EMU is set to (125 k, 250 k, 500 k or 1 M):
 * the gauge looks for it in listen-only mode, where a wrong rate costs the
 * bus nothing, one rate after the other until stream frames come
 * (base..base+7). Then it switches to normal mode at that rate, so it
 * acknowledges the frames even when it is the EMU's only partner on the
 * bus. It never sends anything. Silence for SEARCH_AGAIN_US: look again.
 * The rate found is kept in NVS and tried first next time.
 */
#include "emu_can.h"
#include "board_round.h"

#include <string.h>

#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

static const char *TAG = "emu";

#define TRY_MS          400            /* per rate while looking */
#define SEARCH_AGAIN_US (3 * 1000000)

static const int RATES[] = { 1000, 500, 250, 125 };
#define N_RATES (int)(sizeof RATES / sizeof RATES[0])

static emu_values_t s_v;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile int s_kbit;

static twai_timing_config_t timing(int kbit)
{
    switch (kbit) {
    case 125: return (twai_timing_config_t)TWAI_TIMING_CONFIG_125KBITS();
    case 250: return (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
    case 500: return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
    default:  return (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
    }
}

static bool s_up;

static void bring_up(int kbit, twai_mode_t mode)
{
    if (s_up) {
        twai_stop();
        twai_driver_uninstall();
        s_up = false;
    }
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        PIN_TWAI_TX, PIN_TWAI_RX, mode);
    g.rx_queue_len = 32;
    g.tx_queue_len = 0;                 /* nothing is ever sent */
    twai_timing_config_t t = timing(kbit);
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    if (twai_driver_install(&g, &t, &f) == ESP_OK && twai_start() == ESP_OK) {
        s_up = true;
    } else {
        ESP_LOGE(TAG, "TWAI would not start at %d kbit", kbit);
    }
}

static bool is_stream(const twai_message_t *m)
{
    return !m->rtr && !m->extd && m->identifier >= EMU_BASE_ID &&
           m->identifier < EMU_BASE_ID + EMU_FRAMES &&
           m->data_length_code >= 4;
}

static int stored_rate(void)
{
    nvs_handle_t h;
    int32_t k = 0;
    if (nvs_open("emu", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i32(h, "kbit", &k);
        nvs_close(h);
    }
    return k;
}

static void store_rate(int kbit)
{
    nvs_handle_t h;
    if (kbit == stored_rate()) return;
    if (nvs_open("emu", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_i32(h, "kbit", kbit);
        nvs_commit(h);
        nvs_close(h);
    }
}

/* listen at each rate in turn (the stored one first) until stream frames
 * come; returns the rate */
static int search(void)
{
    int first = stored_rate();
    for (;;) {
        for (int i = -1; i < N_RATES; i++) {
            int k = i < 0 ? first : RATES[i];
            if (k == 0 || (i >= 0 && k == first)) continue;
            bring_up(k, TWAI_MODE_LISTEN_ONLY);
            int64_t end = esp_timer_get_time() + TRY_MS * 1000;
            twai_message_t m;
            int hits = 0;
            while (esp_timer_get_time() < end) {
                if (twai_receive(&m, pdMS_TO_TICKS(20)) == ESP_OK &&
                    is_stream(&m) && ++hits >= 3) {
                    return k;
                }
            }
        }
    }
}

static void can_task(void *arg)
{
    (void)arg;
    for (;;) {
        s_kbit = 0;
        int k = search();
        bring_up(k, TWAI_MODE_NORMAL);
        s_kbit = k;
        store_rate(k);
        ESP_LOGI(TAG, "EMU stream at %d kbit, base 0x%03X", k, EMU_BASE_ID);

        int64_t last = esp_timer_get_time();
        while (esp_timer_get_time() - last < SEARCH_AGAIN_US) {
            twai_message_t m;
            if (twai_receive(&m, pdMS_TO_TICKS(50)) == ESP_OK && is_stream(&m)) {
                int64_t now = esp_timer_get_time();
                portENTER_CRITICAL(&s_mux);
                emu_decode(&s_v, EMU_BASE_ID, m.identifier, m.data,
                           m.data_length_code, now);
                portEXIT_CRITICAL(&s_mux);
                last = now;
            }
            twai_status_info_t st;
            if (twai_get_status_info(&st) == ESP_OK &&
                st.state == TWAI_STATE_BUS_OFF) {
                ESP_LOGW(TAG, "bus-off, recovering");
                twai_initiate_recovery();
                vTaskDelay(pdMS_TO_TICKS(200));
                twai_start();
            }
        }
        ESP_LOGW(TAG, "stream silent, looking for it again");
    }
}

void emu_can_start(void)
{
    emu_init(&s_v);
    xTaskCreatePinnedToCore(can_task, "emu", 4096, NULL, 6, NULL, 0);
}

void emu_can_get(emu_values_t *out)
{
    portENTER_CRITICAL(&s_mux);
    memcpy(out, &s_v, sizeof *out);
    portEXIT_CRITICAL(&s_mux);
}

int emu_can_kbit(void)
{
    return s_kbit;
}
