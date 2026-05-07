#ifndef APP_MQTT_CLIENT_H
#define APP_MQTT_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t app_mqtt_init(void);
esp_err_t app_mqtt_start(void);
bool app_mqtt_is_connected(void);
void app_mqtt_publish_battery_voltage(float voltage);

#endif
