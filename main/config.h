/*
 * config.h - 全局配置
 *
 * 根据实际硬件修改这里的引脚和参数
 */

#ifndef CONFIG_SMARTSWITCH_H
#define CONFIG_SMARTSWITCH_H

/* ============ WiFi ============ */
#define WIFI_SSID           "XM_TWT_AP"
#define WIFI_PASS           "19691022Md"
#define WIFI_MAX_RETRY      10
#define WIFI_TWT_ENABLE     1       // 1=启用iTWT(需AP支持WiFi6), 0=使用Modem Sleep

/* ============ iTWT 参数 ============ */
/* Wake Interval (µs) = Mantissa × 2^Exponent
 * 间隔2-5秒: exp=13, mant≈244~610 → 取400≈3.28秒
 * Wake Duration (µs) = Duration × wake_duration_unit (unit=0:256µs, unit=1:1024µs)
 * 时长100-200ms: unit=1, dura≈98~195 → 取150≈153.6ms */
/* === 实际稳定值（AP协商结果）=== */
#define WIFI_TWT_WAKE_INVL_EXPN   13
#define WIFI_TWT_WAKE_INVL_MANT   400
#define WIFI_TWT_MIN_WAKE_DURA    255
#define WIFI_TWT_WAKE_DURATION_UNIT 0     /* 0=256µs */
#define WIFI_TWT_TRIGGER_ENABLED  1     // 1=触发式(AP发Trigger帧才唤醒,更省电)
#define WIFI_TWT_ANNOUNCED        1     // 1=Announced(唤醒时通知AP), 0=Unannounced
#define WIFI_TWT_SETUP_TIMEOUT_MS 5000  // 协商超时(ms)

/* ============ MQTT ============ */
#define MQTT_BROKER_URI     "mqtt://192.168.0.160:1883"  // 改成你的MQTT broker
#define MQTT_USERNAME       ""       // 无认证留空
#define MQTT_PASSWORD       ""
#define MQTT_CLIENT_ID      "esp32c6_light_switch"
#define MQTT_TOPIC_CMD      "homeassistant/switch/light_switch/set"
#define MQTT_TOPIC_STATE    "homeassistant/switch/light_switch/state"
#define MQTT_TOPIC_BATTERY  "homeassistant/sensor/light_switch_battery/state"
#define MQTT_TOPIC_AVAIL    "homeassistant/switch/light_switch/availability"

/* ============ GPIO ============ */
#define GPIO_SERVO_PWM      4       // 舵机PWM信号
#define GPIO_MT3608_EN      5       // MT3608使能(控制5V)
#define GPIO_BATTERY_ADC    1       // 电池电压ADC输入(100K+100K分压)

/* ============ 舵机 ============ */
#define SERVO_MIN_PULSE_US  500     // 0度脉宽(us)
#define SERVO_MAX_PULSE_US  2500    // 180度脉宽(us)
#define SERVO_FREQ_HZ       50      // PWM频率(Hz)
#define SERVO_OFF_ANGLE     0       // 灯灭时舵机角度
#define SERVO_ON_ANGLE      180     // 灯亮时舵机角度
#define SERVO_MOVE_MS       500     // 舵机运动耗时(ms), 运动后关闭电源

/* ============ 电池 ============ */
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_1   // GPIO1对应ADC1_CH1
#define BATTERY_ADC_UNIT    ADC_UNIT_1
#define BATTERY_ADC_ATTEN   ADC_ATTEN_DB_12 // 12dB衰减, ESP32-C6最大衰减, 量程约3100mV
#define BATTERY_DIVIDER_RATIO 2.0           // 分压比(两个100K), 实际电压=ADC值*2
#define BATTERY_SAMPLE_COUNT 16             // 多次采样取平均
#define BATTERY_VOLTAGE_MIN 3.0             // 电池电压下限(V), 低于此值报警
#define BATTERY_REPORT_INTERVAL_S 300       // 电压上报间隔(秒), 5分钟一次

/* ============ 调试 ============ */
#define TAG_WIFI    "WIFI"
#define TAG_MQTT    "MQTT"
#define TAG_SERVO   "SERVO"
#define TAG_BATT    "BATT"
#define TAG_POWER   "POWER"
#define TAG_MAIN    "MAIN"

#endif /* CONFIG_SMARTSWITCH_H */
