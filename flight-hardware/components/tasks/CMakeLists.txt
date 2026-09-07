idf_component_register(
    SRCS "sensor_task.c" "intercom_task.c"
    INCLUDE_DIRS "include"
    REQUIRES databus
)
