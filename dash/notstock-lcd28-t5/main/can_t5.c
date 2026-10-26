/* The diagnostic CAN on the RP2350, see can_t5.h. The RP2350 has no CAN
 * controller: can2040 is one in software, on PIO0, a full node (it sends the
 * OBD requests and acknowledges what it receives). The bit rate is fixed at
 * 500 kbit, the OBD port's: a node at a wrong rate would disturb the bus.
 *
 * Received frames are queued in the PIO interrupt (all of it in RAM, see
 * CMakeLists.txt) and handed to the OBD client from the main loop, so the
 * client and the UI never run at the same time. */
#include "can_t5.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "can2040.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "obd2.h"
#include "pico/stdlib.h"
#include "rusefi_can.h"

#define RING     64                  /* frames between two polls, plenty */
#define LOG_MS   3000

volatile dash_data_t g_dash;         /* what obd2.c decodes into */

static struct can2040 s_cbus;
static struct { uint32_t id; uint8_t len; uint8_t d[8]; } s_ring[RING];
static volatile uint32_t s_head, s_tail;   /* written by the IRQ / the loop */
static volatile uint32_t s_rx_all, s_dropped;
static uint32_t s_tx_ok, s_tx_full;

static void __not_in_flash_func(rx_cb)(struct can2040 *cd, uint32_t notify,
                                       struct can2040_msg *m)
{
    (void)cd;
    if (notify != CAN2040_NOTIFY_RX) return;
    s_rx_all++;
    if (m->id & (CAN2040_ID_RTR | CAN2040_ID_EFF)) return;
    /* the engine's and the other ECUs' answers only */
    if (m->id < 0x7E8 || m->id > 0x7EF) return;
    uint32_t h = s_head;
    if (h - s_tail >= RING) {
        s_dropped++;
        return;
    }
    s_ring[h % RING].id = m->id;
    s_ring[h % RING].len = m->dlc > 8 ? 8 : (uint8_t)m->dlc;
    memcpy(s_ring[h % RING].d, m->data, 8);
    s_head = h + 1;
}

static void __not_in_flash_func(pio_irq)(void)
{
    can2040_pio_irq_handler(&s_cbus);
}

/* obd2.c sends through this */
bool obd_send(uint32_t id, const uint8_t d[8])
{
    struct can2040_msg m = { .id = id, .dlc = 8 };
    memcpy(m.data, d, 8);
    if (can2040_transmit(&s_cbus, &m) == 0) {
        s_tx_ok++;
        return true;
    }
    s_tx_full++;
    return false;
}

void can_t5_start(void)
{
    /* no value until the ECU gives one */
    g_dash.rpm = g_dash.speed = g_dash.boost = g_dash.map = NAN;
    g_dash.clt = g_dash.iat = g_dash.oilt = g_dash.egt = NAN;
    g_dash.last_rx_us = 0;
    obd_reset();

    can2040_setup(&s_cbus, 0);
    can2040_callback_config(&s_cbus, rx_cb);
    irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq);
    irq_set_priority(PIO0_IRQ_0, PICO_HIGHEST_IRQ_PRIORITY);
    irq_set_enabled(PIO0_IRQ_0, true);
    can2040_start(&s_cbus, clock_get_hz(clk_sys), T5_CAN_BITRATE,
                  T5_PIN_CAN_RX, T5_PIN_CAN_TX);
    printf("can: 500 kbit, TX GPIO%d, RX GPIO%d\n", T5_PIN_CAN_TX, T5_PIN_CAN_RX);
}

static void diag_log(uint32_t now_ms)
{
    static uint32_t last;
    if (now_ms - last < LOG_MS) return;
    last = now_ms;
    static const char *const ST[] = { "idle", "scanning", "polling", "NO ECU" };
    struct can2040_stats st;
    can2040_get_statistics(&s_cbus, &st);
    printf("obd: %s, ecu %03X, tx %lu rx %lu timeouts %lu neg %lu | can rx %lu "
           "tx %lu full %lu drop %lu, bus rx %lu tx %lu err %lu\n",
           g_obd.state <= OBD_NO_ECU ? ST[g_obd.state] : "?", g_obd.ecu_id,
           (unsigned long)g_obd.tx, (unsigned long)g_obd.rx,
           (unsigned long)g_obd.timeouts, (unsigned long)g_obd.negative,
           (unsigned long)s_rx_all, (unsigned long)s_tx_ok,
           (unsigned long)s_tx_full, (unsigned long)s_dropped,
           (unsigned long)st.rx_total, (unsigned long)st.tx_total,
           (unsigned long)st.parse_error);
    printf("     rpm %.0f boost %.2f clt %.0f iat %.0f oil %.0f egt %.0f "
           "soot %.2f dp %.0f km %.0f\n",
           g_dash.rpm, g_dash.boost, g_dash.clt, g_dash.iat, g_dash.oilt,
           g_dash.egt, g_obd.dpf.soot_g, g_obd.dpf.dp_hpa, g_obd.dpf.dist_km);
}

void can_t5_poll(void)
{
    int64_t now = (int64_t)time_us_64();
    while (s_tail != s_head) {
        uint32_t t = s_tail;
        obd_frame(s_ring[t % RING].id, s_ring[t % RING].d, s_ring[t % RING].len, now);
        s_tail = t + 1;
    }
    obd_tick(now);
    diag_log((uint32_t)(now / 1000));
}
