#ifndef WIFI_STA_H
#define WIFI_STA_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t wifi_sta_init(void);
bool wifi_sta_is_connected(void);
void wifi_sta_reconnect(void);

#endif
