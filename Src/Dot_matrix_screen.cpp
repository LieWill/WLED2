#include "Dot_matrix_screen.hpp"

extern uint8_t light;

void DotMatrixScreen::setColor(size_t x, size_t y, rgb color)
{
    // 设置亮度
    color.green = color.green * light / 255.0;
    color.red = color.red * light / 255.0;
    color.blue = color.blue * light / 255.0;
    if (y % 2 == 0)
    {
         _pixels[y * _width + x] = color; // 偶数正序列
    }
    else
    {
        _pixels[y * _width + (15 - x)] = color; // 奇数逆序列
    }
}
