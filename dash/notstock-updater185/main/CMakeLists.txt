# The panel's driver and pins from the gauge's own project
set(LCD "${CMAKE_CURRENT_LIST_DIR}/../../notstock-round-lcd185/main")
idf_component_register(
    SRCS "main.c" "lcd_text.c"
         "${LCD}/st77916/esp_lcd_st77916.c" "${LCD}/st77916/st77916_init_new.c"
    INCLUDE_DIRS "." "${LCD}/st77916"
    PRIV_INCLUDE_DIRS "${LCD}"
    REQUIRES app_update bootloader_support driver esp_event esp_http_server
             esp_lcd esp_netif esp_timer esp_wifi lwip nvs_flash)
