idf_component_register(SRCS "F1CUart.c" "ESP_Mult.c" "NowMode.c" "F1CUart.c"
                    PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi esp_driver_uart esp_driver_gpio esp_psram
                    INCLUDE_DIRS ".")
