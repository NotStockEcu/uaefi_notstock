/* CAN on the OBD port: the 7" dash's OBD-II client (obd2.c: mode 01 PIDs, the
 * VW measuring values over UDS, the particulate filter, trouble codes) on
 * TWAI, and the bridge from what it reads to the round gauge's rnd_data_t.
 *
 * obd2.c writes into g_dash like on the 7"; it is defined here, with NAN
 * for what has not been read, which the gauge shows as "--".
 */
#include "can_obd.h"
/* the board: the 2.1" LCD here, the 1.75" AMOLED from its own project
 * (../notstock-round-amoled, which defines BOARD_AMOLED) */
#ifdef BOARD_AMOLED
#include "board_amoled.h"
#else
#include "board_round.h"
#endif
#include "obd2.h"
#include "rusefi_can.h"

#include <math.h>
#include <string.h>

#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "can";

_Static_assert(RND_DTC_MAX == OBD_DTC_MAX, "trouble code lists differ");

volatile dash_data_t g_dash = {
    .rpm = NAN, .speed = NAN, .boost = NAN, .map = NAN,
    .clt = NAN, .iat = NAN, .oilt = NAN, .egt = NAN,
};

/* obd2.c sends through here */
bool obd_send(uint32_t id, const uint8_t d[8])
{
    twai_message_t m = { .identifier = id, .data_length_code = 8 };
    memcpy(m.data, d, 8);
    return twai_transmit(&m, 0) == ESP_OK;
}

static void can_task(void *arg)
{
    (void)arg;
    twai_message_t msg;
    obd_reset();
    while (1) {
        /* short wait: the loop also paces the OBD requests */
        if (twai_receive(&msg, pdMS_TO_TICKS(5)) == ESP_OK && !msg.rtr) {
            obd_frame(msg.identifier, msg.data, msg.data_length_code,
                      esp_timer_get_time());
        }
        obd_tick(esp_timer_get_time());

        /* a stuck bus latches the controller into bus-off: recover, so the
         * gauge comes back by itself after a wiring glitch */
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

void can_obd_start(void)
{
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        PIN_TWAI_TX, PIN_TWAI_RX, TWAI_MODE_NORMAL);
    g.rx_queue_len = 32;
    g.tx_queue_len = 4;
    twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_ERROR_CHECK(twai_driver_install(&g, &t, &f));
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "TWAI up on tx %d rx %d, 500 kbit, OBD-II", PIN_TWAI_TX,
             PIN_TWAI_RX);
    xTaskCreatePinnedToCore(can_task, "can", 4096, NULL, 6, NULL, 0);
}

/* ------------------------------------------------------- to the gauge */
void can_obd_fill(rnd_data_t *d)
{
    int64_t rx = g_dash.last_rx_us;
    d->link = rx != 0 && esp_timer_get_time() - rx < LINK_TIMEOUT_US;

    d->v[RND_WATER] = g_dash.clt;
    d->v[RND_OIL] = g_dash.oilt;
    d->v[RND_BOOST] = g_dash.boost;
    d->v[RND_INTAKE] = g_dash.iat;
    d->v[RND_EXHAUST] = g_dash.egt;
    d->v[RND_RPM] = g_dash.rpm;

    d->dpf.soot_g = g_obd.dpf.soot_g;
    d->dpf.soot_meas_g = g_obd.dpf.soot_meas_g;
    d->dpf.dp_hpa = g_obd.dpf.dp_hpa;
    d->dpf.dist_km = g_obd.dpf.dist_km;
    d->dpf.temp_c = g_obd.dpf.temp_c;

    /* the codes: the list only when obd2.c is not writing it */
    rnd_dtc_status_t *s = &d->dtc;
    s->busy = g_obd.dtc.busy;
    s->result = g_obd.dtc.result;
    s->nrc = g_obd.dtc.nrc;
    s->seq = g_obd.dtc.seq;
    if (!s->busy) {
        s->n = g_obd.dtc.n;
        s->more = g_obd.dtc.more;
        for (int i = 0; i < s->n && i < RND_DTC_MAX; i++) {
            s->list[i].code = g_obd.dtc.list[i].code;
            s->list[i].ecu = g_obd.dtc.list[i].ecu;
            s->list[i].kind = g_obd.dtc.list[i].kind;
        }
    }
}

void rnd_dtc_read(void)
{
    obd_dtc_read();
}

void rnd_dtc_clear(void)
{
    obd_dtc_clear();
}
