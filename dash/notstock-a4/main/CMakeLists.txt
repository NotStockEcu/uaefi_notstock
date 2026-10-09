# The A4 gauge on the Waveshare ESP32-S3-Touch-AMOLED-1.32: its own dials
# (ui_a4.c, faces, fonts), the round gauge's OBD-II client and CAN code
# (../notstock-round, which also gives the boot logo, the chimes and the
# rnd_data_t these pass around), the AMOLED 1.75's board driver with the
# 1.32's pins (board_a132.h), and the 7" dash's OBD-II client.
set(ROUND "${CMAKE_CURRENT_LIST_DIR}/../../notstock-round/main")
set(AMOLED "${CMAKE_CURRENT_LIST_DIR}/../../notstock-round-amoled/main")
set(DASH7 "${CMAKE_CURRENT_LIST_DIR}/../../notstock-dash-7inch/main")
file(GLOB SRCS "*.c" "faces/*.c" "fonts/*.c")
list(APPEND SRCS
    "${AMOLED}/hw_amoled.c"
    "${ROUND}/can_obd.c" "${ROUND}/boot_fb.c" "${ROUND}/splash.c"
    "${ROUND}/sounds.c" "${ROUND}/motion.c" "${ROUND}/gmeter.c"
    "${DASH7}/obd2.c" "${DASH7}/dtc_text.c")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "." "${ROUND}" "${DASH7}")

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    RND_SIZE=466 BOARD_A132 LV_LVGL_H_INCLUDE_SIMPLE)
