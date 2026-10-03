# Every .c in main/ and main/fonts/, plus the OBD client and the trouble code
# texts from the 7" dash next door (../notstock-dash-7inch), shared as they
# are: what was tested on the T5.1 there runs here unchanged.
set(DASH7 "${CMAKE_CURRENT_LIST_DIR}/../../notstock-dash-7inch/main")
file(GLOB SRCS "${CMAKE_CURRENT_LIST_DIR}/*.c" "${CMAKE_CURRENT_LIST_DIR}/fonts/*.c")
list(APPEND SRCS "${DASH7}/obd2.c" "${DASH7}/dtc_text.c")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "." "${DASH7}")

# the generated font and image files pick their lvgl include path from this
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
