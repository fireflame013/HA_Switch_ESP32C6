/*
 * servo.c - SG90舵机PWM控制 + MT3608电源管理
 *
 * 流程:
 * 1. 先开启MT3608(5V供电)
 * 2. 等待电源稳定
 * 3. 设置舵机角度(PWM)
 * 4. 等待舵机到位
 * 5. 关闭MT3608(省电)
 */

#include "servo.h"
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = TAG_SERVO;
static bool s_light_on = false;
static bool s_power_on = false;

/* ========== LEDC PWM 初始化 ========== */

static void ledc_init(void)
{
    /* 定时器配置 */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_13_BIT,  // 13位分辨率
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    /* 通道配置 */
    ledc_channel_config_t channel_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = GPIO_SERVO_PWM,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel_cfg);
}

/* ========== 角度转duty ========== */

static uint32_t angle_to_duty(int angle)
{
    /* 13位分辨率, 50Hz周期=20ms
     * duty = pulse_width_us / 20000 * 2^13
     */
    int pulse_us = SERVO_MIN_PULSE_US +
                   (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * angle / 180;
    return (uint32_t)((uint64_t)pulse_us * 8192 / 20000);
}

/* ========== 电源控制 ========== */

static void servo_power_on(void)
{
    if (s_power_on) return;
    gpio_set_level(GPIO_MT3608_EN, 1);
    s_power_on = true;
    ESP_LOGD(TAG, "MT3608已开启, 5V电源就绪");
}

static void servo_power_off(void)
{
    /* 先把PWM设为0, 防止舵机发出异响 */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    gpio_set_level(GPIO_MT3608_EN, 0);
    s_power_on = false;
    ESP_LOGD(TAG, "MT3608已关闭, 5V电源切断");
}

/* ========== 舵机运动 ========== */

static void servo_move_to(int angle)
{
    ESP_LOGI(TAG, "舵机转到 %d度", angle);

    servo_power_on();
    vTaskDelay(pdMS_TO_TICKS(20));  // 等电源稳定

    uint32_t duty = angle_to_duty(angle);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    vTaskDelay(pdMS_TO_TICKS(SERVO_MOVE_MS));  // 等舵机到位

    servo_power_off();
}

/* ========== 公开接口 ========== */

void servo_init(void)
{
    /* 配置MT3608 EN引脚 */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << GPIO_MT3608_EN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_cfg);
    gpio_set_level(GPIO_MT3608_EN, 0);  // 默认关闭5V

    /* 初始化LEDC */
    ledc_init();

    ESP_LOGI(TAG, "舵机初始化完成 (PWM=GPIO%d, EN=GPIO%d)", GPIO_SERVO_PWM, GPIO_MT3608_EN);
}

void servo_set_on(void)
{
    servo_move_to(SERVO_ON_ANGLE);
    s_light_on = true;
}

void servo_set_off(void)
{
    servo_move_to(SERVO_OFF_ANGLE);
    s_light_on = false;
}

bool servo_is_on(void)
{
    return s_light_on;
}
