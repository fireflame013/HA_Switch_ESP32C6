idf_component_register(SRCS "main.c"
                            "wifi_sta.c"
                            "mqtt_client.c"
                            "servo.c"
                            "battery.c"
                            "power.c"
                       PRIV_REQUIRES esp_wifi esp_event mqtt esp_driver_gpio esp_driver_ledc esp_adc nvs_flash
                       INCLUDE_DIRS ".")
