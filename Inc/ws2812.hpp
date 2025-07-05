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
    esp_err_t init() {
        // RMT 发送配置
        rmt_tx_channel_config_t tx_config = {
            .gpio_num = _pin,
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10 * 1000 * 1000, // 10MHz, 0.1us/tick
            .mem_block_symbols = 64,
            .trans_queue_depth = 4,
            .intr_priority = 0,  // 添加缺失的字段
            .flags = {
                .invert_out = false,
                .with_dma = false,
                .io_loop_back = false,
                .io_od_mode = false,
                .allow_pd = false   // 添加缺失的字段
            }
        };

        // 创建 RMT 通道
        ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_config, &_channel), "WS2812", "创建RMT通道失败");

        // 创建编码器
        rmt_bytes_encoder_config_t bytes__encoderconfig = {
            .bit0 = {
                .duration0 = 4, // T0H 0.4us (4 ticks)
                .level0 = 1,
                .duration1 = 8, // T0L 0.8us (8 ticks)
                .level1 = 0,
            },
            .bit1 = {
                .duration0 = 8, // T1H 0.8us (8 ticks)
                .level0 = 1,
                .duration1 = 4, // T1L 0.4us (4 ticks)
                .level1 = 0,
            },
            .flags = {
                .msb_first = 1  // WS2812 使用 MSB 优先
            }
        };
        ESP_RETURN_ON_ERROR(rmt_new_bytes_encoder(&bytes__encoderconfig, &_encoder), "WS2812", "创建编码器失败");

        // 启用通道
        return rmt_enable(_channel);
    }

    // 更新 LED 灯带
    esp_err_t show(std::vector<rgb> &color) {
        if (!_channel || !_encoder) return ESP_ERR_INVALID_STATE;
        // 准备传输配置
        rmt_transmit_config_t transmit_config = {
            .loop_count = 0, // 不循环
            .flags = {
                .eot_level = 0, // 结束后保持低电平
                .queue_nonblocking = false  // 添加缺失的字段
            }
        };
        return rmt_transmit(_channel, _encoder, color.data(), color.size()*3, &transmit_config);
    }

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