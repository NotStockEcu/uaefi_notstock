/* regen_detect.h on the host: cc -I../../../notstock-dash-7inch/main test_regen.c -lm (from tools/test) */
#include "regen_detect.h"
#include <assert.h>
#include <stdio.h>

static regen_det_t r = REGEN_DET_INIT;
static uint32_t t;

/* feed temp and distance for ms, once a second; how many flips */
static int feed(float temp, float km, uint32_t ms)
{
    int flips = 0;
    for (uint32_t e = 0; e < ms; e += 1000, t += 1000) flips += regen_step(&r, temp, km, t);
    return flips;
}

int main(void)
{
    assert(feed(450, 300, 60000) == 0 && !r.on);         /* uphill, warm */
    assert(feed(570, 300, 20000) == 0 && !r.on);         /* not held long enough */
    assert(feed(450, 301, 5000) == 0);
    assert(feed(600, 301, 29000) == 0 && !r.on);
    assert(feed(600, 301, 2000) == 1 && r.on);           /* held 30 s: on */
    assert(feed(620, 310, 600000) == 0 && r.on);         /* 10 minutes */
    assert(feed(610, 0.2f, 1000) == 1 && !r.on);         /* set back: off at once */
    assert(feed(600, 0.5f, 120000) == 0 && !r.on);       /* still hot: no start */
    assert(feed(400, 1.0f, 5000) == 0);                  /* cooled */
    assert(feed(600, 2.0f, 31000) == 1 && r.on);         /* a new one */
    assert(feed(450, 2.5f, 31000) == 1 && !r.on);        /* broken off: by temperature */
    assert(feed(NAN, NAN, 10000) == 0 && !r.on);         /* nothing read */
    puts("regen_detect: ok");
    return 0;
}
