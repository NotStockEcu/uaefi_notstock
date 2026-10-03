/* PC render of the round gauge UI (main/ui_round*.c) into a PPM, masked to
 * the round panel. Usage:
 *   sim out.ppm [page=N] [water=V] [oil=V] [boost=V] [intake=V]
 *               [exhaust=V] [rpm=V] [link=0|1] [t=S] [swipe=left|right]
 *               [screen=menu|dpf] [soot=G] [filter=C]
 *               [regen=start|end]   filter temperature crosses the
 *                                   regeneration threshold half way through
 *               [screen=settings|look|limits] [limit=N] [beep=0|1]
 *               [warnN=V]           warn limit N (page order, 6 = DPF soot)
 *               [boot=1]            start with the logo; t= picks the moment
 *               [look=N]            RND_LOOK_*: 0 NOTSTOCK, 1 RETRO, 2 FUTURO
 *               [relook=N,M,...]    switch looks while running, as LOOK does
 *               [dtap=N]            N double taps in the middle, then run t=
 *               [night=1] [nightlvl=P]
 *               [tap=x,y;x,y;...]   single taps, after the screen is up
 *               [hold=x,y,ms;...]   finger down at x,y for ms, then lifted
 *               [page=7]            MULTI; [multi=a,b,c,d] its slots (page
 *                                   ids, 6 DPF soot, 255 empty)
 *               [screen=multiedit]  its editor
 *               [lang=0|1]          RND_LANG_*: English, Czech
 *               [screen=diag]       trouble codes, read on opening: the
 *                                   fake car has three stored and one
 *                                   pending; [dtc=N] N of them (0..4),
 *                                   [dtcclear=1] clear them after 2 s,
 *                                   [dtcrefuse=1] the ECU refuses
 *               [hide=MASK] [order=a,b,c,d,e,f]   pages (screen=pages
 *                                   limit=N shows position N)
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "ui_round.h"

#define W RND_W
#define H RND_H
#define STEP_MS 33

static lv_color_t s_fb[W * H];
static int s_tx = -1, s_ty = -1;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px)
{
    for (int y = a->y1; y <= a->y2; y++) {
        int n = a->x2 - a->x1 + 1;
        memcpy(&s_fb[y * W + a->x1], px, n * sizeof(lv_color_t));
        px += n;
    }
    lv_disp_flush_ready(drv);
}

void rnd_beep(int n)
{
    fprintf(stderr, "beep x%d\n", n);
}

void rnd_settings_save(void)
{
    fprintf(stderr, "settings saved\n");
}

/* the backlight shows as darker pixels in the output */
static int s_backlight = 100;
void rnd_backlight(uint8_t percent)
{
    s_backlight = percent;
}

void rnd_sim_settings(const char *which, int limit);

/* ---------------------------------------------------- fake trouble codes */
static const rnd_dtc_t FAKE_DTC[] = {
    { 0x0401, 0, RND_DTC_STORED }, { 0x2463, 0, RND_DTC_STORED },
    { 0x0670, 0, RND_DTC_STORED }, { 0x0299, 0, RND_DTC_PENDING },
};
static int s_dtc_have = 4, s_dtc_refuse, s_dtc_busy_ms;
static rnd_dtc_status_t s_dtc;

void rnd_dtc_read(void)
{
    if (s_dtc.busy) return;
    s_dtc.busy = RND_DTC_READING;
    s_dtc.result = RND_DTC_READ;
    s_dtc_busy_ms = 1000;
}

void rnd_dtc_clear(void)
{
    if (s_dtc.busy) return;
    s_dtc.busy = RND_DTC_CLEARING;
    s_dtc_busy_ms = 1500;
    if (s_dtc_refuse) {
        s_dtc.result = RND_DTC_REFUSED;
        s_dtc.nrc = 0x22;
    } else {
        s_dtc.result = RND_DTC_CLEARED;
        s_dtc_have = 0;
    }
}

static void dtc_tick(int ms)
{
    if (!s_dtc.busy || (s_dtc_busy_ms -= ms) > 0) return;
    s_dtc.n = (uint8_t)s_dtc_have;
    memcpy(s_dtc.list, FAKE_DTC, sizeof FAKE_DTC);
    s_dtc.seq++;
    s_dtc.busy = RND_DTC_IDLE;
}

static void touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    data->point.x = s_tx;
    data->point.y = s_ty;
    data->state = s_tx < 0 ? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
}

