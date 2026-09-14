idf_component_register(SRCS "cell_gateway_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES databus nvs_flash sim_modem sd_card esp_led tasks)
