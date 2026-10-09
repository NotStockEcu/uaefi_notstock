/* CAN on the OBD port: the 7" dash's OBD-II client (obd2.c: mode 01 PIDs, the
 * VW measuring values over UDS, the particulate filter, trouble codes) on
 * TWAI, and the bridge from what it reads to the round gauge's rnd_data_t.
 *
 * obd2.c writes into g_dash like on the 7"; it is defined here, with NAN
 * for what has not been read, which the gauge shows as "--".
 */
#include "can_obd.h"
/* the board: the 2.1" LCD here, the others from their own projects
 * (../notstock-round-amoled, ../notstock-round-lcd185) */
#if defined(BOARD_A132)
#include "board_a132.h"
#elif defined(BOARD_AMOLED)
#include "board_amoled.h"
#elif defined(BOARD_LCD185)
#include "board_lcd185.h"
#else
#include "board_round.h"
#endif
#include "obd2.h"
#include "rusefi_can.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
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

static uint32_t s_rx, s_rx_ecu, s_tx_ok, s_tx_fail;   /* for the log */

/* obd2.c sends through here */
bool obd_send(uint32_t id, const uint8_t d[8])
{
    twai_message_t m = { .identifier = id, .data_length_code = 8 };
    memcpy(m.data, d, 8);
    bool ok = twai_transmit(&m, 0) == ESP_OK;
    if (ok) s_tx_ok++;
    else s_tx_fail++;
    return ok;
}

/* every few seconds: what the bus does. RX pin at 0 all the time, or
 * nothing received and the TX error count climbing to 128 (error passive):
 * the transceiver's wiring (CTX / CRX swapped, no 3V3) or CAN-H / CAN-L. */
static void diag_log(void)
{
    static int64_t next;
    int64_t now = esp_timer_get_time();
    if (now < next) return;
    next = now + 3000000;
    twai_status_info_t st = { 0 };
    twai_get_status_info(&st);
    static const char *const STATE[] = { "stopped", "running", "bus-off", "recovering" };
    ESP_LOGI(TAG, "rx %lu (ECU answers %lu), tx ok %lu fail %lu, %s, "
             "tx err %lu rx err %lu, bus errors %lu, rx pin %d",
             (unsigned long)s_rx, (unsigned long)s_rx_ecu,
             (unsigned long)s_tx_ok, (unsigned long)s_tx_fail,
             st.state <= TWAI_STATE_RECOVERING ? STATE[st.state] : "?",
             (unsigned long)st.tx_error_counter, (unsigned long)st.rx_error_counter,
             (unsigned long)st.bus_error_count, gpio_get_level(PIN_TWAI_RX));
}

/* Sniffing: another tester on the bus (VCDS, OBDeleven) asking the engine
 * on 0x7E0. Its requests, and the engine's answers on 0x7E8 while it asks,
 * go to the log ("sniff"), and the gauge holds its own requests back so
 * the two do not talk over each other. Read a value in the tester, find
 * its request (22 xx xx: the DID) and the answer (62 xx xx ...) here. */
#define SNIFF_HOLD_US 2000000
static int64_t s_foreign_until;

static void sniff(const twai_message_t *m, int64_t now)
{
    bool req = m->identifier == 0x7E0;
    if (req) s_foreign_until = now + SNIFF_HOLD_US;
    else if (!(m->identifier == 0x7E8 && now < s_foreign_until)) return;
    char hex[3 * 8 + 1];
    int n = m->data_length_code > 8 ? 8 : m->data_length_code;
    for (int i = 0; i < n; i++) snprintf(hex + 3 * i, 4, "%02X ", m->data[i]);
    hex[n ? 3 * n - 1 : 0] = 0;
    ESP_LOGI(TAG, "sniff %03lX %s %s", (unsigned long)m->identifier,
             req ? ">" : "<", hex);
}

static void can_task(void *arg)
{
    (void)arg;
    twai_message_t msg;
    obd_reset();
    while (1) {
        /* short wait: the loop also paces the OBD requests */
        int64_t now = esp_timer_get_time();
        if (twai_receive(&msg, pdMS_TO_TICKS(5)) == ESP_OK && !msg.rtr) {
            s_rx++;
            if (msg.identifier >= 0x7E8 && msg.identifier <= 0x7EF) s_rx_ecu++;
            now = esp_timer_get_time();
            sniff(&msg, now);
            obd_frame(msg.identifier, msg.data, msg.data_length_code, now);
        }
        /* quiet while another tester talks to the engine */
        if (now >= s_foreign_until) obd_tick(esp_timer_get_time());
        diag_log();

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
