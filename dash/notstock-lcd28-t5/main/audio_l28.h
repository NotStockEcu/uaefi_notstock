/* Beeps through the board's PCM5101A DAC and its amplifier (the speaker on
 * the 4-pin MX1.25, volume on the board's trimmer): I2S on PIO1, BCK
 * GPIO2, LRCK GPIO3, DIN GPIO4. The DAC takes its clocks from BCK (SCK
 * grounded), is never muted (XSMT high), I2S format (FMT low). */
#pragma once

void audio_init(void);
void audio_beep(int n);       /* n short beeps; does not block */
void audio_volume(int level); /* 1..3 */
void audio_poll(void);        /* from the main loop */
