idf_component_register(
    SRCS "radio.c" "si5351.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_event
)
