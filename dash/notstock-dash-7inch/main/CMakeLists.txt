idf_component_register(
    SRCS
        "main.c"
        "ui.c"
        "rusefi_can.c"
        "settings.c"
        "touch.c"
        "ui_menu.c"
        "ui_log.c"
        "icons.c"
        "dials.c"
        "splash.c"
        "boot_anim.c"
        "fonts/dash_lbl_18.c"
        "fonts/dash_lbl_13.c"
        "fonts/dash_speed_56.c"
        "fonts/dash_orb_40.c"
        "fonts/dash_orb_18.c"
        "fonts/dash_orb_30.c"
        "fonts/dash_orb_14.c"
    INCLUDE_DIRS ".")

# the generated font files pick their lvgl include path from this
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)

