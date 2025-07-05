#pragma once

#include <vector>
#include "ws2812.hpp"

class DotMatrixScreen : private WS2812
{
private:
    std::vector<rgb> _pixels; // 存储屏幕像素数据（每个像素一个rgb颜色）
    uint32_t _width;          // 屏幕宽度（像素点数）
    uint32_t _height;         // 屏幕高度（像素点数）
public:
    DotMatrixScreen(gpio_num_t pin, size_t width, size_t height)
        : WS2812(pin), _width(width), _height(height)
    {
        _pixels.resize(width * height, rgb(0, 0, 0)); // 初始化为黑色
        init(); // 初始化 WS2812
        Matrix_show();
    }
    void setColor(size_t x, size_t y, rgb color);
    inline void Matrix_show()
    {
        show(_pixels); // 显示屏幕内容
    }
};