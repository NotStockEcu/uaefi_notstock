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
 *   hold=N         LOG held at the end of the run, cursor on point N
 *                  (0 oldest .. 299 newest)
 *   look=0..3      NOTSTOCK / EMO / LONK / HILL
 *   switch=a,b,..  after the run, switch to these looks in turn (as SAVE
 *                  in the menu would) and render the last one
 *   proto=1        OBD-II: the test screen, fed by a fake VW T5 engine ECU
 *                  below through the real obd2.c (ecu=0: nobody answers)
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
#include "obd2.h"
#include "sniff.h"
void ui_log_sim_hold(int point);

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

/* ------------------------------------------------ fake OBD-II engine ECU */
/* Roughly what a T5.1 CAAC might offer: MAP on 0x0B but not 0x87, no oil
 * temperature, EGT on 0x78 (a two-frame ISO-TP answer). The gearbox (7E9)
 * answers the functional scan too, and first, to exercise the ECU pick. */
static int s_fake_ecu = 1;
static struct { uint32_t id; uint8_t d[8]; } s_q[32];
static int s_qn;

static void q_push(uint32_t id, const uint8_t *d)
{
    if (s_qn < 32) {
        s_q[s_qn].id = id;
        memcpy(s_q[s_qn].d, d, 8);
        s_qn++;
    }
}

