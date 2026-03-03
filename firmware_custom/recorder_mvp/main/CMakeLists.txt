idf_component_register(
    SRCS
        "app_main.c"
        "rec_audio.c"
        "rec_storage.c"
        "rec_input.c"
        "rec_ui.c"
        "rec_led.c"
        "rec_power.c"
    INCLUDE_DIRS "."
    REQUIRES sensecap-watcher esp_timer
)
