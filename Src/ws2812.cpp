#include "ws2812.hpp"

uint8_t rgb::gamma_correction_[256] = {
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,
        1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,
        2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   5,   5,   5,
        5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,   8,   9,   9,   9,   10,
        10,  10,  11,  11,  11,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16,
        17,  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  24,  24,  25,
        25,  26,  27,  27,  28,  29,  29,  30,  31,  32,  32,  33,  34,  35,  35,  36,
        37,  38,  39,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  50,
        51,  52,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  66,  67,  68,
        69,  70,  72,  73,  74,  75,  77,  78,  79,  81,  82,  83,  85,  86,  87,  89,
        90,  92,  93,  95,  96,  98,  99,  101, 102, 104, 105, 107, 109, 110, 112, 114,
        115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
        144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
        177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
        215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
    };


esp_err_t WS2812::init()
{
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

esp_err_t WS2812::show(std::vector<rgb> &color)
{
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

