/*
 * power.c - 电源管理
 *
 * 电池监控任务: 定期采样电压并上报MQTT
 * 低电量保护: 电压过低时关闭舵机电源
 */

#include "power.h"
#include "config.h"
#include "battery.h"
#include "app_mqtt_client.h"
#include "servo.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = TAG_POWER;
static TaskHandle_t s_battery_task = NULL;

/* ========== 电池监控任务 ========== */

static void battery_monitor_task(void *arg)
{
    ESP_LOGI(TAG, "电池监控任务启动, 上报间隔=%ds", BATTERY_REPORT_INTERVAL_S);

    while (1) {
        float voltage = battery_read_voltage();
        ESP_LOGI(TAG, "电池电压: %.2fV", voltage);

        /* 上报电压到MQTT */
        app_mqtt_publish_battery_voltage(voltage);

        /* 低电量保护 */
        if (voltage < BATTERY_VOLTAGE_MIN) {
            ESP_LOGW(TAG, "⚠ 电池电压过低(%.2fV < %.1fV), 建议充电!", voltage, BATTERY_VOLTAGE_MIN);
            /* 如果灯是开的, 关掉以节省电量 */
            if (servo_is_on()) {
                ESP_LOGW(TAG, "自动关闭舵机以保护电池");
                servo_set_off();
                app_mqtt_publish_battery_voltage(voltage);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(BATTERY_REPORT_INTERVAL_S * 1000));
    }
}

/* ========== 公开接口 ========== */

void power_init(void)
{
    battery_init();
    ESP_LOGI(TAG, "电源管理初始化完成");
}

void power_start_monitor(void)
{
    if (s_battery_task != NULL) return;

    xTaskCreate(
        battery_monitor_task,
        "batt_monitor",
        2048,           // 2KB栈空间
        NULL,
        tskIDLE_PRIORITY + 1,
        &s_battery_task
    );
}
