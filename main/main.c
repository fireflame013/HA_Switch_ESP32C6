/*
 * main.c - 太阳能灯控智能家居主程序
 *
 * ESP32-C6 + CN3163太阳能充电 + SG90舵机 + Home Assistant
 *
 * 启动流程:
 * 1. 初始化NVS, WiFi, MQTT
 * 2. 连接WiFi(支持TWT低功耗)
 * 3. 连接MQTT, 发布HA自动发现配置
 * 4. 启动电池监控任务
 * 5. 主循环: 检查连接状态, 心跳保持
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "config.h"
#include "wifi_sta.h"
#include "app_mqtt_client.h"
#include "servo.h"
#include "power.h"

static const char *TAG = TAG_MAIN;

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " 太阳能灯控 v1.0");
    ESP_LOGI(TAG, " ESP32-C6 | SG90 | CN3163 | Home Assistant");
    ESP_LOGI(TAG, "========================================");

    /* 1. 舵机 + 电源管理初始化 */
    servo_init();
    power_init();

    /* 2. WiFi连接 */
    ESP_LOGI(TAG, "[1/3] 连接WiFi...");
    esp_err_t ret = wifi_sta_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi连接失败, 停止启动");
        return;
    }
    ESP_LOGI(TAG, "WiFi就绪");

    /* 3. MQTT连接 */
    ESP_LOGI(TAG, "[2/3] 连接MQTT...");
    ret = app_mqtt_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MQTT初始化失败");
        return;
    }
    ret = app_mqtt_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MQTT启动失败");
        return;
    }

    /* 等待MQTT连接 */
    int wait = 0;
    while (!app_mqtt_is_connected() && wait < 15) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        wait++;
    }
    if (app_mqtt_is_connected()) {
        ESP_LOGI(TAG, "MQTT就绪");
    } else {
        ESP_LOGW(TAG, "MQTT连接超时, 继续运行(将自动重连)");
    }

    /* 4. 启动电池监控 */
    ESP_LOGI(TAG, "[3/3] 启动电池监控...");
    power_start_monitor();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " 系统就绪, 等待Home Assistant指令");
    ESP_LOGI(TAG, "========================================");

    /* 5. 主循环 - 保持存活, 监控连接状态 */
    while (1) {
        /* WiFi断连自动重连由事件处理函数负责 */
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
