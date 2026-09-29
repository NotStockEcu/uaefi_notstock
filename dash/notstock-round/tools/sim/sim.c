/* PC render of the round gauge UI (main/ui_round.c) into a PPM, masked to
 * the round panel. Usage:
 *   sim out.ppm [page=N] [water=V] [oil=V] [boost=V] [intake=V]
 *               [exhaust=V] [rpm=V] [link=0|1] [t=S] [swipe=left|right]
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

static void touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    data->point.x = s_tx;
    data->point.y = s_ty;
    data->state = s_tx < 0 ? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
}

static void run(float seconds, const rnd_data_t *d)
{
    for (float t = 0; t < seconds; t += STEP_MS / 1000.0f) {
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
        "water", "oil", "boost", "intake", "exhaust", "rpm", "dpf",
    };
    rnd_data_t d = { .v = { 86, 92, 1.12f, 31, 412, 2350, 34 }, .link = true };
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

    ui_round_create();
    if (page) ui_round_page(page);
    run(t_end, &d);

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
            }
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    return 0;
}
