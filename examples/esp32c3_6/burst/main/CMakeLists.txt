idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES lora_networking esp_driver_spi esp_timer)