static void sf(uint32_t id, int n, const uint8_t *payload)
{
    uint8_t d[8] = { (uint8_t)n, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
    memcpy(d + 1, payload, n);
    q_push(id, d);
}

bool obd_send(uint32_t id, const uint8_t d[8])
{
    if (!s_fake_ecu) return true;
    if (id == 0x7E0 && d[0] == 0x30) {          /* flow control for PID 78 */
        const uint8_t cf[8] = { 0x21, 0x0F, 0xC8, 0x10, 0x00, 0x00, 0xAA, 0xAA };
        q_push(0x7E8, cf);
        return true;
    }
    if ((id != 0x7DF && id != 0x7E0) || d[1] != 0x01) return true;
    uint8_t pid = d[2];
    if (id == 0x7DF && pid == 0x00) {           /* gearbox is quicker */
        const uint8_t p[] = { 0x41, 0x00, 0x18, 0x00, 0x00, 0x00 };
        sf(0x7E9, 6, p);
    }
    uint8_t p[8] = { 0x41, pid };
    switch (pid) {
    case 0x00: p[2]=0x18; p[3]=0x3F; p[4]=0x80; p[5]=0x13; sf(0x7E8, 6, p); break;
    case 0x20: p[2]=0x80; p[3]=0x05; p[4]=0xA0; p[5]=0x01; sf(0x7E8, 6, p); break;
    case 0x40: p[2]=0xC0; p[3]=0x80; p[4]=0x00; p[5]=0x01; sf(0x7E8, 6, p); break;
    case 0x60: p[2]=0x00; p[3]=0x00; p[4]=0x01; p[5]=0x00; sf(0x7E8, 6, p); break;
    case 0x05: p[2]=86+40;  sf(0x7E8, 3, p); break;
    case 0x0F: p[2]=29+40;  sf(0x7E8, 3, p); break;
    case 0x0B: p[2]=178;    sf(0x7E8, 3, p); break;
    case 0x33: p[2]=99;     sf(0x7E8, 3, p); break;
    case 0x0D: p[2]=62;     sf(0x7E8, 3, p); break;
    case 0x0C: p[2]=(1850*4)>>8; p[3]=(1850*4)&0xFF; sf(0x7E8, 4, p); break;
    case 0x78: {                                /* 11 bytes: first frame */
        const uint8_t ff[8] = { 0x10, 0x0B, 0x41, 0x78, 0x03, 0x12, 0x10, 0x10 };
        q_push(0x7E8, ff);
        break;
    }
    default: {                                  /* not supported: refuse */
        const uint8_t n[] = { 0x7F, 0x01, 0x12 };
        sf(0x7E8, 3, n);
        break;
    }
    }
    return true;
}
/* ------------------------------------------------- fake VCDS for SNIFF */
/* What VCDS reading two engine values and one cluster value might look
 * like: single and multi-frame 0x62 answers, one refusal. */
static void fake_tester(int step)
{
    static const struct { uint32_t id; uint8_t d[8]; } T[] = {
        { 0x7E0, { 0x03, 0x22, 0xF4, 0x0C, 0x55, 0x55, 0x55, 0x55 } },
        { 0x7E8, { 0x05, 0x62, 0xF4, 0x0C, 0x0D, 0x70, 0xAA, 0xAA } },
        { 0x7E0, { 0x03, 0x22, 0x11, 0xBD, 0x55, 0x55, 0x55, 0x55 } },
        { 0x7E8, { 0x10, 0x09, 0x62, 0x11, 0xBD, 0x0B, 0x9A, 0x0C } },
        { 0x7E0, { 0x30, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55, 0x55 } },
        { 0x7E8, { 0x21, 0x40, 0x0A, 0xF1, 0xAA, 0xAA, 0xAA, 0xAA } },
        { 0x714, { 0x03, 0x22, 0x22, 0x03, 0x55, 0x55, 0x55, 0x55 } },
        { 0x77E, { 0x05, 0x62, 0x22, 0x03, 0x00, 0x7B, 0xAA, 0xAA } },
        { 0x7E0, { 0x03, 0x22, 0x12, 0x34, 0x55, 0x55, 0x55, 0x55 } },
        { 0x7E8, { 0x03, 0x7F, 0x22, 0x31, 0xAA, 0xAA, 0xAA, 0xAA } },
    };
    const int n = sizeof T / sizeof T[0];
    int i = step % n;
    uint8_t d[8];
    memcpy(d, T[i].d, 8);
    if (T[i].id == 0x77E) d[5] = (uint8_t)(0x70 + step / n % 16); /* warming */
    sniff_frame(T[i].id, d, 8, s_now_us);
    g_dash.last_rx_us = s_now_us;
}

bool rusefi_can_link_ok(void)
{
    if (g_set.protocol) {
        return g_dash.last_rx_us && s_now_us - g_dash.last_rx_us < 1500000;
    }
    return s_link != 0;
}
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
        { "map",   &g_dash.map },
    };
    float peak[3] = { NAN, NAN, NAN };
    volatile float *peak_field[3] = { &g_dash.clt, &g_dash.iat, &g_dash.boost };
    const char *peak_name[3] = { "peak_clt", "peak_iat", "peak_boost" };
    int night = 0, area = 0, colour = 0, look = 0, proto = 0;
    bool menu = false, logscr = false;
    int hold = -1;
    const char *sw = NULL;
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
        if (strcmp(k, "hold") == 0)   { hold = atoi(v); used = true; }
        if (strcmp(k, "night") == 0)  { night = atoi(v); used = true; }
        if (strcmp(k, "proto") == 0)  { proto = atoi(v); used = true; }
        if (strcmp(k, "ecu") == 0)    { s_fake_ecu = atoi(v); used = true; }
        if (strcmp(k, "look") == 0)   { look = atoi(v); used = true; }
        if (strcmp(k, "switch") == 0) { sw = v; used = true; }
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
    g_set.look = (uint8_t)look;
    g_set.protocol = (uint8_t)proto;
    if (proto == PROTO_SNIFF) {
        sniff_reset();
        g_dash.last_rx_us = 0;
    } else if (proto) {
        obd_reset();
        g_dash.last_rx_us = 0;
    }
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
        if (proto == PROTO_SNIFF) {
            for (int k = 0; k < STEP_MS / 5; k++) {
                s_now_us += 5000;
                static int step;
                fake_tester(step++);
            }
        } else if (proto) {
            /* the CAN task runs every 5 ms on the panel */
            for (int k = 0; k < STEP_MS / 5; k++) {
                s_now_us += 5000;
                for (int j = 0; j < s_qn; j++) {
                    obd_frame(s_q[j].id, s_q[j].d, 8, s_now_us);
                }
                s_qn = 0;
                obd_tick(s_now_us);
            }
        } else {
            s_now_us += STEP_MS * 1000;
        }
        lv_tick_inc(STEP_MS);
        lv_timer_handler();
    }
    for (const char *p = sw; p && *p; ) {
        g_set.look = (uint8_t)atoi(p);
        lv_scr_load(ui_menu_screen());       /* switching happens from the menu */
        ui_show_dash();
        for (int i = 0; i < 25; i++) {
            s_now_us += STEP_MS * 1000;
            lv_tick_inc(STEP_MS);
            lv_timer_handler();
        }
        p = strchr(p, ',');
        if (p) p++;
    }

    if (hold >= 0) {
        ui_log_sim_hold(hold);
        for (int i = 0; i < 10; i++) {       /* let it run on while held */
            s_now_us += STEP_MS * 1000;
            lv_tick_inc(STEP_MS);
            lv_timer_handler();
        }
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
