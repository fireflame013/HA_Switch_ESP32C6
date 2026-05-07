/*
 * mqtt_client.c - MQTT + Home Assistant 自动发现
 *
 * 功能:
 * - 连接MQTT Broker
 * - 发布HA自动发现配置(开关实体)
 * - 订阅控制指令
 * - 发布状态和电池电压
 */

#include "app_mqtt_client.h"
#include "config.h"
#include "servo.h"
#include "battery.h"

/* esp-mqtt component header (in managed_components/espressif__mqtt) */
#include "mqtt_client.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = TAG_MQTT;
static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;

/* ========== HA自动发现 ========== */

static void publish_ha_discovery(void)
{
    /* 开关实体发现配置 */
    const char *switch_config = "{"
        "\"name\":\"太阳能灯开关\","
        "\"unique_id\":\"esp32c6_light_switch_01\","
        "\"command_topic\":\"" MQTT_TOPIC_CMD "\","
        "\"state_topic\":\"" MQTT_TOPIC_STATE "\","
        "\"availability_topic\":\"" MQTT_TOPIC_AVAIL "\","
        "\"payload_on\":\"ON\","
        "\"payload_off\":\"OFF\","
        "\"state_on\":\"ON\","
        "\"state_off\":\"OFF\","
        "\"device\":{"
            "\"identifiers\":[\"esp32c6_light_switch_01\"],"
            "\"name\":\"太阳能灯控\","
            "\"model\":\"ESP32C6-SmartSwitch\","
            "\"manufacturer\":\"DIY\""
        "}"
    "}";

    esp_mqtt_client_publish(s_client,
        "homeassistant/switch/esp32c6_light_switch_01/config",
        switch_config, 0, 1, true);

    /* 电池电压传感器发现配置 */
    const char *sensor_config = "{"
        "\"name\":\"电池电压\","
        "\"unique_id\":\"esp32c6_light_switch_battery\","
        "\"state_topic\":\"" MQTT_TOPIC_BATTERY "\","
        "\"unit_of_measurement\":\"V\","
        "\"device_class\":\"voltage\","
        "\"state_class\":\"measurement\","
        "\"suggested_display_precision\":2,"
        "\"device\":{"
            "\"identifiers\":[\"esp32c6_light_switch_01\"]"
        "}"
    "}";

    esp_mqtt_client_publish(s_client,
        "homeassistant/sensor/esp32c6_light_switch_battery/config",
        sensor_config, 0, 1, true);

    /* 上线 */
    esp_mqtt_client_publish(s_client, MQTT_TOPIC_AVAIL, "online", 0, 1, true);

    ESP_LOGI(TAG, "HA自动发现配置已发布");
}

/* ========== MQTT事件处理 ========== */

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT已连接");
            s_connected = true;
            /* 订阅控制指令 */
            esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_CMD, 1);
            /* 发布HA发现配置 */
            publish_ha_discovery();
            /* 发布初始状态 */
            esp_mqtt_client_publish(s_client, MQTT_TOPIC_STATE,
                servo_is_on() ? "ON" : "OFF", 0, 1, true);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT断开连接");
            s_connected = false;
            break;

        case MQTT_EVENT_DATA:
            /* 收到控制指令 */
            if (event->data_len > 0) {
                char cmd[8] = {0};
                int len = event->data_len < 7 ? event->data_len : 7;
                memcpy(cmd, event->data, len);

                ESP_LOGI(TAG, "收到指令: %s", cmd);

                if (strcmp(cmd, "ON") == 0) {
                    servo_set_on();
                    esp_mqtt_client_publish(s_client, MQTT_TOPIC_STATE, "ON", 0, 1, true);
                } else if (strcmp(cmd, "OFF") == 0) {
                    servo_set_off();
                    esp_mqtt_client_publish(s_client, MQTT_TOPIC_STATE, "OFF", 0, 1, true);
                }
            }
            break;

        default:
            break;
    }
}

/* ========== 公开接口 ========== */

esp_err_t app_mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
        .session.keepalive = 0,   /* 关闭keepalive，由TWT控制唤醒 */
    };

    if (strlen(MQTT_USERNAME) > 0) {
        mqtt_cfg.credentials.username = MQTT_USERNAME;
    }
    if (strlen(MQTT_PASSWORD) > 0) {
        mqtt_cfg.credentials.authentication.password = MQTT_PASSWORD;
    }

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "MQTT客户端初始化失败");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    return ESP_OK;
}

esp_err_t app_mqtt_start(void)
{
    if (s_client == NULL) return ESP_ERR_INVALID_STATE;
    return esp_mqtt_client_start(s_client);
}

bool app_mqtt_is_connected(void)
{
    return s_connected;
}

void app_mqtt_publish_battery_voltage(float voltage)
{
    if (!s_connected) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", voltage);
    esp_mqtt_client_publish(s_client, MQTT_TOPIC_BATTERY, buf, 0, 0, false);
}
