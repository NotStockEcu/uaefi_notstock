/* PC render of the 2.8" T5.1 pages (main/ui_t5.c) into a PPM.
 *   sim out.ppm [boost=BAR] [egt=C] [oil=C] [clt=C] [iat=C] [rpm=N]
 *               [speed=KMH] [soot=G] [dp=MBAR] [km=KM] [peak=BAR]
 *               [page=N] [link=0|1] [state=0|1|2] [t=S]
 * state: 0 polling, 1 scanning, 2 no ECU; peak: a boost reached before
 * (the MAX), then the value now
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "ui_t5.h"

#define W T5_W
#define H T5_H
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

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: sim out.ppm [key=value ...], see sim.c\n");
        return 1;
    }
    static const char *const KEY[T5_N] = {
        [T5_BOOST] = "boost", [T5_EGT] = "egt", [T5_OILT] = "oil",
        [T5_CLT] = "clt", [T5_IAT] = "iat", [T5_RPM] = "rpm",
        [T5_SPEED] = "speed", [T5_SOOT] = "soot", [T5_DP] = "dp", [T5_KM] = "km",
    };
    t5_view_t v = { .x = { 1.32f, 612, 96, 89, 31, 2650, 87, 14.6f, 42, 318 },
                    .link = true, .state = T5_ST_POLLING };
    float t_end = 1.0f, peak = NAN;
    int page = 0;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *e = strchr(a, '=');
        if (!e) continue;
        size_t n = (size_t)(e - a);
        float x = strtof(e + 1, NULL);
        bool done = false;
        for (int k = 0; k < T5_N; k++) {
            if (strlen(KEY[k]) == n && !strncmp(a, KEY[k], n)) {
                v.x[k] = x;
                done = true;
            }
        }
        if (done) continue;
        if (!strncmp(a, "peak=", 5)) peak = x;
        else if (!strncmp(a, "page=", 5)) page = (int)x;
        else if (!strncmp(a, "link=", 5)) v.link = x != 0;
        else if (!strncmp(a, "state=", 6)) v.state = (int)x;
        else if (!strncmp(a, "t=", 2)) t_end = x;
        else fprintf(stderr, "unknown input '%s'\n", a);
    }
    if (!v.link) {
        for (int k = 0; k < T5_N; k++) v.x[k] = NAN;
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

    ui_t5_create();
    ui_t5_page(page);
    if (!isnan(peak)) {
        t5_view_t p = v;
        p.x[T5_BOOST] = peak;
        ui_t5_update(&p);
    }
    for (float t = 0; t < t_end; t += STEP_MS / 1000.0f) {
        ui_t5_update(&v);
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
