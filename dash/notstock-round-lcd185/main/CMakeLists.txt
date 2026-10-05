# The round gauge of ../notstock-round, unchanged, on the Waveshare
# ESP32-S3-Touch-LCD-1.85: its UI, fonts, settings, CAN code and the faces
# drawn for 480 x 480 (the UI renders at 480 and hw_lcd185.c scales each
# frame to the panel's 360), plus this board's hw_lcd185.c and Espressif's
# ST77916 driver in place of the 2.1" LCD's hw.c. The OBD client and the
# trouble code texts from the 7" dash, as there.
set(ROUND "${CMAKE_CURRENT_LIST_DIR}/../../notstock-round/main")
set(DASH7 "${CMAKE_CURRENT_LIST_DIR}/../../notstock-dash-7inch/main")
file(GLOB SRCS "${ROUND}/*.c" "${ROUND}/fonts/*.c" "${ROUND}/faces480/*.c")
list(REMOVE_ITEM SRCS "${ROUND}/hw.c")
list(APPEND SRCS "hw_lcd185.c" "st77916/esp_lcd_st77916.c"
     "st77916/st77916_init_new.c" "${DASH7}/obd2.c" "${DASH7}/dtc_text.c")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "." "st77916" "${ROUND}" "${ROUND}/faces480" "${DASH7}")

# board_lcd185.h instead of board_round.h; the generated font and image
# files pick their lvgl include path from the second
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    BOARD_LCD185 LV_LVGL_H_INCLUDE_SIMPLE)
