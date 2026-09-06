idf_component_register(
    SRCS "sim_modem.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_netif esp_modem lwip
)
