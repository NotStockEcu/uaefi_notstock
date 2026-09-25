/* GT911 capacitive touch, wired to the same I2C bus as the CH422G.
 *
 * Pin map from the Waveshare ESP32-S3-Touch-LCD-7 documentation:
 *   SDA GPIO8, SCL GPIO9, INT GPIO4, RST on CH422G EXIO1, address 0x5D.
 *
 * The address is latched at reset: INT held low while RST is released gives
 * 0x5D, INT high gives 0x14. This driver drives INT low, so it expects 0x5D,
 * and falls back to probing 0x14 if that does not answer.
 */
#include "touch.h"
#include "board.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch";

#define GT_ADDR_A 0x5D
#define GT_ADDR_B 0x14

#define REG_STATUS  0x814E
#define REG_POINT1  0x814F
#define REG_PID     0x8140
#define REG_CFG_XMAX 0x8048     /* config: X output max, little endian */
#define REG_CFG_YMAX 0x804A
#define REG_COMMAND  0x8040     /* 0 = normal coordinate reading */
#define REG_CFG_VER  0x8047

static uint8_t s_addr = GT_ADDR_A;
static bool s_ok = false;

/* What the controller thinks its resolution is. These boards ship in 800x480
 * and 1024x600 flavours and the touch controller carries its own config, so
 * it can hand back coordinates on a different grid than the panel actually
 * has. Reading it and scaling is the only safe thing to do. */
static uint16_t s_native_w = LCD_H_RES, s_native_h = LCD_V_RES;

/* Diagnostics. Until a first point arrives, the raw buffer-status byte is
 * logged once a second. That separates the two failure modes: a status that
 * stays 0x00 means the controller sees nothing, a status with bit 7 set but
 * no usable coordinates means the read or the decode is wrong. */
/* exio_set lives in main.c; declared here to avoid a header just for it */
void exio_set(uint8_t mask, bool on);

static bool s_seen_point = false;
static int64_t s_last_diag = 0;

static esp_err_t gt_read(uint16_t reg, uint8_t *buf, size_t len)
{
    uint8_t a[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    return i2c_master_write_read_device(I2C_NUM_0, s_addr, a, 2, buf, len,
                                        pdMS_TO_TICKS(50));
}

static esp_err_t gt_write8(uint16_t reg, uint8_t v)
{
    uint8_t b[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), v };
    return i2c_master_write_to_device(I2C_NUM_0, s_addr, b, 3,
                                      pdMS_TO_TICKS(50));
}

