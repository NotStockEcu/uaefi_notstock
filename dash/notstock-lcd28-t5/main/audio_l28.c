/* PCM5101A beeps, see audio_l28.h. The DAC wants its bit clock running all
 * the time, so silence plays endlessly (a DMA channel in endless mode on a
 * buffer of zeros); a beep is a second channel on one period of a 3 kHz
 * tone (16 frames at 48 kHz, read as a ring), chained back to the silence
 * when it ends. The main loop starts the next beep of a series. */
#include "audio_l28.h"

#include <math.h>
#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "i2s_out.pio.h"
#include "pico/stdlib.h"

#define PIN_BCK  2              /* LRCK on 3 */
#define PIN_DIN  4
#define RATE     48000
#define TONE_HZ  3000           /* 16 frames a period: a 64-byte ring */
#define BEEP_MS  120
#define GAP_MS   100

static uint32_t s_tone[16] __attribute__((aligned(64)));
static uint32_t s_zero[16] __attribute__((aligned(64)));
static int s_sm = -1, s_tone_ch, s_sil_ch;
static int s_left;              /* beeps still to start */
static uint32_t s_next_ms;

static void ch_config(int ch, const uint32_t *src, int chain_to)
{
    dma_channel_config c = dma_channel_get_default_config(ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_ring(&c, false, 6);              /* 64 bytes */
    channel_config_set_dreq(&c, pio_get_dreq(pio1, s_sm, true));
    channel_config_set_chain_to(&c, chain_to);
    dma_channel_configure(ch, &c, &pio1->txf[s_sm], src, 0, false);
}

void audio_volume(int level)
{
    static const float AMP[3] = { 3000, 9000, 22000 };     /* of 32767 */
    if (level < 1) level = 1;
    if (level > 3) level = 3;
    for (int i = 0; i < 16; i++) {
        int16_t v = (int16_t)lroundf(AMP[level - 1] * sinf(2 * (float)M_PI * i / 16));
        s_tone[i] = ((uint32_t)(uint16_t)v << 16) | (uint16_t)v;
    }
}

void audio_init(void)
{
    audio_volume(2);
    uint off = pio_add_program(pio1, &i2s_out_program);
    s_sm = pio_claim_unused_sm(pio1, true);
    pio_sm_config c = i2s_out_program_get_default_config(off);
    sm_config_set_out_pins(&c, PIN_DIN, 1);
    sm_config_set_sideset_pins(&c, PIN_BCK);
    sm_config_set_out_shift(&c, false, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    float div = (float)clock_get_hz(clk_sys) / (RATE * 64.0f);
    sm_config_set_clkdiv(&c, div);
    pio_gpio_init(pio1, PIN_BCK);
    pio_gpio_init(pio1, PIN_BCK + 1);
    pio_gpio_init(pio1, PIN_DIN);
    pio_sm_set_consecutive_pindirs(pio1, s_sm, PIN_BCK, 3, true);
    pio_sm_init(pio1, s_sm, off + i2s_out_offset_entry_point, &c);

    s_tone_ch = dma_claim_unused_channel(true);
    s_sil_ch = dma_claim_unused_channel(true);
    ch_config(s_sil_ch, s_zero, s_sil_ch);              /* chains to nothing */
    ch_config(s_tone_ch, s_tone, s_sil_ch);             /* then silence */
    dma_channel_set_trans_count(s_tone_ch, RATE * BEEP_MS / 1000, false);
    /* silence for ever: the RP2350's endless transfer count */
    dma_channel_set_trans_count(s_sil_ch, (0xFu << 28) | 1, true);
    pio_sm_set_enabled(pio1, s_sm, true);
    printf("audio: I2S %d Hz on PIO1, beep %d Hz\n", RATE, TONE_HZ);
}

static void beep_now(void)
{
    dma_channel_abort(s_sil_ch);
    dma_channel_set_read_addr(s_tone_ch, s_tone, false);
    dma_channel_set_trans_count(s_tone_ch, RATE * BEEP_MS / 1000, true);
}

void audio_beep(int n)
{
    if (s_sm < 0 || n <= 0) return;
    s_left = n;
    s_next_ms = to_ms_since_boot(get_absolute_time());
}

void audio_poll(void)
{
    if (s_sm < 0 || !s_left) return;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if ((int32_t)(now - s_next_ms) < 0) return;
    beep_now();
    s_left--;
    s_next_ms = now + BEEP_MS + GAP_MS;
}
