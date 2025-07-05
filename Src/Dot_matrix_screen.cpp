#include "Dot_matrix_screen.hpp"

extern uint8_t light;

void DotMatrixScreen::setColor(size_t x, size_t y, rgb color)
{
    color.green = color.green * light / 255.0;
    color.red = color.red * light / 255.0;
    color.blue = color.blue * light / 255.0;
    if (y % 2 == 0)
    {
         _pixels[y * _width + x] = color; // 设置指定位置的颜色
    }
    else
    {
        _pixels[y * _width + (15 - x)] = color; // 设置指定位置的颜色
    }
}
