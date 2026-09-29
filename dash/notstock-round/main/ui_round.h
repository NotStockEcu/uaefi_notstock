/* Round gauge UI: one value at a time on a 466 x 466 round AMOLED, swipe
 * left / right for the next one. ESP-free, so tools/sim renders it on a PC.
 */
#pragma once
#include <stdbool.h>
#include "lvgl.h"

#define RND_W 466
#define RND_H 466

enum { RND_WATER, RND_OIL, RND_BOOST, RND_INTAKE, RND_EXHAUST, RND_RPM,
       RND_COUNT };

/* live values, NAN where unknown; link false: nothing from the car */
typedef struct {
    float v[RND_COUNT];
    bool  link;
} rnd_data_t;

void ui_round_create(void);
void ui_round_update(const rnd_data_t *d);   /* call at ~30 Hz */
void ui_round_page(int page);                 /* what a swipe does */
int  ui_round_current(void);
