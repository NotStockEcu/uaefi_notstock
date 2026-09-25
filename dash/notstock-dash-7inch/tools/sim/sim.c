/* Host-side renderer for the dash.
 *
 * Builds the real main/ui.c, ui_menu.c, settings.c, fonts and artwork against
 * LVGL on the PC and dumps one frame as a binary PPM. Nothing here is a copy
 * of the layout, so what it draws is what the panel draws.
 *
 *   sim out.ppm [rpm=5700] [speed=120] [clt=88] [iat=35] [boost=0.8]
 *               [afr=12.5] [link=0|1] [demo=1] [t=3.5] [screen=menu]
 *
 * tools/preview.py builds this and turns the PPM into PNGs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"
#include "rusefi_can.h"
#include "settings.h"
#include "ui.h"
#include "ui_menu.h"

#define W 800
#define H 480
#define STEP_MS 40

/* ------------------------------------------------ what the firmware expects */
volatile dash_data_t g_dash;
static int64_t s_now_us;
static int s_link = 1;

int64_t esp_timer_get_time(void) { return s_now_us; }
bool rusefi_can_link_ok(void) { return s_link != 0; }
void rusefi_can_start(void) {}

/* ------------------------------------------------------------ framebuffer */
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

static int write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        uint32_t c = lv_color_to32(s_fb[i]);
        uint8_t rgb[3] = { (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 0;
}

/* ---------------------------------------------------------------- inputs */
typedef struct { const char *name; volatile float *field; } input_t;

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s out.ppm [name=value ...]\n", argv[0]);
        return 2;
    }

    const input_t inputs[] = {
        { "rpm",   &g_dash.rpm },
        { "speed", &g_dash.speed },
        { "clt",   &g_dash.clt },
        { "iat",   &g_dash.iat },
        { "boost", &g_dash.boost },
        { "afr",   &g_dash.afr },
    };
    bool menu = false;
    bool demo = false;
    float t_end = 4.0f;

    for (int i = 2; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (!eq) continue;
        *eq = 0;
        const char *k = argv[i], *v = eq + 1;
        bool used = false;
        for (size_t j = 0; j < sizeof inputs / sizeof inputs[0]; j++) {
            if (strcmp(k, inputs[j].name) == 0) {
                *inputs[j].field = strtof(v, NULL);
                used = true;
            }
        }
        if (strcmp(k, "link") == 0)   { s_link = atoi(v); used = true; }
        if (strcmp(k, "demo") == 0)   { demo = atoi(v) != 0; used = true; }
        if (strcmp(k, "t") == 0)      { t_end = strtof(v, NULL); used = true; }
        if (strcmp(k, "screen") == 0) { menu = strcmp(v, "menu") == 0; used = true; }
        if (!used) fprintf(stderr, "unknown input '%s'\n", k);
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

    settings_load();
    g_set.demo = demo;
    g_dash.last_rx_us = 1;
    ui_create();
    if (menu) {
        ui_menu_refresh();
        lv_scr_load(ui_menu_screen());
    }

    /* run the UI timer long enough for the needle smoothing to settle */
    for (float t = 0; t < t_end; t += STEP_MS / 1000.0f) {
        s_now_us += STEP_MS * 1000;
        lv_tick_inc(STEP_MS);
        lv_timer_handler();
    }
    lv_refr_now(NULL);

    return write_ppm(argv[1]) == 0 ? 0 : 1;
}