static void gt_reset(void)
{
    /* Timing matters more than it looks. The GT911 latches its I2C address
     * from the INT level when reset is released, and it wants INT handed back
     * as an input within a few tens of milliseconds. Holding INT low for much
     * longer can leave the chip in a state where it answers on I2C and reports
     * a valid config but never scans, which looks exactly like a dead panel.
     * These delays follow Waveshare's own driver. */
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_TP_INT,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    gpio_set_level(PIN_TP_INT, 0);      /* low across the release = 0x5D */
    exio_set(EXIO_TP_RST, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    exio_set(EXIO_TP_RST, true);
    vTaskDelay(pdMS_TO_TICKS(10));

    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static bool gt_probe(uint8_t *id)
{
    s_addr = GT_ADDR_A;
    esp_err_t a = gt_read(REG_PID, id, 4);
    if (a == ESP_OK) return true;
    ESP_LOGW(TAG, "no answer at 0x%02X (%s), trying 0x%02X",
             GT_ADDR_A, esp_err_to_name(a), GT_ADDR_B);
    s_addr = GT_ADDR_B;
    return gt_read(REG_PID, id, 4) == ESP_OK;
}

bool touch_init(void)
{
    uint8_t id[4] = { 0 };

    gt_reset();
    if (!gt_probe(id)) {
        ESP_LOGE(TAG, "GT911 not responding on 0x5D or 0x14. Check the i2c "
                      "scan line above: if neither address is listed the "
                      "touch flex is not seated.");
        return false;
    }

    /* Put it into plain coordinate mode and clear any latched buffer state.
     * A stale status byte is never cleared by the read path, because that
     * only writes back once bit 7 has been seen set. */
    gt_write8(REG_COMMAND, 0x00);
    gt_write8(REG_STATUS, 0x00);

    uint8_t cfg[8] = { 0 };
    if (gt_read(REG_CFG_VER, cfg, 8) == ESP_OK) {
        uint16_t w = (uint16_t)(cfg[1] | (cfg[2] << 8));
        uint16_t h = (uint16_t)(cfg[3] | (cfg[4] << 8));
        if (w >= 200 && w <= 4096 && h >= 200 && h <= 4096) {
            s_native_w = w;
            s_native_h = h;
        } else {
            ESP_LOGW(TAG, "config reports %ux%u, ignoring it", w, h);
        }
        ESP_LOGI(TAG, "GT911 config ver 0x%02X, %u touch points, "
                      "flags 0x%02X 0x%02X",
                 cfg[0], cfg[5] & 0x0F, cfg[6], cfg[7]);
    }

    s_ok = true;
    ESP_LOGI(TAG, "GT911 at 0x%02X, id %c%c%c%c, controller grid %ux%u, "
                  "panel %dx%d%s",
             s_addr, id[0], id[1], id[2], id[3],
             s_native_w, s_native_h, LCD_H_RES, LCD_V_RES,
             (s_native_w != LCD_H_RES || s_native_h != LCD_V_RES)
                 ? "  -> scaling" : "");
    return true;
}

/* One automatic retry. If the controller has not produced a single point a
 * few seconds after boot, reset it again with the same tightened timing. It
 * costs nothing when touch is working and recovers the case where the first
 * reset landed while the panel rail was still settling. */
static void maybe_retry(void)
{
    static int tries = 0;
    static int64_t t0 = 0;
    if (s_seen_point || tries >= 2) return;
    int64_t now = esp_timer_get_time();
    if (t0 == 0) { t0 = now; return; }
    if (now - t0 < 4000000) return;
    t0 = now;
    tries++;
    ESP_LOGW(TAG, "no points after %d s, resetting the controller (try %d)",
             (int)(now / 1000000), tries);
    gt_reset();
    uint8_t id[4];
    if (gt_probe(id)) {
        gt_write8(REG_COMMAND, 0x00);
        gt_write8(REG_STATUS, 0x00);
        ESP_LOGI(TAG, "controller back at 0x%02X", s_addr);
    }
}

static uint16_t scale(uint32_t v, uint16_t from, uint16_t to)
{
    if (from == 0) return 0;
    uint32_t r = v * to / from;
    return (uint16_t)(r >= to ? to - 1 : r);
}

bool touch_read(uint16_t *x, uint16_t *y)
{
    if (!s_ok) return false;

    uint8_t st = 0;
    esp_err_t e = gt_read(REG_STATUS, &st, 1);

    if (!s_seen_point) {
        maybe_retry();
        int64_t now = esp_timer_get_time();
        if (now - s_last_diag > 1000000) {
            s_last_diag = now;
            ESP_LOGI(TAG, "no touch yet: status read %s, raw 0x%02X",
                     esp_err_to_name(e), st);
        }
    }

    if (e != ESP_OK) return false;
    if (!(st & 0x80)) return false;             /* no new data */

    int n = st & 0x0F;
    bool got = false;
    if (n > 0) {
        uint8_t p[8];
        if (gt_read(REG_POINT1, p, 8) == ESP_OK) {
            uint16_t rx = (uint16_t)(p[1] | (p[2] << 8));
            uint16_t ry = (uint16_t)(p[3] | (p[4] << 8));
            /* Scale rather than reject. The old code dropped anything at or
             * beyond 800x480, which on a controller configured for 1024x600
             * threw away the entire right and bottom of the screen, including
             * the corner the settings menu lives in. */
            *x = scale(rx, s_native_w, LCD_H_RES);
            *y = scale(ry, s_native_h, LCD_V_RES);
            got = true;
            if (!s_seen_point) {
                s_seen_point = true;
                ESP_LOGI(TAG, "first point: raw %u,%u -> %u,%u", rx, ry,
                         *x, *y);
            }
        }
    }
    gt_write8(REG_STATUS, 0);                   /* ack, or it never updates */
    return got;
}


static void indev_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    static uint16_t lx = 0, ly = 0;
    static bool was_down = false;
    uint16_t x, y;
    if (touch_read(&x, &y)) {
        lx = x;
        ly = y;
        if (!was_down) {
            was_down = true;
            /* one line per press, not per frame: enough to prove the panel
             * is alive and to see where it thinks you touched */
            ESP_LOGI(TAG, "touch %u,%u", lx, ly);
        }
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        was_down = false;
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point.x = lx;
    data->point.y = ly;
}

void touch_register_lvgl(void)
{
    static lv_indev_drv_t drv;
    lv_indev_drv_init(&drv);
    drv.type = LV_INDEV_TYPE_POINTER;
    drv.read_cb = indev_read_cb;
    lv_indev_drv_register(&drv);
}
