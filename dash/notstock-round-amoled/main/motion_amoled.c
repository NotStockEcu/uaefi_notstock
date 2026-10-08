/* GPS and G-meter on the ESP32-S3-Touch-AMOLED-1.75(-G).
 *
 * GPS: the -G board's LC76G sits on the board's I2C (0x50 to write, 0x54
 * to read; its UART is not wired to the ESP32 unless R15/R16 are fitted,
 * so IO17/IO18 stay the CAN's). Waveshare's way of reading it: ask how
 * many NMEA bytes wait (51 AA, length 4), then ask for them. Every 250 ms.
 * Its reset is the TCA9554's EXIO7, held high (running).
 *
 * Accelerometer: the QMI8658 at 0x6B, +-4 g, 125 Hz, read at 50 Hz, turned
 * into the car's frame by gmeter.c. Zero (the car standing, from the
 * G-meter's double tap): averages 1 s. Forward is learnt from the GPS
 * speed while driving. The calibration is kept in NVS.
 *
 * Boards without the GPS: the LC76G does not answer, gps.present false,
 * the compass says so; the G-meter works on every 1.75.
 */
#include "gmeter.h"
#include "hw.h"
#include "nmea.h"

#include <math.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

esp_err_t hw_i2c_write(uint8_t addr, const uint8_t *d, size_t n);
esp_err_t hw_i2c_write_read(uint8_t addr, const uint8_t *w, size_t wn,
                            uint8_t *r, size_t rn);
esp_err_t hw_i2c_read(uint8_t addr, uint8_t *r, size_t rn);

static const char *TAG = "motion";

#define TCA9554_ADDR  0x20
#define GPS_W         0x50
#define GPS_R         0x54
#define QMI_ADDR      0x6B
#define QMI_LSB_PER_G 8192.0f         /* +-4 g */
#define SAMPLE_MS     20
#define GPS_EVERY     12              /* samples: 240 ms */
#define GPS_MAX       2048
#define ZERO_SAMPLES  50
#define LEARN_KMH     15.0f           /* learn forward only above this */

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static nmea_t s_nmea;            /* shown, under s_mux */
static nmea_t s_work;            /* parsed into, motion task only */
static gm_cal_t s_cal;
static bool s_gps_ok, s_imu_ok;
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
            ESP_LOGI(TAG, "G-meter calibration loaded (zeroed %d, learnt %u)",
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

/* -------------------------------------------------------------- GPS */
static void gps_reset_high(void)
{
    uint8_t r = 0x01, v = 0xFF;
    /* output register first, then P7 to output; the rest stay inputs */
    if (hw_i2c_write_read(TCA9554_ADDR, &r, 1, &v, 1) == ESP_OK) {
        uint8_t w[2] = { 0x01, (uint8_t)(v | 0x80) };
        hw_i2c_write(TCA9554_ADDR, w, 2);
        r = 0x03;
        if (hw_i2c_write_read(TCA9554_ADDR, &r, 1, &v, 1) == ESP_OK) {
            uint8_t c[2] = { 0x03, (uint8_t)(v & ~0x80) };
            hw_i2c_write(TCA9554_ADDR, c, 2);
        }
    }
}

static uint8_t s_buf[GPS_MAX];

/* one poll: whatever NMEA the module holds, into the parser */
static bool gps_poll(void)
{
    static const uint8_t ask_len[8] = { 0x08, 0x00, 0x51, 0xAA, 0x04, 0, 0, 0 };
    uint8_t l[4];
    if (hw_i2c_write(GPS_W, ask_len, sizeof ask_len) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
    if (hw_i2c_read(GPS_R, l, 4) != ESP_OK) return false;
    uint32_t n = l[0] | (l[1] << 8) | ((uint32_t)l[2] << 16) | ((uint32_t)l[3] << 24);
    if (n == 0) return true;
    if (n > GPS_MAX) n = GPS_MAX;
    uint8_t ask[8] = { 0x00, 0x20, 0x51, 0xAA, (uint8_t)n, (uint8_t)(n >> 8),
                       (uint8_t)(n >> 16), (uint8_t)(n >> 24) };
    if (hw_i2c_write(GPS_W, ask, sizeof ask) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
    if (hw_i2c_read(GPS_R, s_buf, n) != ESP_OK) return false;
    nmea_feed(&s_work, (const char *)s_buf, (int)n);
    portENTER_CRITICAL(&s_mux);
    s_nmea = s_work;
    portEXIT_CRITICAL(&s_mux);
    return true;
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
    int nacc = 0, nzero = 0, k = 0;
    uint32_t last_rmc = 0;
    float v_prev = NAN;
    int64_t t_prev = 0;
    bool first = true;

    for (;;) {
        float a[3];
        if (s_imu_ok && qmi_read(a)) {
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

        if (++k >= GPS_EVERY) {
            k = 0;
            bool ok = gps_poll();
            if (ok != s_gps_ok) {
                ESP_LOGI(TAG, "GPS %s", ok ? "answers" : "gone");
                s_gps_ok = ok;
            }
            /* a new RMC: a new speed, and with the reading averaged since
             * the last one, something to learn forward from */
            portENTER_CRITICAL(&s_mux);
            uint32_t rmc = s_nmea.rmc_count;
            float v = s_nmea.speed_kmh;
            portEXIT_CRITICAL(&s_mux);
            if (rmc != last_rmc) {
                last_rmc = rmc;
                int64_t now = esp_timer_get_time();
                float dt = (now - t_prev) / 1e6f;
                if (!isnan(v) && !isnan(v_prev) && dt > 0.3f && dt < 2.5f &&
                    v > LEARN_KMH && v_prev > LEARN_KMH && nacc > 0) {
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
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_MS));
    }
}

void motion_start(void)
{
    nmea_init(&s_work);
    s_nmea = s_work;
    gm_cal_default(&s_cal);
    cal_load();
    gps_reset_high();
    s_imu_ok = qmi_init();
    s_gps_ok = gps_poll();
    ESP_LOGI(TAG, "QMI8658 %s, LC76G %s", s_imu_ok ? "ready" : "missing",
             s_gps_ok ? "answers" : "does not answer");
    xTaskCreatePinnedToCore(motion_task, "motion", 4096, NULL, 4, NULL, 0);
}

/* ------------------------------------------------------------ for the UI */
void hw_motion_fill(rnd_data_t *d)
{
    portENTER_CRITICAL(&s_mux);
    d->gps.present = s_gps_ok;
    d->gps.fix = s_nmea.fix;
    d->gps.speed_kmh = s_nmea.speed_kmh;
    d->gps.course_deg = s_nmea.course_deg;
    d->gps.sats = s_nmea.sats;
    d->gps.alt_m = s_nmea.alt_m;
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
