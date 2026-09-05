idf_component_register(
    SRCS "sms.c" "modem.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_gpio esp_netif
)
