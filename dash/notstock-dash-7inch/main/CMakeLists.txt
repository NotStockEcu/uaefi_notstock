# Every .c in main/ and main/fonts/ is built; new screens, looks and
# generated art need no entry here.
idf_component_register(
    SRC_DIRS "." "fonts"
    INCLUDE_DIRS ".")

# the generated font files pick their lvgl include path from this
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
