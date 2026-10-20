# The 1.85"'s updater (../../notstock-updater185: the access point, the
# page, the upload into ota_0) with this board's screen: lcd_amoled.c, the
# CO5300 brought up as ../../notstock-round-amoled/main/hw_amoled.c does.
set(UPD "${CMAKE_CURRENT_LIST_DIR}/../../notstock-updater185/main")
set(AMOLED "${CMAKE_CURRENT_LIST_DIR}/../../notstock-round-amoled/main")
idf_component_register(
    SRCS "${UPD}/main.c" "lcd_amoled.c"
    INCLUDE_DIRS "." "${UPD}"
    PRIV_INCLUDE_DIRS "${AMOLED}"
    REQUIRES app_update bootloader_support driver esp_event esp_http_server
             esp_lcd esp_netif esp_timer esp_wifi lwip nvs_flash)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    UPD_SSID="NOTSTOCK-175" UPD_GAUGE="notstock_round_amoled" UPD_BOARD="1.75")
