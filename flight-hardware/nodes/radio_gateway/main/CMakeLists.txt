idf_component_register(SRCS "radio_gateway_main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES databus nvs_flash sim_modem)
