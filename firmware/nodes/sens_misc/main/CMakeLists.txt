idf_component_register(SRCS "sens_misc_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES pt1000 esp_led databus mock_store tasks esp_event esp_netif nvs_flash)