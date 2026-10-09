/* PC render of the A4 dials (main/ui_a4.c) into a PPM, masked round.
 *   sim out.ppm [page=0..5] [oil=C] [iat=C] [clt=C] [egt=C] [boost=BAR]
 *               [soot=G] [dp=MBAR] [dpft=C] [link=0|1] [t=S] [sweep=1]
 * dpft: the filter temperature, over 400 the regeneration is on
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "ui_a4.h"

#define N A4_SIZE
#define STEP_MS 33

static lv_color_t s_fb[N * N];

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px)
{
    for (int y = a->y1; y <= a->y2; y++) {
        int n = a->x2 - a->x1 + 1;
        memcpy(&s_fb[y * N + a->x1], px, n * sizeof(lv_color_t));
        px += n;
    }
    lv_disp_flush_ready(drv);
}

void a4_backlight(uint8_t p) { fprintf(stderr, "backlight %u\n", p); }
void a4_regen_sound(bool start) { fprintf(stderr, "regen %s\n", start ? "start" : "end"); }

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: sim out.ppm [key=value ...], see sim.c\n");
        return 1;
    }
    rnd_data_t d;
    memset(&d, 0, sizeof d);
    d.link = true;
    d.v[RND_OIL] = 96;
    d.v[RND_INTAKE] = 34;
    d.v[RND_WATER] = 90;
    d.v[RND_EXHAUST] = 420;
    d.v[RND_BOOST] = 1.42f;
    d.v[RND_RPM] = 2600;
    d.dpf.soot_g = 14.6f;
    d.dpf.dp_hpa = 18;
    d.dpf.temp_c = 260;
    d.dpf.soot_meas_g = d.dpf.dist_km = NAN;
    int page = 0, sweep = 0;
    float t_end = 2.0f;
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        const char *v = strchr(a, '=');
        if (!v) continue;
        v++;
#define K(name) (!strncmp(a, name "=", strlen(name) + 1))
        if (K("page")) page = atoi(v);
        else if (K("oil")) d.v[RND_OIL] = strtof(v, NULL);
        else if (K("iat")) d.v[RND_INTAKE] = strtof(v, NULL);
        else if (K("clt")) d.v[RND_WATER] = strtof(v, NULL);
        else if (K("egt")) d.v[RND_EXHAUST] = strtof(v, NULL);
        else if (K("boost")) d.v[RND_BOOST] = strtof(v, NULL);
        else if (K("soot")) d.dpf.soot_g = strtof(v, NULL);
        else if (K("dp")) d.dpf.dp_hpa = strtof(v, NULL);
        else if (K("dpft")) d.dpf.temp_c = strtof(v, NULL);
        else if (K("link")) d.link = atoi(v);
        else if (K("t")) t_end = strtof(v, NULL);
        else if (K("sweep")) sweep = atoi(v);
        else fprintf(stderr, "unknown input '%s'\n", a);
#undef K
    }

    lv_init();
    static lv_disp_draw_buf_t db;
    static lv_color_t buf[N * 40];
    lv_disp_draw_buf_init(&db, buf, NULL, N * 40);
    static lv_disp_drv_t dd;
    lv_disp_drv_init(&dd);
    dd.hor_res = N;
    dd.ver_res = N;
    dd.flush_cb = flush_cb;
    dd.draw_buf = &db;
    lv_disp_drv_register(&dd);

    ui_a4_create(page);
    if (sweep) ui_a4_sweep();
    for (float t = 0; t < t_end; t += STEP_MS / 1000.0f) {
        ui_a4_update(&d);
        lv_tick_inc(STEP_MS);
        lv_timer_handler();
    }
    lv_refr_now(NULL);

    FILE *f = fopen(argv[1], "wb");
    if (!f) return 1;
    fprintf(f, "P6\n%d %d\n255\n", N, N);
    for (int i = 0; i < N * N; i++) {
        int x = i % N, y = i / N;
        float r = hypotf(x - N / 2 + 0.5f, y - N / 2 + 0.5f);
        lv_color_t c = s_fb[i];
        uint8_t rgb[3] = { (uint8_t)(c.ch.red << 3 | c.ch.red >> 2),
                           (uint8_t)(c.ch.green << 2 | c.ch.green >> 4),
                           (uint8_t)(c.ch.blue << 3 | c.ch.blue >> 2) };
        if (r > N / 2) rgb[0] = rgb[1] = rgb[2] = 24;     /* outside the panel */
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 0;
}
