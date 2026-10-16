/* NOT STOCK updater for the round gauge on the ESP32-S3-Touch-LCD-1.85:
 * a small app in the factory partition that puts a new gauge firmware
 * into ota_0 over Wi-Fi, so the gauge need not come out of the vent.
 *
 * The gauge (ota_0) starts it from its menu (AKTUALIZACE): it sets "upd"
 * in NVS and boots the factory partition. Here then: an access point
 * NOTSTOCK-185 (password notstock), a page at http://192.168.4.1 (any name
 * the phone asks for answers there, so it pops up as a sign-in page), the
 * .bin uploaded from it straight into ota_0, checked to be the gauge's own,
 * and the gauge booted again. ZRUŠIT on the page, or 15 minutes without a
 * request, goes back to the gauge as it was.
 *
 * Without "upd" (the first start after flashing over USB, which leaves the
 * boot choice blank) it boots the gauge at once; with no gauge in ota_0 it
 * stays, waiting for one. An upload broken off half way leaves ota_0
 * unusable, the boot choice still here: the next start comes here again.
 */
#include "lcd_text.h"

#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "upd";

#define AP_SSID   "NOTSTOCK-185"
#define AP_PASS   "notstock"
#define AP_IP     "192.168.4.1"
#define GAUGE     "notstock_round_lcd185"   /* the gauge's project name */
#define IDLE_MIN  15

static const esp_partition_t *s_ota0;
static SemaphoreHandle_t s_lcd;
static volatile bool s_busy;               /* an upload is going */
static esp_timer_handle_t s_idle;

/* ------------------------------------------------------------- the flag */
static bool flag_get(void)
{
    nvs_handle_t h;
    uint8_t v = 0;
    if (nvs_open("ota", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, "upd", &v);
        nvs_close(h);
    }
    return v != 0;
}

static void flag_clear(void)
{
    nvs_handle_t h;
    if (nvs_open("ota", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, "upd");
        nvs_commit(h);
        nvs_close(h);
    }
}

static bool gauge_ok(esp_app_desc_t *d)
{
    esp_app_desc_t tmp;
    if (!d) d = &tmp;
    return s_ota0 && esp_ota_get_partition_description(s_ota0, d) == ESP_OK;
}

/* back to the gauge: it boots next */
static void to_gauge(void)
{
    flag_clear();
    esp_ota_set_boot_partition(s_ota0);
    vTaskDelay(pdMS_TO_TICKS(1500));       /* the answer gets out */
    esp_restart();
}

/* --------------------------------------------------------------- screen */
static char s_installed[64];

static void screen(const char *status, uint16_t color, int percent)
{
    xSemaphoreTake(s_lcd, portMAX_DELAY);
    lcd_clear();
    lcd_text(48, true, C_ORANGE, "AKTUALIZACE");
    lcd_text(104, false, C_GREY, "WiFi");
    lcd_text(132, true, C_WHITE, AP_SSID);
    lcd_text(182, false, C_GREY, "heslo  " AP_PASS);
    lcd_text(214, true, C_WHITE, AP_IP);
    lcd_text(262, false, color, status);
    if (percent >= 0) lcd_bar(298, percent, C_GREEN);
    else lcd_text(292, false, C_GREY, s_installed);
    lcd_show();
    xSemaphoreGive(s_lcd);
}

/* ---------------------------------------------------------------- pages */
static const char PAGE[] =
    "<!doctype html><html lang=cs><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>NOT STOCK: aktualizace</title><style>"
    "body{font-family:sans-serif;background:#111;color:#eee;max-width:460px;margin:auto;padding:18px}"
    "h1{color:#ff9a1f;font-size:22px}input,button{font-size:18px;width:100%%;margin:8px 0;padding:10px}"
    "button{background:#ff9a1f;border:0;border-radius:8px;color:#111;font-weight:bold}"
    "#z{background:#333;color:#eee}progress{width:100%%;height:22px}"
    "</style></head><body><h1>NOT STOCK 1.85: aktualizace</h1>"
    "<p>V budíku je: <b>%s</b></p>"
    "<p>Vyber soubor <b>notstock_round_lcd185.bin</b> a nahraj ho.</p>"
    "<input type=file id=f accept='.bin'><button onclick='up()'>Nahrát</button>"
    "<progress id=p max=100 value=0></progress><p id=s></p>"
    "<button id=z onclick='zpet()'>Zrušit, zpět do budíku</button>"
    "<script>"
    "function up(){var f=document.getElementById('f').files[0];if(!f){s.textContent='Nejdřív vyber soubor.';return}"
    "var x=new XMLHttpRequest();x.open('POST','/update');"
    "x.upload.onprogress=function(e){p.value=e.loaded*100/e.total};"
    "x.onload=function(){s.textContent=x.responseText};"
    "x.onerror=function(){s.textContent='Spojení se přerušilo. Zkus to znovu.'};"
    "s.textContent='Nahrávám, nevypínej budík…';x.send(f)}"
    "function zpet(){fetch('/cancel',{method:'POST'}).then(function(r){return r.text()})"
    ".then(function(t){s.textContent=t})}"
    "</script></body></html>";

