# The round gauge of ../notstock-round, unchanged, on the Waveshare
# ESP32-S3-Touch-AMOLED-1.75: its UI, fonts, settings and CAN code, the
# faces drawn for 466 x 466 (faces466), and this board's own hw.c in place
# of the 2.1" LCD's. Plus the OBD client and the trouble code texts from the
# 7" dash, as there.
set(ROUND "${CMAKE_CURRENT_LIST_DIR}/../../notstock-round/main")
set(DASH7 "${CMAKE_CURRENT_LIST_DIR}/../../notstock-dash-7inch/main")
file(GLOB SRCS "${ROUND}/*.c" "${ROUND}/fonts/*.c" "${ROUND}/faces466/*.c")
list(REMOVE_ITEM SRCS "${ROUND}/hw.c")
list(APPEND SRCS "hw_amoled.c" "${DASH7}/obd2.c" "${DASH7}/dtc_text.c")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "." "${ROUND}" "${ROUND}/faces466" "${DASH7}")

# 466 x 466 panel; board_amoled.h instead of board_round.h; the generated
# font and image files pick their lvgl include path from the last one
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    RND_SIZE=466 BOARD_AMOLED LV_LVGL_H_INCLUDE_SIMPLE)
