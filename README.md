# ESP32-C6 太阳能灯控智能家居

## 硬件清单
- **MCU**: ESP32-C6 (WiFi 6, 低功耗)
- **充电**: CN3163 太阳能充电管理
- **储能**: 锂电池 (18650或聚合物)
- **执行器**: SG90 舵机 (控制灯开关)
- **升压**: MT3608 (5V, 给舵机供电)
- **分压**: 100K+100K (电池电压ADC检测)

## GPIO 分配
| GPIO | 功能 | 说明 |
|------|------|------|
| GPIO1 | ADC1_CH1 | 电池电压检测 (分压后) |
| GPIO4 | LEDC | 舵机PWM信号 |
| GPIO5 | GPIO OUT | MT3608 EN (5V使能) |

## 功能特性
- **Home Assistant 集成** - MQTT自动发现, 即插即用
- **WiFi 6 TWT 休眠** - 低功耗待机, 实时响应
- **电池电压监控** - 定期上报, 低电量自动关灯保护
- **舵机电源管理** - 运动时供电, 静止时断电省电

## 编译烧录
```bash
# 设置ESP-IDF环境 (如果还没加到bashrc)
source ~/esp-idf/export.sh

# 编译
cd ~/esp32c6-smartswitch
idf.py set-target esp32c6
idf.py build

# 烧录 (替换为实际串口)
idf.py -p /dev/ttyACM0 flash

# 监视串口输出
idf.py -p /dev/ttyACM0 monitor
```

## 使用前修改
编辑 `main/config.h`, 修改以下配置:
1. `WIFI_SSID` / `WIFI_PASS` - 你的WiFi名称和密码
2. `MQTT_BROKER_URI` - 你的MQTT Broker地址
3. `MQTT_USERNAME` / `MQTT_PASSWORD` - MQTT认证(如果需要)

## Home Assistant 配置
设备会自动出现在HA中, 无需手动配置:
- **开关实体**: `switch.太阳能灯开关`
- **传感器**: `sensor.电池电压`

## 低功耗策略
- **TWT模式** (需AP支持WiFi6): 无线电按协商周期休眠, 功耗极低
- **Modem Sleep回退** (AP不支持WiFi6时): 关闭射频模块, 功耗~0.8mA
- **舵机断电**: 运动完成后立即关闭MT3608, 避免舵机待机电流(~10mA)
