/*
 * wifi_sta.c - WiFi Station + iTWT 低功耗
 *
 * 功能:
 * - 连接指定WiFi AP
 * - 支持WiFi 6 iTWT (individual Target Wake Time) 主动协商
 * - iTWT协商失败时自动回退到Modem Sleep
 *
 * 参考 esp32c6-test/twt_test_main.c 的 iTWT 实现
 */

#include "wifi_sta.h"
#include "config.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_wifi_he.h"
#include "esp_wifi_he_types.h"
#include "nvs_flash.h"

static const char *TAG = TAG_WIFI;

/* WiFi连接状态事件组 */
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
static bool s_connected = false;

/* iTWT协商结果（由事件回调设置） */
static bool s_twt_setup_success = false;
static uint8_t s_twt_flow_id = 0xFF;

/* ========== 事件处理 ========== */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi STA 启动, 开始连接...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        s_twt_setup_success = false;
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "连接断开, 重试 %d/%d", s_retry_num, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "连接失败, 达到最大重试次数");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "获取IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_TWT_WAKEUP) {
        wifi_event_sta_twt_wakeup_t *wake = (wifi_event_sta_twt_wakeup_t *)event_data;
        ESP_LOGD(TAG, "TWT唤醒 [flow_id=%d, type=%d]", wake->flow_id, wake->twt_type);
    }
}

/* ========== iTWT Setup 事件回调 ========== */

static void itwt_setup_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_event_sta_itwt_setup_t *setup = (wifi_event_sta_itwt_setup_t *)event_data;

    if (setup->status == 1) {
        /* 协商成功 */
        s_twt_setup_success = true;
        s_twt_flow_id = setup->config.flow_id;

        /* 计算实际唤醒间隔和时长 */
        /* Wake Interval (µs) = mantissa × 2^exponent */
        uint64_t wake_interval_us = (uint64_t)setup->config.wake_invl_mant
                                    << setup->config.wake_invl_expn;

        uint64_t wake_duration_us = (uint64_t)setup->config.min_wake_dura
                                    * (setup->config.wake_duration_unit == 1 ? 1024 : 256);

        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "==========================================");
        ESP_LOGI(TAG, "  iTWT 协商成功！");
        ESP_LOGI(TAG, "==========================================");
        ESP_LOGI(TAG, "  Flow ID             : %d (AP分配)", setup->config.flow_id);
        ESP_LOGI(TAG, "  触发模式            : %s",
                 setup->config.trigger ? "Trigger-Enabled" : "Non-Trigger");
        ESP_LOGI(TAG, "  流类型              : %s",
                 setup->config.flow_type ? "Unannounced" : "Announced");
        ESP_LOGI(TAG, "  唤醒间隔            : %.1f ms (≈%.2f s)",
                 wake_interval_us / 1000.0, wake_interval_us / 1000000.0);
        ESP_LOGI(TAG, "  最小唤醒时长        : %.1f ms",
                 wake_duration_us / 1000.0);
        ESP_LOGI(TAG, "  目标唤醒时间        : %llu µs", setup->target_wake_time);
        ESP_LOGI(TAG, "==========================================");
    } else {
        /* 协商失败 */
        s_twt_setup_success = false;
        ESP_LOGE(TAG, "");
        ESP_LOGE(TAG, "==========================================");
        ESP_LOGE(TAG, "  iTWT 协商失败！ status=0x%x", setup->status);
        ESP_LOGE(TAG, "==========================================");
        if (setup->status == 0x200) {  /* ESP_ERR_WIFI_TWT_SETUP_TIMEOUT */
            ESP_LOGE(TAG, "  原因: AP 无响应（超时 %d ms）", WIFI_TWT_SETUP_TIMEOUT_MS);
            ESP_LOGE(TAG, "  建议: 确认路由器支持Wi-Fi 6且TWT已开启");
        } else if (setup->status == 0x201) {  /* TXFAIL */
            ESP_LOGE(TAG, "  原因: TWT Setup Request发送失败");
        } else if (setup->status == 0x202) {  /* REJECT */
            ESP_LOGE(TAG, "  原因: AP拒绝iTWT协商");
        }
        ESP_LOGE(TAG, "  -> 回退到Modem Sleep");
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    }
}

/* ========== iTWT Teardown 事件回调 ========== */

