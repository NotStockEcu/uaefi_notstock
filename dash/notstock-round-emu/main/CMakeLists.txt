# The EMU page and its CAN stream decoder, plus from ../notstock-round the
# 2.1" board's drivers (hw.c), the boot logo (splash.c, boot_fb.c).
set(ROUND "${CMAKE_CURRENT_LIST_DIR}/../../notstock-round/main")
file(GLOB SRCS "${CMAKE_CURRENT_LIST_DIR}/*.c" "${CMAKE_CURRENT_LIST_DIR}/fonts/*.c")
list(APPEND SRCS "${ROUND}/hw.c" "${ROUND}/boot_fb.c" "${ROUND}/splash.c")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "." "${ROUND}")

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
