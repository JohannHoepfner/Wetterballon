idf_component_register(SRCS "radio_gateway_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES databus neo_m8 nvs_flash radio tasks mock_store esp_led)
