idf_component_register(
    SRCS "pt1000.c" "ads1115.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_adc
)