static void idle_restart(void)
{
    esp_timer_stop(s_idle);
    esp_timer_start_once(s_idle, (uint64_t)IDLE_MIN * 60 * 1000000);
}

static esp_err_t page_get(httpd_req_t *r)
{
    idle_restart();
    static char buf[sizeof PAGE + 96];
    snprintf(buf, sizeof buf, PAGE, s_installed);
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    return httpd_resp_send(r, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t answer(httpd_req_t *r, const char *status, const char *text)
{
    httpd_resp_set_status(r, status);
    httpd_resp_set_type(r, "text/plain; charset=utf-8");
    return httpd_resp_send(r, text, HTTPD_RESP_USE_STRLEN);
}

/* the file straight into ota_0; the first 4 kB looked at first: the
 * image's magic, and the app description's project name */
static esp_err_t update_post(httpd_req_t *r)
{
    idle_restart();
    int total = r->content_len;
    if (total < 4096 || total > (int)s_ota0->size) {
        return answer(r, "400 Bad Request", "To není firmware budíku (velikost nesedí).");
    }
    static char buf[4096];
    int head = 0;
    while (head < (int)sizeof buf) {
        int n = httpd_req_recv(r, buf + head, sizeof buf - head);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) return ESP_FAIL;
        head += n;
    }
    const size_t at = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    const esp_app_desc_t *d = (const esp_app_desc_t *)(buf + at);
    if ((uint8_t)buf[0] != ESP_IMAGE_HEADER_MAGIC || d->magic_word != ESP_APP_DESC_MAGIC_WORD) {
        return answer(r, "400 Bad Request", "To není firmware pro ESP32 (.bin budíku).");
    }
    if (strncmp(d->project_name, GAUGE, sizeof d->project_name) != 0) {
        static char msg[128];
        snprintf(msg, sizeof msg, "Tohle je firmware %.32s, ne budíku 1.85.", d->project_name);
        return answer(r, "400 Bad Request", msg);
    }

    s_busy = true;
    ESP_LOGI(TAG, "uploading %s %s, %d bytes", d->project_name, d->version, total);
    screen("NAHRÁVÁM", C_WHITE, 0);
    esp_ota_handle_t h;
    esp_err_t e = esp_ota_begin(s_ota0, OTA_WITH_SEQUENTIAL_WRITES, &h);
    if (e == ESP_OK) e = esp_ota_write(h, buf, head);
    int got = head, shown = 0;
    while (e == ESP_OK && got < total) {
        int n = httpd_req_recv(r, buf, MIN((int)sizeof buf, total - got));
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) {
            e = ESP_FAIL;
            break;
        }
        e = esp_ota_write(h, buf, n);
        got += n;
        int pc = (int)((int64_t)got * 100 / total);
        if (pc >= shown + 5) {
            shown = pc;
            char st[24];
            snprintf(st, sizeof st, "NAHRÁVÁM %d %%", pc);
            screen(st, C_WHITE, pc);
        }
    }
    if (e == ESP_OK) {
        e = esp_ota_end(h);                /* checks the whole image */
    } else {
        esp_ota_abort(h);
    }
    s_busy = false;
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "update failed: %s", esp_err_to_name(e));
        screen("CHYBA, ZNOVU", C_RED, -1);
        return answer(r, "500 Internal Server Error",
                      "Nahrání se nepovedlo. Budík zůstává v aktualizaci, zkus to znovu.");
    }
    screen("HOTOVO, RESTART", C_GREEN, 100);
    answer(r, "200 OK", "Hotovo. Budík se restartuje s novým firmwarem.");
    to_gauge();
    return ESP_OK;
}

