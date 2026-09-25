idf_component_register(
    SRCS
        "main.c"
        "ui.c"
        "rusefi_can.c"
        "settings.c"
        "touch.c"
        "ui_menu.c"
        "icons.c"
        "dials.c"
        "logo.c"
        "fonts/dash_num_64.c"
        "fonts/dash_num_46.c"
        "fonts/dash_num_28.c"
        "fonts/dash_num_15.c"
        "fonts/dash_lbl_18.c"
        "fonts/dash_lbl_13.c"
    INCLUDE_DIRS ".")

# the generated font files pick their lvgl include path from this
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)

