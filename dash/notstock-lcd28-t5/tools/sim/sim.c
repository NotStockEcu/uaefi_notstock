/* PC render of the 2.8" T5.1 screens (main/ui_t5.c) into a PPM.
 *   sim out.ppm [boost=BAR] [egt=C] [oil=C] [clt=C] [iat=C] [rpm=N]
 *               [speed=KMH] [soot=G] [sootm=G] [dp=MBAR] [km=KM]
 *               [dpft=C] [peak=BAR] [page=N] [screen=0..3] [link=0|1]
 *               [state=0|1|2] [dtc=N] [dtcres=0..4] [t=S]
 *               [lang=0|1] [hc=0|1] [big=MASK] [small=ABC]
 * screen: 0 gauges, 1 menu, 2 DPF, 3 diagnostics, 4 settings, 5 limits,
 * 6 display, 7 sound, 8 language; page: the big dial's value (T5_CH_*);
 * small: the three small dials' values as digits (T5_CH_*); state: 0 polling,
 * 1 scanning, 2 no ECU; dpft over 400: a regeneration; dtc: that many
 * made-up trouble codes; peak: a boost reached before (the MAX)
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

void t5_beep(int n) { fprintf(stderr, "beep x%d\n", n); }
void t5_backlight(uint8_t p) { (void)p; }
void t5_dtc_read(void) {}
void t5_dtc_clear(void) {}
void t5_settings_save(void) { fprintf(stderr, "settings saved\n"); }

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
        [T5_SPEED] = "speed", [T5_SOOT] = "soot", [T5_SOOT_MEAS] = "sootm",
        [T5_DP] = "dp", [T5_KM] = "km", [T5_DPF_T] = "dpft",
    };
    t5_view_t v = { .x = { [T5_BOOST] = 1.32f, [T5_EGT] = 612, [T5_OILT] = 96,
                           [T5_CLT] = 89, [T5_IAT] = 31, [T5_RPM] = 2650,
                           [T5_SPEED] = 87, [T5_SOOT] = 12.3f,
                           [T5_SOOT_MEAS] = 11.8f, [T5_DP] = 42, [T5_KM] = 318,
                           [T5_DPF_T] = 240 },
                    .link = true, .state = T5_ST_POLLING };
    t5_settings_defaults();
    float t_end = 1.0f, peak = NAN;
    int page = 0, screen = 0, ndtc = -1, dtcres = T5_DTC_READ;
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
        else if (!strncmp(a, "screen=", 7)) screen = (int)x;
        else if (!strncmp(a, "dtc=", 4)) ndtc = (int)x;
        else if (!strncmp(a, "dtcres=", 7)) dtcres = (int)x;
        else if (!strncmp(a, "lang=", 5)) g_t5_set.lang = (uint8_t)x;
        else if (!strncmp(a, "hc=", 3)) g_t5_set.contrast = (uint8_t)x;
        else if (!strncmp(a, "big=", 4)) g_t5_set.big = (uint8_t)x;
        else if (!strncmp(a, "small=", 6)) {
            for (int k = 0; k < T5_SMALL && e[1 + k]; k++) g_t5_set.small[k] = (uint8_t)(e[1 + k] - '0');
        }
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

    if (ndtc >= 0) {
        static const uint16_t C[] = { 0x0401, 0x2002, 0xC100, 0x242F, 0x0299 };
        v.dtc.result = (uint8_t)dtcres;
        v.dtc.n = (uint8_t)ndtc;
        v.dtc.seq = 1;
        for (int i = 0; i < ndtc && i < T5_DTC_MAX; i++) {
            v.dtc.list[i].code = C[i % 5];
            v.dtc.list[i].kind = i == 1 ? T5_DTC_PENDING : T5_DTC_STORED;
        }
    }
    ui_t5_create();
    ui_t5_page(page);
    ui_t5_open(screen);
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
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    fprintf(stderr, "lvgl mem: %u of %u used, max %u\n",
            (unsigned)(mon.total_size - mon.free_size), (unsigned)mon.total_size,
            (unsigned)mon.max_used);

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
