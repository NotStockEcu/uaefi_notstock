/* host stub: no flash, settings always come up as defaults */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef int nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;
static inline esp_err_t nvs_open(const char *n, nvs_open_mode_t m, nvs_handle_t *h) { (void)n; (void)m; (void)h; return ESP_FAIL; }
static inline esp_err_t nvs_get_u8(nvs_handle_t h, const char *k, uint8_t *v) { (void)h; (void)k; (void)v; return ESP_FAIL; }
static inline esp_err_t nvs_get_blob(nvs_handle_t h, const char *k, void *v, size_t *l) { (void)h; (void)k; (void)v; (void)l; return ESP_FAIL; }
static inline esp_err_t nvs_set_u8(nvs_handle_t h, const char *k, uint8_t v) { (void)h; (void)k; (void)v; return ESP_OK; }
static inline esp_err_t nvs_set_blob(nvs_handle_t h, const char *k, const void *v, size_t l) { (void)h; (void)k; (void)v; (void)l; return ESP_OK; }
static inline esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }
static inline void nvs_close(nvs_handle_t h) { (void)h; }
