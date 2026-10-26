/* The EMU Black CAN stream on the RP2350, see can_l2.h. The RP2350 has no
 * CAN controller: can2040 is one in software, on PIO0. It is a full node:
 * it acknowledges what it receives (the EMU needs a partner that does,
 * see ../notstock-round-emu/README.md), and it never sends a frame of its
 * own here.
 *
 * The bit rate is the EMU's (125 k .. 1 M): tried one after the other, 500 k
 * first, until stream frames come; if they stop for 3 s it looks again. */
#include "can_l2.h"

#include <stdio.h>
#include <string.h>

#include "can2040.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/structs/timer.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#define TRY_MS   400        /* per bit rate while looking */
#define LOST_MS  3000       /* stream gone: look again */

static const uint32_t RATES[] = { 500000, 1000000, 250000, 125000 };
#define N_RATES (sizeof RATES / sizeof RATES[0])

static struct can2040 s_cbus;
static emu_values_t s_v;            /* written in the PIO interrupt */
static volatile uint32_t s_frames;  /* stream frames since start */
static volatile uint32_t s_rx_any;  /* any frame (a sign of the right rate) */
static int s_rate_i = -1;
static int s_kbit;                  /* 0: looking */
static uint32_t s_try_ms, s_seen_frames, s_seen_ms;

/* the time without a call into flash (time_us_64() is a function there) */
static inline int64_t now_us_ram(void)
{
    uint32_t hi = timer_hw->timerawh, lo;
    for (;;) {
        lo = timer_hw->timerawl;
        uint32_t hi2 = timer_hw->timerawh;
        if (hi2 == hi) break;
        hi = hi2;
    }
    return (int64_t)(((uint64_t)hi << 32) | lo);
}

/* in the PIO interrupt: all of it in RAM, as can2040 and emu_decode
 * (CMakeLists.txt), so no flash access holds the interrupt up */
static void __not_in_flash_func(rx_cb)(struct can2040 *cd, uint32_t notify,
                                       struct can2040_msg *m)
{
    (void)cd;
    if (notify != CAN2040_NOTIFY_RX) return;
    s_rx_any++;
    if (m->id & (CAN2040_ID_RTR | CAN2040_ID_EFF)) return;
    if (emu_decode(&s_v, EMU_BASE_ID, m->id, m->data, (int)m->dlc,
                   now_us_ram())) {
        s_frames++;
    }
}

static void __not_in_flash_func(pio_irq)(void)
{
    can2040_pio_irq_handler(&s_cbus);
}

static void start_rate(int i)
{
    if (s_rate_i >= 0) can2040_stop(&s_cbus);
    s_rate_i = i;
    can2040_start(&s_cbus, clock_get_hz(clk_sys), RATES[i], L2_PIN_CAN_RX, L2_PIN_CAN_TX);
    s_try_ms = to_ms_since_boot(get_absolute_time());
    s_seen_frames = s_frames;
    printf("can: trying %lu kbit\n", (unsigned long)(RATES[i] / 1000));
}

void can_l2_start(void)
{
    emu_init(&s_v);
    can2040_setup(&s_cbus, 0);
    can2040_callback_config(&s_cbus, rx_cb);
    irq_set_exclusive_handler(PIO0_IRQ_0, pio_irq);
    irq_set_priority(PIO0_IRQ_0, PICO_HIGHEST_IRQ_PRIORITY);
    irq_set_enabled(PIO0_IRQ_0, true);
    start_rate(0);
}

void can_l2_poll(void)
{
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t f = s_frames;
    if (f != s_seen_frames) {
        s_seen_frames = f;
        s_seen_ms = now;
        if (!s_kbit) {
            s_kbit = (int)(RATES[s_rate_i] / 1000);
            printf("can: EMU stream at %d kbit\n", s_kbit);
        }
        return;
    }
    if (s_kbit) {
        if (now - s_seen_ms > LOST_MS) {
            printf("can: stream lost, looking again\n");
            s_kbit = 0;
            start_rate(s_rate_i);          /* the last rate first */
        }
        return;
    }
    if (now - s_try_ms > TRY_MS) {
        start_rate((s_rate_i + 1) % (int)N_RATES);
    }
}

void can_l2_get(emu_values_t *out)
{
    uint32_t save = save_and_disable_interrupts();
    memcpy(out, &s_v, sizeof *out);
    restore_interrupts(save);
}

int can_l2_kbit(void)
{
    return s_kbit;
}
