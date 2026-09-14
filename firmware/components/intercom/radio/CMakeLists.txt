idf_component_register(
    SRCS "radio.c" "si5351.c" "rtty.c" "radio_frame.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_event esp_timer
)
