/* PC render of the EMU page (main/ui_emu.c) into a PPM. The values go in as
 * real stream frames through emu_stream.c, so the decoder is in the loop.
 *   sim out.ppm [boost=BAR] [afr=AFR] [clt=C] [link=0|1] [t=S] [err=MASK]
 * err: the ECU's ERRFLAG bits (EMU_ERR_*), e.g. err=8 the wideband failed
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "ui_emu.h"

#define W EMU_W
#define H EMU_H
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

/* the frames an EMU Black would send for these values */
static void send(emu_values_t *v, float boost, float afr, float clt,
                 unsigned err, int64_t now)
{
    uint8_t d[8] = {0};
    unsigned map = (unsigned)lroundf(101 + boost * 100);
    d[0] = 0xB8; d[1] = 0x0B;                     /* 3000 rpm */
    d[4] = map & 0xFF; d[5] = map >> 8;
    emu_decode(v, EMU_BASE_ID, EMU_BASE_ID + 0, d, 8, now);
    memset(d, 0, 8);
    d[2] = 101;                                   /* baro */
    int c = (int)lroundf(clt);
    d[6] = c & 0xFF; d[7] = (c >> 8) & 0xFF;
    emu_decode(v, EMU_BASE_ID, EMU_BASE_ID + 2, d, 8, now);
    memset(d, 0, 8);
    d[2] = (uint8_t)lroundf(afr / EMU_STOICH * 128);
    emu_decode(v, EMU_BASE_ID, EMU_BASE_ID + 3, d, 8, now);
    memset(d, 0, 8);
    d[4] = err & 0xFF; d[5] = err >> 8;
    emu_decode(v, EMU_BASE_ID, EMU_BASE_ID + 4, d, 8, now);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: sim out.ppm [boost=] [afr=] [clt=] [link=] [t=]\n");
        return 1;
    }
    float boost = 1.45f, afr = 12.1f, clt = 88, t_end = 1.0f;
    int link = 1;
    unsigned err = 0;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (!strncmp(a, "boost=", 6)) boost = strtof(a + 6, NULL);
        if (!strncmp(a, "afr=", 4)) afr = strtof(a + 4, NULL);
        if (!strncmp(a, "clt=", 4)) clt = strtof(a + 4, NULL);
        if (!strncmp(a, "link=", 5)) link = atoi(a + 5);
        if (!strncmp(a, "t=", 2)) t_end = strtof(a + 2, NULL);
        if (!strncmp(a, "err=", 4)) err = (unsigned)atoi(a + 4);
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

    ui_emu_create();
    emu_values_t v;
    emu_init(&v);
    int64_t now = 1000000;
    for (float t = 0; t < t_end; t += STEP_MS / 1000.0f) {
        now += STEP_MS * 1000;
        if (link) send(&v, boost, afr, clt, err, now);
        emu_view_t view;
        emu_view_from(&v, now, &view);
        ui_emu_update(&view);
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
