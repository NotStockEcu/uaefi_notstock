/* G-meter: the board's QMI8658 at 0x6B (on the 2.1" LCD, the 1.85" LCD
 * and the 1.75" AMOLED alike), +-4 g, ~125 Hz, read at 50 Hz on the
 * board's I2C through hw_i2c_* (each board's hw file), turned into the
 * car's frame by gmeter.c. No QMI8658: the G-meter says NO SENSOR.
 *
 * Zero (the car standing, the G-meter's double tap): averages 1 s, gives
 * "up". Forward is learnt while driving from the OBD speed (g_dash.speed):
 * once a second the change of speed and the reading averaged over that
 * second. The calibration is kept in NVS.
 *
 */
#include "gmeter.h"
#include "hw.h"
#include "rusefi_can.h"

#include <math.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"


static const char *TAG = "gmeter";

#define QMI_ADDR      0x6B
#define QMI_LSB_PER_G 8192.0f         /* +-4 g */
#define SAMPLE_MS     20
#define LEARN_EVERY   50              /* samples: 1 s */
#define ZERO_SAMPLES  50
#define LEARN_KMH     15.0f           /* learn forward only above this */

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static gm_cal_t s_cal;
static bool s_imu_ok;
static float s_lon, s_lat;
static volatile bool s_zero_req;

/* -------------------------------------------------------------- NVS */
static void cal_load(void)
{
    nvs_handle_t h;
    size_t n = sizeof s_cal;
    gm_cal_t c;
    if (nvs_open("motion", NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_blob(h, "gmcal", &c, &n) == ESP_OK && n == sizeof c &&
            c.version == GM_CAL_VERSION) {
            s_cal = c;
            ESP_LOGI(TAG, "calibration loaded (zeroed %d, learnt %u)",
                     c.zeroed, c.learnt);
        }
        nvs_close(h);
    }
}

static void cal_save(void)
{
    nvs_handle_t h;
    if (nvs_open("motion", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, "gmcal", &s_cal, sizeof s_cal);
        nvs_commit(h);
        nvs_close(h);
    }
}

/* -------------------------------------------------------------- IMU */
static bool qmi_reg(uint8_t r, uint8_t v)
{
    uint8_t d[2] = { r, v };
    return hw_i2c_write(QMI_ADDR, d, 2) == ESP_OK;
}

static bool qmi_init(void)
{
    uint8_t r = 0x00, id = 0;
    if (hw_i2c_write_read(QMI_ADDR, &r, 1, &id, 1) != ESP_OK || id != 0x05) {
        ESP_LOGW(TAG, "QMI8658 not found (id 0x%02X)", id);
        return false;
    }
    return qmi_reg(0x02, 0x40) &&           /* CTRL1: address auto-increment */
           qmi_reg(0x03, 0x16) &&           /* CTRL2: +-4 g, ~125 Hz */
           qmi_reg(0x08, 0x01);             /* CTRL7: accelerometer on */
}

static bool qmi_read(float a[3])
{
    uint8_t r = 0x35, d[6];
    if (hw_i2c_write_read(QMI_ADDR, &r, 1, d, 6) != ESP_OK) return false;
    for (int i = 0; i < 3; i++) {
        a[i] = (int16_t)(d[2 * i] | (d[2 * i + 1] << 8)) / QMI_LSB_PER_G;
    }
    return true;
}

/* ------------------------------------------------------------- task */
static void motion_task(void *arg)
{
    (void)arg;
    float lp[3] = { 0 }, acc[3] = { 0 }, zsum[3] = { 0 };
    int nacc = 0, nzero = 0;
    float v_prev = NAN;
    int64_t t_prev = 0;
    bool first = true;

    for (;;) {
        float a[3];
        if (qmi_read(a)) {
            for (int i = 0; i < 3; i++) {
                lp[i] = first ? a[i] : lp[i] + (a[i] - lp[i]) * 0.25f;
                acc[i] += a[i];
            }
            first = false;
            nacc++;
            if (s_zero_req) {
                for (int i = 0; i < 3; i++) zsum[i] += a[i];
                if (++nzero >= ZERO_SAMPLES) {
                    float m[3] = { zsum[0] / nzero, zsum[1] / nzero, zsum[2] / nzero };
                    portENTER_CRITICAL(&s_mux);
                    gm_zero(&s_cal, m);
                    portEXIT_CRITICAL(&s_mux);
                    cal_save();
                    ESP_LOGI(TAG, "zeroed: up %.2f %.2f %.2f",
                             s_cal.up[0], s_cal.up[1], s_cal.up[2]);
                    memset(zsum, 0, sizeof zsum);
                    nzero = 0;
                    s_zero_req = false;
                }
            }
            float lon, lat;
            portENTER_CRITICAL(&s_mux);
            gm_project(&s_cal, lp, &lon, &lat);
            s_lon = lon;
            s_lat = lat;
            portEXIT_CRITICAL(&s_mux);
        }

        /* once a second: the change of the OBD speed against the reading
         * averaged over that second teaches which way is forward */
        if (nacc >= LEARN_EVERY) {
            int64_t now = esp_timer_get_time();
            float v = g_dash.speed;
            float dt = (now - t_prev) / 1e6f;
            if (!isnan(v) && !isnan(v_prev) && dt > 0.5f && dt < 2.5f &&
                v > LEARN_KMH && v_prev > LEARN_KMH) {
                float m[3] = { acc[0] / nacc, acc[1] / nacc, acc[2] / nacc };
                float dvdt = (v - v_prev) / 3.6f / dt;
                portENTER_CRITICAL(&s_mux);
                bool moved = gm_learn(&s_cal, m, dvdt);
                uint16_t n = s_cal.learnt;
                portEXIT_CRITICAL(&s_mux);
                if (moved && n % 20 == 0) cal_save();
            }
            v_prev = v;
            t_prev = now;
            memset(acc, 0, sizeof acc);
            nacc = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_MS));
    }
}

void motion_start(void)
{
    gm_cal_default(&s_cal);
    cal_load();
    s_imu_ok = qmi_init();
    ESP_LOGI(TAG, "QMI8658 %s", s_imu_ok ? "ready" : "missing");
    if (s_imu_ok) xTaskCreatePinnedToCore(motion_task, "gmeter", 4096, NULL, 4, NULL, 0);
}

/* ------------------------------------------------------------ for the UI */
void hw_motion_fill(rnd_data_t *d)
{
    portENTER_CRITICAL(&s_mux);
    d->g.present = s_imu_ok;
    d->g.zeroed = s_cal.zeroed;
    d->g.learnt = s_cal.learnt >= 6;
    d->g.lon_g = s_lon;
    d->g.lat_g = s_lat;
    portEXIT_CRITICAL(&s_mux);
}

void hw_g_zero(void)
{
    s_zero_req = true;
}
