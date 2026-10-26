/* The regeneration sounds as PCM (16 kHz mono 16 bit), for the boards with
 * a speaker: sounds.c, made by tools/gen_sounds.py. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ui_round.h"

#define RND_PCM_RATE 16000

/* sound RND_SND_*, event RND_EV_*, lang RND_LANG_*: the samples, NULL for
 * OFF and BEEP (the board beeps those itself) */
const int16_t *rnd_pcm(int sound, int event, int lang, size_t *n);
