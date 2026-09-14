idf_component_register(SRCS "sens_mock_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_event esp_netif nvs_flash)
