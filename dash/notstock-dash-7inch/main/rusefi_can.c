/* rusEFI 'verbose CAN' broadcast decoder.
 *
 * Frame layout: rusefi/firmware/controllers/can/can_verbose.cpp
 * Scaling:      PACK_ADD_TEMPERATURE 40, PACK_MULT_PRESSURE 30 (kPa),
 *               PACK_MULT_ANGLE 50, PACK_MULT_LAMBDA 10000,
 *               PACK_MULT_VOLTAGE 1000, PACK_MULT_PERCENT 100
 * All frames little-endian, 8 bytes, 11-bit IDs.
 */
#include "rusefi_can.h"
#include "board.h"
#include "settings.h"

#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "can";

volatile dash_data_t g_dash = {
    .boost = -1.0f, .afr = 14.7f, .lambda = 1.0f,
};

#define ADD_T          40
#define MULT_PRESSURE  30.0f
#define MULT_ANGLE     50.0f
#define MULT_LAMBDA    10000.0f
#define MULT_VOLTAGE   1000.0f
#define MULT_PERCENT   100.0f
#define KPA_PER_BAR    100.0f

static inline uint16_t u16(const uint8_t *d, int i)
{
    return (uint16_t)(d[i] | (d[i + 1] << 8));
}

static inline int16_t s16(const uint8_t *d, int i)
{
    return (int16_t)u16(d, i);
}

static void decode(const twai_message_t *m)
{
    const uint8_t *d = m->data;
    if (m->data_length_code < 8 || m->rtr) {
        return;
    }

    if (CAN_ALS_ID && m->identifier == CAN_ALS_ID) {
        g_dash.als = (d[0] & 0x01) != 0;
        g_dash.last_rx_us = esp_timer_get_time();
        return;
    }

    int off = (int)m->identifier - CAN_BASE_ID;

    switch (off) {
    case 0: {   /* Status */
        uint8_t bits = d[4];
        g_dash.revlimit = (bits & 0x01) != 0;
        g_dash.cel      = (bits & 0x08) != 0;
        g_dash.fan      = (bits & 0x40) || (bits & 0x80);   /* fan | fan2 */
        break;
    }
    case 1:     /* Speeds */
        g_dash.rpm     = u16(d, 0);
        g_dash.timing  = s16(d, 2) / MULT_ANGLE;
        g_dash.injduty = d[4] / 2.0f;
        g_dash.speed   = d[6];          /* uint8 kph, tops out at 255 */
        break;

    case 2:     /* PedalAndTps */
        g_dash.tps = s16(d, 2) / MULT_PERCENT;
        break;

    case 3:     /* Sensors1 */
        g_dash.map   = u16(d, 0) / MULT_PRESSURE;
        g_dash.boost = g_dash.map / KPA_PER_BAR - set_baro();
        g_dash.clt   = (float)d[2] - ADD_T;
        g_dash.iat   = (float)d[3] - ADD_T;
        break;

    case 4:     /* Sensors2: two pad bytes first */
        g_dash.oilp = u16(d, 2) / MULT_PRESSURE / KPA_PER_BAR;
        g_dash.oilt = (float)d[4] - ADD_T;
        g_dash.vbat = u16(d, 6) / MULT_VOLTAGE;
        break;

    case 7:     /* Fueling3 */
        g_dash.lambda = u16(d, 0) / MULT_LAMBDA;
        g_dash.afr    = g_dash.lambda * set_stoich();
        g_dash.fuelp  = u16(d, 4) / MULT_PRESSURE / KPA_PER_BAR;
        break;

    default:
        return;     /* not one of ours, do not touch the link timestamp */
    }

    g_dash.last_rx_us = esp_timer_get_time();
}

bool rusefi_can_link_ok(void)
{
    int64_t t = g_dash.last_rx_us;
    return t != 0 && (esp_timer_get_time() - t) < LINK_TIMEOUT_US;
}

static void can_task(void *arg)
{
    twai_message_t msg;
    while (1) {
        if (twai_receive(&msg, pdMS_TO_TICKS(200)) == ESP_OK) {
            decode(&msg);
        }

        /* A stuck bus latches the controller into bus-off. Recover so the
         * dash comes back on its own after a wiring glitch. */
        twai_status_info_t st;
        if (twai_get_status_info(&st) == ESP_OK &&
            st.state == TWAI_STATE_BUS_OFF) {
            ESP_LOGW(TAG, "bus-off, recovering");
            twai_initiate_recovery();
            vTaskDelay(pdMS_TO_TICKS(200));
            twai_start();
        }
    }
}

void rusefi_can_start(void)
{
    /* NORMAL mode, not LISTEN_ONLY: on a two-node bus (ECU + dash) the dash
     * has to acknowledge frames, otherwise rusEFI racks up TX errors and
     * eventually goes bus-off. The dash never queues a transmission. */
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        PIN_TWAI_TX, PIN_TWAI_RX, TWAI_MODE_NORMAL);
    g.rx_queue_len = 32;
    g.tx_queue_len = 0;

#if CAN_BITRATE_500
    twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
#else
    twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();
#endif
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g, &t, &f));
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "TWAI up on tx %d rx %d, base 0x%03X",
             PIN_TWAI_TX, PIN_TWAI_RX, CAN_BASE_ID);

    xTaskCreatePinnedToCore(can_task, "can", 4096, NULL, 6, NULL, 0);
}
