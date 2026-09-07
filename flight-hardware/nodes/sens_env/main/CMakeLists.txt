idf_component_register(SRCS "sens_env_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_event esp_netif nvs_flash tasks mock_store tasks esp_led)
