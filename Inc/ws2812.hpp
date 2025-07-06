#pragma once

#include <vector>
#include <cstring>
#include <driver/rmt_tx.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_check.h>

struct rgb
{
private:
    // Gamma 校正表 (提高颜色精度)
    static uint8_t gamma_correction_[256];
public:
    uint8_t green;
    uint8_t red;
    uint8_t blue;
    rgb(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) : green(g), red(r), blue(b) {}
    rgb gamme()
    {
        return rgb(gamma_correction_[red], gamma_correction_[green], gamma_correction_[blue]);
    }
    bool operator==(const rgb &other) const {
        return red == other.red && green == other.green && blue == other.blue;
    }
};

class WS2812 {
public:
    // 构造函数 - 移除了rmt__channelt参数
    WS2812(gpio_num_t Pin_num) 
        : _pin(Pin_num), _channel(nullptr), _encoder(nullptr) {
    }
    // 初始化 RMT 外设
    esp_err_t init();
    // 更新 LED 灯带
    esp_err_t show(std::vector<rgb> &color);
    // 等待传输完成
    esp_err_t wait() {
        return rmt_tx_wait_all_done(_channel, 100); // 等待最多100ms
    }
    // 析构函数
    ~WS2812() {
        if (_encoder) rmt_del_encoder(_encoder);
        if (_channel) {
            rmt_disable(_channel);
            rmt_del_channel(_channel);
        }
    }
private:
    gpio_num_t _pin;
    uint16_t led_count_;
    rmt_channel_handle_t _channel;
    rmt_encoder_handle_t _encoder;
    std::vector<rgb> _pixels;
};