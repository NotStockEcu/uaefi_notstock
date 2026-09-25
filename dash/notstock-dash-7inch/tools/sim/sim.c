/* Host-side renderer for the dash.
 *
 * Builds the real main/ui.c, ui_menu.c, settings.c, fonts and artwork against
 * LVGL on the PC and dumps one frame as a binary PPM. Nothing here is a copy
 * of the layout, so what it draws is what the panel draws.
 *
 *   sim out.ppm [rpm=5700] [speed=120] [clt=88] [iat=35] [boost=0.8]
 *               [afr=12.5] [link=0|1] [demo=1] [t=3.5] [screen=menu]
 *               [touch=x,y]   finger held at x,y for the whole run
 *
 *   boot=ms        the boot animation at ms after power-up, crossfading
 *                  into the dash rendered from the other inputs
 *   screen=log     the LOG screen; with demo=1 t=30 it has 30 s of history
 *   night=1        night mode
 *   area=0|1       shift flash on the whole screen / on the rev counter
 *   colour=0..3    shift flash red / white / blue / amber
 *   peak_clt=.. peak_iat=.. peak_boost=..
 *                  hold these for the first second, then drop to the
 *                  normal values, to show the peak needles
 *
 * tools/preview.py builds this and turns the PPM into PNGs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "lvgl.h"
#include "boot_anim.h"
#include "rusefi_can.h"
#include "settings.h"
#include "ui.h"
#include "ui_menu.h"
#include "ui_log.h"

#define W 800
#define H 480
#define STEP_MS 40

/* ------------------------------------------------ what the firmware expects */
volatile dash_data_t g_dash;
static int64_t s_now_us;
static int s_link = 1;
static int s_touch_x = -1, s_touch_y = -1;   /* held down the whole run */

static void touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    data->point.x = s_touch_x < 0 ? 0 : s_touch_x;
    data->point.y = s_touch_y < 0 ? 0 : s_touch_y;
    data->state = s_touch_x < 0 ? LV_INDEV_STATE_RELEASED
                                : LV_INDEV_STATE_PRESSED;
}

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
    float peak[3] = { NAN, NAN, NAN };
    volatile float *peak_field[3] = { &g_dash.clt, &g_dash.iat, &g_dash.boost };
    const char *peak_name[3] = { "peak_clt", "peak_iat", "peak_boost" };
    int night = 0, area = 0, colour = 0;
    bool menu = false, logscr = false;
    bool demo = false;
    int boot_ms = -1;
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
        if (strcmp(k, "screen") == 0) {
            menu = strcmp(v, "menu") == 0;
            logscr = strcmp(v, "log") == 0;
            used = true;
        }
        if (strcmp(k, "boot") == 0)   { boot_ms = atoi(v); used = true; }
        if (strcmp(k, "night") == 0)  { night = atoi(v); used = true; }
        if (strcmp(k, "area") == 0)   { area = atoi(v); used = true; }
        if (strcmp(k, "colour") == 0) { colour = atoi(v); used = true; }
        for (int j = 0; j < 3; j++) {
            if (strcmp(k, peak_name[j]) == 0) {
                peak[j] = strtof(v, NULL);
                used = true;
            }
        }
        if (strcmp(k, "touch") == 0) {
            sscanf(v, "%d,%d", &s_touch_x, &s_touch_y);
            used = true;
        }
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

    static lv_indev_drv_t indev;
    lv_indev_drv_init(&indev);
    indev.type = LV_INDEV_TYPE_POINTER;
    indev.read_cb = touch_cb;
    lv_indev_drv_register(&indev);

    settings_load();
    g_set.demo = demo;
    g_set.night = night != 0;
    g_set.flash_area = (uint8_t)area;
    g_set.flash_colour = (uint8_t)colour;
    g_dash.last_rx_us = 1;
    ui_create();
    if (menu) {
        ui_menu_refresh();
        lv_scr_load(ui_menu_screen());
    }
    if (logscr) lv_scr_load(ui_log_screen());

    /* peaks first, then the real values */
    float real[3];
    for (int j = 0; j < 3; j++) {
        real[j] = *peak_field[j];
        if (!isnan(peak[j])) *peak_field[j] = peak[j];
    }

    /* run the UI timer long enough for the needle smoothing to settle */
    for (float t = 0; t < t_end; t += STEP_MS / 1000.0f) {
        if (t >= 1.0f && t < 1.0f + STEP_MS / 1000.0f) {
            for (int j = 0; j < 3; j++) *peak_field[j] = real[j];
        }
        s_now_us += STEP_MS * 1000;
        lv_tick_inc(STEP_MS);
        lv_timer_handler();
    }
    lv_refr_now(NULL);

    if (boot_ms >= 0) {
        /* same frames the panel shows, from the same code as main.c */
        static uint16_t dash[W * H];
        uint16_t *fb = (uint16_t *)s_fb;
        memcpy(dash, fb, sizeof dash);
        if (boot_ms < BOOT_IN_MS + BOOT_HOLD_MS) {
            memset(fb, 0, sizeof dash);
            boot_draw_logo(fb, boot_in_level(boot_ms));
        } else {
            boot_draw_cross(fb, dash,
                            boot_fade_level(boot_ms - BOOT_IN_MS - BOOT_HOLD_MS));
        }
    }

    return write_ppm(argv[1]) == 0 ? 0 : 1;
}
