idf_component_register(SRCS "debug_station_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_event esp_netif nvs_flash)