static esp_err_t cancel_post(httpd_req_t *r)
{
    if (!gauge_ok(NULL)) {
        return answer(r, "409 Conflict",
                      "V budíku teď žádný firmware není: nejdřív nějaký nahraj.");
    }
    screen("ZPĚT DO BUDÍKU", C_WHITE, -1);
    answer(r, "200 OK", "Budík se restartuje beze změny.");
    to_gauge();
    return ESP_OK;
}

/* any other address (a phone checking for internet): to the page */
static esp_err_t to_page(httpd_req_t *r, httpd_err_code_t err)
{
    (void)err;
    httpd_resp_set_status(r, "302 Found");
    httpd_resp_set_hdr(r, "Location", "http://" AP_IP "/");
    return httpd_resp_send(r, NULL, 0);
}

static void http_start(void)
{
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.recv_wait_timeout = 20;
    c.lru_purge_enable = true;
    httpd_handle_t s;
    ESP_ERROR_CHECK(httpd_start(&s, &c));
    const httpd_uri_t u[] = {
        { .uri = "/", .method = HTTP_GET, .handler = page_get },
        { .uri = "/update", .method = HTTP_POST, .handler = update_post },
        { .uri = "/cancel", .method = HTTP_POST, .handler = cancel_post },
    };
    for (size_t i = 0; i < sizeof u / sizeof u[0]; i++) httpd_register_uri_handler(s, &u[i]);
    httpd_register_err_handler(s, HTTPD_404_NOT_FOUND, to_page);
}

/* ------------------------------------------------------------------ DNS */
/* every A question answered with our address */
static void dns_task(void *arg)
{
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(53),
                             .sin_addr.s_addr = htonl(INADDR_ANY) };
    bind(sock, (struct sockaddr *)&a, sizeof a);
    uint8_t q[512];
    for (;;) {
        struct sockaddr_in from;
        socklen_t fl = sizeof from;
        int n = recvfrom(sock, q, sizeof q - 16, 0, (struct sockaddr *)&from, &fl);
        if (n < 12) continue;
        int p = 12;                        /* past the question's name */
        while (p < n && q[p]) p += q[p] + 1;
        p += 5;                            /* zero, type, class */
        if (p > n) continue;
        q[2] = 0x81;                       /* a response, recursion available */
        q[3] = 0x80;
        q[6] = 0; q[7] = 1;                /* one answer */
        q[8] = q[9] = q[10] = q[11] = 0;
        const uint8_t ans[16] = { 0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4,
                                  192, 168, 4, 1 };
        memcpy(q + p, ans, sizeof ans);
        sendto(sock, q, p + sizeof ans, 0, (struct sockaddr *)&from, fl);
    }
}

/* ----------------------------------------------------------------- Wi-Fi */
static void wifi_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t ic = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&ic));
    wifi_config_t c = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = sizeof AP_SSID - 1,
            .password = AP_PASS,
            .channel = 6,
            .max_connection = 2,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &c));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void idle_cb(void *arg)
{
    (void)arg;
    if (s_busy || !gauge_ok(NULL)) return;
    ESP_LOGI(TAG, "%d minutes without a request: back to the gauge", IDLE_MIN);
    screen("ZPĚT DO BUDÍKU", C_WHITE, -1);
    to_gauge();
}

void app_main(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    s_ota0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                      ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    esp_app_desc_t d;
    bool have = gauge_ok(&d);
    if (!flag_get() && have) {
        ESP_LOGI(TAG, "not asked for: the gauge");
        esp_ota_set_boot_partition(s_ota0);
        esp_restart();
    }
    if (have) snprintf(s_installed, sizeof s_installed, "v budíku %.12s", d.version);
    else snprintf(s_installed, sizeof s_installed, "budík prázdný");

    s_lcd = xSemaphoreCreateMutex();
    lcd_init();
    screen("ČEKÁM NA SOUBOR", C_WHITE, -1);
    wifi_start();
    http_start();
    xTaskCreate(dns_task, "dns", 3072, NULL, 5, NULL);
    const esp_timer_create_args_t t = { .callback = idle_cb, .name = "idle" };
    ESP_ERROR_CHECK(esp_timer_create(&t, &s_idle));
    idle_restart();
    ESP_LOGI(TAG, "AP %s, http://%s", AP_SSID, AP_IP);
}