static void run(float seconds, rnd_data_t *d)
{
    for (float t = 0; t < seconds; t += STEP_MS / 1000.0f) {
        dtc_tick(STEP_MS);
        d->dtc = s_dtc;
        ui_round_update(d);
        lv_tick_inc(STEP_MS);
        lv_timer_handler();
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: sim out.ppm [key=value ...]\n");
        return 1;
    }
    static const char *const KEY[RND_COUNT] = {
        "water", "oil", "boost", "intake", "exhaust", "rpm",
    };
    rnd_data_t d = {
        .v = { 86, 92, 1.12f, 31, 412, 2350 },
        .dpf = { 12.28f, -3.32f, 5, 274.5f, 90.5f },   /* the T5.1's log */
        .link = true,
    };
    const char *screen = NULL, *regen = NULL;
    int limit = 0, boot = 0;
    const char *relook = NULL;
    int dtap = 0, dtcclear = 0;
    const char *taps = NULL, *holds = NULL;
    rnd_settings_defaults();
    int page = 0;
    float t_end = 1.5f;
    const char *swipe = NULL;
    for (int i = 2; i < argc; i++) {
        char k[32];
        const char *eq = strchr(argv[i], '=');
        if (!eq || eq - argv[i] >= (int)sizeof k) continue;
        memcpy(k, argv[i], eq - argv[i]);
        k[eq - argv[i]] = 0;
        const char *v = eq + 1;
        bool used = false;
        for (int j = 0; j < RND_COUNT; j++) {
            if (strcmp(k, KEY[j]) == 0) {
                d.v[j] = strcmp(v, "nan") == 0 ? NAN : strtof(v, NULL);
                used = true;
            }
        }
        if (strcmp(k, "page") == 0)  { page = atoi(v); used = true; }
        if (strcmp(k, "link") == 0)  { d.link = atoi(v) != 0; used = true; }
        if (strcmp(k, "t") == 0)     { t_end = strtof(v, NULL); used = true; }
        if (strcmp(k, "swipe") == 0) { swipe = v; used = true; }
        if (strcmp(k, "screen") == 0) { screen = v; used = true; }
        if (strcmp(k, "limit") == 0) { limit = atoi(v); used = true; }
        if (strcmp(k, "boot") == 0)  { boot = atoi(v); used = true; }
        if (strcmp(k, "look") == 0)  { g_rnd_set.look = (uint8_t)atoi(v); used = true; }
        if (strcmp(k, "relook") == 0) { relook = v; used = true; }
        if (strcmp(k, "dtap") == 0)  { dtap = atoi(v); used = true; }
        if (strcmp(k, "tap") == 0)   { taps = v; used = true; }
        if (strcmp(k, "hold") == 0)  { holds = v; used = true; }
        if (strcmp(k, "hide") == 0)  { g_rnd_set.hidden = (uint8_t)atoi(v); used = true; }
        if (strcmp(k, "order") == 0) {
            const char *q = v;
            for (int i = 0; i < RND_PAGES && q; i++) {
                g_rnd_set.order[i] = (uint8_t)atoi(q);
                q = strchr(q, ',');
                if (q) q++;
            }
            used = true;
        }
        if (strcmp(k, "multi") == 0) {
            const char *q = v;
            for (int i = 0; i < RND_MULTI_SLOTS && q; i++) {
                g_rnd_set.multi[i] = (uint8_t)atoi(q);
                q = strchr(q, ',');
                if (q) q++;
            }
            used = true;
        }
        if (strcmp(k, "night") == 0) { g_rnd_set.night = atoi(v) != 0; used = true; }
        if (strcmp(k, "nightlvl") == 0) { g_rnd_set.night_level = (uint8_t)atoi(v); used = true; }
        if (strcmp(k, "dtc") == 0)   { s_dtc_have = atoi(v); used = true; }
        if (strcmp(k, "dtcclear") == 0) { dtcclear = atoi(v); used = true; }
        if (strcmp(k, "dtcrefuse") == 0) { s_dtc_refuse = atoi(v); used = true; }
        if (strcmp(k, "lang") == 0)  { g_rnd_set.lang = (uint8_t)atoi(v); used = true; }
        if (strcmp(k, "beep") == 0)  { g_rnd_set.beep = atoi(v) != 0; used = true; }
        if (strncmp(k, "warn", 4) == 0 && k[4] >= '0' && k[4] <= '9') {
            int n = atoi(k + 4);
            if (n < RND_WARN_COUNT) g_rnd_set.warn[n] = strtof(v, NULL);
            used = true;
        }
        if (strcmp(k, "regen") == 0) { regen = v; used = true; }
        if (strcmp(k, "soot") == 0)  { d.dpf.soot_g = strtof(v, NULL); used = true; }
        if (strcmp(k, "filter") == 0) { d.dpf.temp_c = strtof(v, NULL); used = true; }
        if (!used) fprintf(stderr, "unknown input '%s'\n", argv[i]);
    }

    lv_init();
    static lv_disp_draw_buf_t buf;
    static lv_color_t px[W * 40];
    lv_disp_draw_buf_init(&buf, px, NULL, W * 40);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res = W;
    drv.ver_res = H;
    drv.flush_cb = flush_cb;
    drv.draw_buf = &buf;
    lv_disp_drv_register(&drv);
    static lv_indev_drv_t indev;
    lv_indev_drv_init(&indev);
    indev.type = LV_INDEV_TYPE_POINTER;
    indev.read_cb = touch_cb;
    lv_indev_drv_register(&indev);

    ui_round_create(boot != 0);
    if (page) ui_round_page(page);
    if (screen) {
        extern lv_obj_t *rnd_dpf_screen(void);
        extern void rnd_menu_open(void);
        if (strcmp(screen, "dpf") == 0)  lv_scr_load(rnd_dpf_screen());
        if (strcmp(screen, "menu") == 0) rnd_menu_open();
        if (strcmp(screen, "diag") == 0) {
            extern void rnd_diag_open(void);
            rnd_diag_open();
        }
        if (strcmp(screen, "multiedit") == 0) {
            extern void rnd_multi_edit_open(void);
            rnd_multi_edit_open();
        }
        rnd_sim_settings(screen, limit);
    }
    if (regen) {
        /* first half on one side of the threshold, then the other */
        bool start = strcmp(regen, "start") == 0;
        d.dpf.temp_c = start ? 310 : 560;
        run(t_end / 2, &d);
        d.dpf.temp_c = start ? 585 : 290;
        if (!start) d.dpf.soot_g = 3.4f;
        run(t_end / 2, &d);
    } else if (dtcclear) {
        /* read on opening, then CLEAR twice, as a finger would */
        run(2.0f, &d);
        rnd_dtc_clear();
        run(t_end, &d);
    } else {
        run(t_end, &d);
    }

    for (const char *q = relook; q && *q; ) {
        extern void rnd_look_apply(void);
        g_rnd_set.look = (uint8_t)atoi(q);
        rnd_look_apply();
        run(0.5f, &d);
        q = strchr(q, ',');
        if (q) q++;
    }
    for (const char *q = taps; q && *q; ) {
        int x, y;
        if (sscanf(q, "%d,%d", &x, &y) == 2) {
            s_tx = x;
            s_ty = y;
            run(0.066f, &d);
            s_tx = s_ty = -1;
            run(0.5f, &d);
        }
        q = strchr(q, ';');
        if (q) q++;
    }
    for (const char *q = holds; q && *q; ) {
        int x, y, ms;
        if (sscanf(q, "%d,%d,%d", &x, &y, &ms) == 3) {
            s_tx = x;
            s_ty = y;
            run(ms / 1000.0f, &d);
            s_tx = s_ty = -1;
            run(0.5f, &d);
        }
        q = strchr(q, ';');
        if (q) q++;
    }
    for (int i = 0; i < dtap; i++) {
        for (int k = 0; k < 2; k++) {       /* two short taps, 130 ms apart */
            s_tx = W / 2;
            s_ty = H / 2 + 40;
            run(0.066f, &d);
            s_tx = s_ty = -1;
            run(0.066f, &d);
        }
        run(0.6f, &d);
    }
    if (swipe) {
        /* a finger across the middle, then let the new page settle */
        int dir = strcmp(swipe, "left") == 0 ? -1 : 1;
        for (int i = 0; i <= 8; i++) {
            s_tx = W / 2 - dir * 150 + dir * 300 * i / 8;
            s_ty = H / 2;
            run(STEP_MS / 1000.0f, &d);
        }
        s_tx = s_ty = -1;
        run(t_end, &d);
    }
    lv_refr_now(NULL);

    /* round panel: outside the circle is the bezel */
    FILE *f = fopen(argv[1], "wb");
    if (!f) return 1;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float dx = x - W / 2 + 0.5f, dy = y - H / 2 + 0.5f;
            unsigned char rgb[3];
            if (dx * dx + dy * dy > (W / 2.0f) * (W / 2.0f)) {
                rgb[0] = rgb[1] = rgb[2] = 0x2A;
            } else {
                lv_color_t c = s_fb[y * W + x];
                rgb[0] = (unsigned char)(c.ch.red << 3 | c.ch.red >> 2);
                rgb[1] = (unsigned char)(c.ch.green << 2 | c.ch.green >> 4);
                rgb[2] = (unsigned char)(c.ch.blue << 3 | c.ch.blue >> 2);
                for (int k = 0; k < 3; k++) rgb[k] = rgb[k] * s_backlight / 100;
            }
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    return 0;
}
