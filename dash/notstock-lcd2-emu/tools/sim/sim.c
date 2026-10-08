/* PC render of the 2" EMU pages (main/ui_l2.c) into a PPM. The values go
 * in as real stream frames through emu_stream.c, so the decoder is in the
 * loop.
 *   sim out.ppm [boost=BAR] [lambda=L] [iat=C] [clt=C] [tps=%] [rpm=N]
 *               [oilt=C] [oilp=BAR] [batt=V] [egt=C] [peak=BAR] [page=N]
 *               [link=0|1] [t=S] [err=MASK] [fan=0|1|-1]
 * err: the ECU's ERRFLAG bits (EMU_ERR_*), e.g. err=8 the wideband failed;
 * peak: a boost reached before (the MAX), then the value now
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "ui_l2.h"

#define W L2_W
#define H L2_H
#define STEP_MS 33

static lv_color_t s_fb[W * H];

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px)
{
    for (int y = a->y1; y <= a->y2; y++) {
        int n = a->x2 - a->x1 + 1;
        memcpy(&s_fb[y * W + a->x1], px, n * sizeof(lv_color_t));
        px += n;
    }
    lv_disp_flush_ready(drv);
}

typedef struct {
    float boost, lambda, iat, clt, tps, rpm, oilt, oilp, batt, egt;
    unsigned err;
    int fan;
} in_t;

/* the frames an EMU Black would send for these values */
static void send(emu_values_t *v, const in_t *in, int64_t now)
{
    uint8_t d[8] = {0};
    unsigned map = (unsigned)lroundf(101 + in->boost * 100);
    unsigned rpm = (unsigned)lroundf(in->rpm);
    d[0] = rpm & 0xFF; d[1] = rpm >> 8;
    d[2] = (uint8_t)lroundf(in->tps * 2);
    d[3] = (uint8_t)(int8_t)lroundf(in->iat);
    d[4] = map & 0xFF; d[5] = map >> 8;
    emu_decode(v, EMU_BASE_ID, EMU_BASE_ID + 0, d, 8, now);
    memset(d, 0, 8);
    d[2] = 101;                                   /* baro */
    d[3] = (uint8_t)lroundf(in->oilt);
    d[4] = (uint8_t)lroundf(in->oilp / 0.0625f);
    int c = (int)lroundf(in->clt);
    d[6] = c & 0xFF; d[7] = (c >> 8) & 0xFF;
    emu_decode(v, EMU_BASE_ID, EMU_BASE_ID + 2, d, 8, now);
    memset(d, 0, 8);
    d[2] = (uint8_t)lroundf(in->lambda * 128);
    unsigned egt = (unsigned)lroundf(in->egt);
    d[4] = egt & 0xFF; d[5] = egt >> 8;
    emu_decode(v, EMU_BASE_ID, EMU_BASE_ID + 3, d, 8, now);
    memset(d, 0, 8);
    unsigned bat = (unsigned)lroundf(in->batt / 0.027f);
    d[2] = bat & 0xFF; d[3] = bat >> 8;
    d[4] = in->err & 0xFF; d[5] = in->err >> 8;
    emu_decode(v, EMU_BASE_ID, EMU_BASE_ID + 4, d, 8, now);
    if (in->fan >= 0) {
        memset(d, 0, 8);
        d[7] = in->fan ? 0x03 : 0x01;            /* fuel pump on, fan */
        emu_decode(v, EMU_BASE_ID, EMU_BASE_ID + 6, d, 8, now);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: sim out.ppm [key=value ...], see sim.c\n");
        return 1;
    }
    in_t in = { 1.45f, 0.82f, 31, 88, 64, 4200, 96, 4.2f, 14.1f, 780, 0, 0 };
    float t_end = 1.0f, peak = NAN;
    int page = 0, link = 1;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = strchr(a, '=');
        if (!v) continue;
        v++;
#define K(name) (!strncmp(a, name "=", strlen(name) + 1))
        if (K("boost")) in.boost = strtof(v, NULL);
        else if (K("lambda")) in.lambda = strtof(v, NULL);
        else if (K("iat")) in.iat = strtof(v, NULL);
        else if (K("clt")) in.clt = strtof(v, NULL);
        else if (K("tps")) in.tps = strtof(v, NULL);
        else if (K("rpm")) in.rpm = strtof(v, NULL);
        else if (K("oilt")) in.oilt = strtof(v, NULL);
        else if (K("oilp")) in.oilp = strtof(v, NULL);
        else if (K("batt")) in.batt = strtof(v, NULL);
        else if (K("egt")) in.egt = strtof(v, NULL);
        else if (K("peak")) peak = strtof(v, NULL);
        else if (K("page")) page = atoi(v);
        else if (K("link")) link = atoi(v);
        else if (K("t")) t_end = strtof(v, NULL);
        else if (K("err")) in.err = (unsigned)atoi(v);
        else if (K("fan")) in.fan = atoi(v);
        else fprintf(stderr, "unknown input '%s'\n", a);
#undef K
    }

    lv_init();
    static lv_disp_draw_buf_t db;
    static lv_color_t buf[W * 40];
    lv_disp_draw_buf_init(&db, buf, NULL, W * 40);
    static lv_disp_drv_t dd;
    lv_disp_drv_init(&dd);
    dd.hor_res = W;
    dd.ver_res = H;
    dd.flush_cb = flush_cb;
    dd.draw_buf = &db;
    lv_disp_drv_register(&dd);

    ui_l2_create();
    ui_l2_page(page);
    emu_values_t v;
    emu_init(&v);
    int64_t now = 1000000;
    if (!isnan(peak) && link) {
        in_t p = in;
        p.boost = peak;
        now += STEP_MS * 1000;
        send(&v, &p, now);
        l2_view_t view;
        l2_view_from(&v, now, &view);
        ui_l2_update(&view);
    }
    for (float t = 0; t < t_end; t += STEP_MS / 1000.0f) {
        now += STEP_MS * 1000;
        if (link) send(&v, &in, now);
        l2_view_t view;
        l2_view_from(&v, now, &view);
        ui_l2_update(&view);
        lv_tick_inc(STEP_MS);
        lv_timer_handler();
    }
    lv_refr_now(NULL);

    FILE *f = fopen(argv[1], "wb");
    if (!f) return 1;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        lv_color_t c = s_fb[i];
        uint8_t rgb[3] = { (uint8_t)(c.ch.red << 3 | c.ch.red >> 2),
                           (uint8_t)(c.ch.green << 2 | c.ch.green >> 4),
                           (uint8_t)(c.ch.blue << 3 | c.ch.blue >> 2) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 0;
}
