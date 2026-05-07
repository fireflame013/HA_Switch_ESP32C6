/*
 * battery.c - 电池电压ADC采样
 *
 * 硬件: 100K+100K电阻分压, ADC读取后×2得实际电池电压
 * ESP32-C6 ADC1, 12dB衰减, 量程约3100mV
 */

#include "battery.h"
#include "config.h"

#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = TAG_BATT;

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static bool s_calibrated = false;

/* ========== ADC校准 ========== */

static void battery_adc_cali_init(void)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali_handle) == ESP_OK) {
        s_calibrated = true;
        ESP_LOGI(TAG, "ADC校准初始化成功(曲线拟合)");
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_config, &s_cali_handle) == ESP_OK) {
        s_calibrated = true;
        ESP_LOGI(TAG, "ADC校准初始化成功(线性拟合)");
    }
#endif

    if (!s_calibrated) {
        ESP_LOGW(TAG, "ADC校准不可用, 将使用原始ADC值(精度较低)");
    }
}

/* ========== 公开接口 ========== */

void battery_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, BATTERY_ADC_CHANNEL, &chan_cfg));

    battery_adc_cali_init();

    ESP_LOGI(TAG, "电池ADC初始化完成 (GPIO%d, 分压比×%.0f)", GPIO_BATTERY_ADC, BATTERY_DIVIDER_RATIO);
}

float battery_read_voltage(void)
{
    if (s_adc_handle == NULL) return 0.0f;

    int raw_sum = 0;
    for (int i = 0; i < BATTERY_SAMPLE_COUNT; i++) {
        int raw = 0;
        adc_oneshot_read(s_adc_handle, BATTERY_ADC_CHANNEL, &raw);
        raw_sum += raw;
    }
    int raw_avg = raw_sum / BATTERY_SAMPLE_COUNT;

    float voltage;
    if (s_calibrated) {
        int voltage_mv = 0;
        adc_cali_raw_to_voltage(s_cali_handle, raw_avg, &voltage_mv);
        voltage = voltage_mv / 1000.0f;
    } else {
        /* 无校准时粗略换算: 12位ADC, 量程1.1V */
        voltage = raw_avg * 1.1f / 4095.0f;
    }

    /* 乘以分压比还原实际电池电压 */
    voltage *= BATTERY_DIVIDER_RATIO;

    ESP_LOGD(TAG, "ADC raw=%d, 电压=%.2fV", raw_avg, voltage);
    return voltage;
}

bool battery_is_low(void)
{
    return battery_read_voltage() < BATTERY_VOLTAGE_MIN;
}
