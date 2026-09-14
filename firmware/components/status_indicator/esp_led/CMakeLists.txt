idf_component_register(
    SRCS "esp_led.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio led_strip
)
