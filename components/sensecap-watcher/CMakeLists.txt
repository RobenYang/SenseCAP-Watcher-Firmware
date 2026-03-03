
set(requires
    "esp_mm"
    "esp_psram"
    "esp_lcd"
    "fatfs"
    "esp_timer"
    "driver"
    "spiffs"
    "lvgl"
    "esp_lvgl_port"
    "espressif__esp_codec_dev"
    "espressif__button"
    "espressif__led_strip"
    "knob"
    "sscma_client"
    "esp_io_expander_pca95xx_16bit"
    "espressif__esp_io_expander"
    "espressif__esp_lcd_spd2010"
    "espressif__esp_lcd_touch_spd2010"
)
# set(requires )

idf_component_register(
    SRCS "sensecap-watcher.c"
    INCLUDE_DIRS "include"
    # PRIV_REQUIRES ${priv_requires}
    REQUIRES ${requires}
)