static void itwt_teardown_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    wifi_event_sta_itwt_teardown_t *td = (wifi_event_sta_itwt_teardown_t *)event_data;
    ESP_LOGW(TAG, "iTWT Teardown: flow_id=%d, status=%d", td->flow_id, td->status);
    s_twt_setup_success = false;
}

/* ========== iTWT 协商 ========== */

static void wifi_setup_iwt(void)
{
#if WIFI_TWT_ENABLE
    /* ESP-IDF 6.x iTWT API
     * 主动发起 iTWT 协商，使用 config.h 中的参数
     */
    wifi_itwt_setup_config_t setup_config = {
        .setup_cmd          = TWT_REQUEST,                         /* 请求协商 */
        .flow_id            = 0,                                   /* 初始0，AP分配 */
        .twt_id             = 0,                                   /* TWT连接ID */
        .flow_type          = WIFI_TWT_ANNOUNCED ? 0 : 1,          /* Announced */
        .min_wake_dura      = WIFI_TWT_MIN_WAKE_DURA,             /* 唤醒时长 */
        .wake_duration_unit = WIFI_TWT_WAKE_DURATION_UNIT,      /* 0=256µs, 1=1024µs */
        .wake_invl_expn     = WIFI_TWT_WAKE_INVL_EXPN,            /* 间隔指数 */
        .wake_invl_mant     = WIFI_TWT_WAKE_INVL_MANT,            /* 间隔尾数 */
        .trigger            = WIFI_TWT_TRIGGER_ENABLED,           /* 触发式 */
        .timeout_time_ms    = WIFI_TWT_SETUP_TIMEOUT_MS,          /* 超时 */
    };

    /* Wake Interval (µs) = mantissa × 2^exponent */
    uint64_t wake_interval_us = (uint64_t)setup_config.wake_invl_mant
                                << setup_config.wake_invl_expn;
    uint64_t wake_duration_us = (uint64_t)setup_config.min_wake_dura
                                * (setup_config.wake_duration_unit ? 1024 : 256);

    ESP_LOGI(TAG, "========== 发起 iTWT 协商 ==========");
    ESP_LOGI(TAG, "  触发模式            : %s",
             setup_config.trigger ? "Trigger-Enabled" : "Non-Trigger");
    ESP_LOGI(TAG, "  流类型              : %s",
             setup_config.flow_type ? "Unannounced" : "Announced");
    ESP_LOGI(TAG, "  唤醒时长            : %.1f ms (dura=%d, unit=%d×%dµs)",
             wake_duration_us / 1000.0, setup_config.min_wake_dura,
             setup_config.wake_duration_unit,
             setup_config.wake_duration_unit ? 1024 : 256);
    ESP_LOGI(TAG, "  唤醒间隔            : %.1f ms (≈%.2f s) [mant=%d, expn=%d]",
             wake_interval_us / 1000.0,
             wake_interval_us / 1000000.0,
             setup_config.wake_invl_mant,
             setup_config.wake_invl_expn);
    ESP_LOGI(TAG, "==========================================");

    esp_err_t err = esp_wifi_sta_itwt_setup(&setup_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_sta_itwt_setup() 失败: 0x%x, 回退到Modem Sleep", err);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    }
    /* 协商结果由 itwt_setup_handler() 回调异步通知 */
#else
    ESP_LOGI(TAG, "TWT未启用, 使用Modem Sleep");
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
#endif
}

/* ========== 公开接口 ========== */

esp_err_t wifi_sta_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    /* NVS初始化 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }

    /* 网络接口和事件循环 */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    /* WiFi默认配置 */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 注册Wi-Fi事件处理 */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_TWT_WAKEUP, &wifi_event_handler, NULL));

#if WIFI_TWT_ENABLE
    /* 注册iTWT事件处理 */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_ITWT_SETUP, &itwt_setup_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_ITWT_TEARDOWN, &itwt_teardown_handler, NULL));
#endif

    /* STA配置 */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi STA 初始化完成, 等待连接...");

    /* 等待连接结果 */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE,
            pdMS_TO_TICKS(30000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "已连接到 %s", WIFI_SSID);
        /* 连接成功后发起iTWT协商 */
        wifi_setup_iwt();
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "连接到 %s 失败", WIFI_SSID);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "连接超时");
        return ESP_ERR_TIMEOUT;
    }
}

bool wifi_sta_is_connected(void)
{
    return s_connected;
}

void wifi_sta_reconnect(void)
{
    s_retry_num = 0;
    esp_wifi_connect();
}
