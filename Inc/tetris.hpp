#pragma once

#include "Dot_matrix_screen.hpp"
#include <vector>
#include <chrono>
#include <thread>
#include <driver/gpio.h>
#include <random>
#include <algorithm>
#include "esp_log.h"

using namespace std;

extern uint8_t buf[256];

// 方块形状定义，每个三维vector表示一种方块的所有旋转状态
const vector<vector<vector<int>>> SHAPES = {
    // I
    {{0,0,0,0},
     {1,1,1,1},
     {0,0,0,0},
     {0,0,0,0}},
     // J
     {{1,0,0},
      {1,1,1},
      {0,0,0}},
      // L
      {{0,0,1},
       {1,1,1},
       {0,0,0}},
       // O
       {{1,1},
        {1,1}},
        // S
        {{0,1,1},
         {1,1,0},
         {0,0,0}},
         // T
         {{0,1,0},
          {1,1,1},
          {0,0,0}},
          // Z
          {{1,1,0},
           {0,1,1},
           {0,0,0}}
};

// 方块颜色字符（命令行中使用不同字符表示不同颜色）
const vector<rgb> COLORS = { rgb(0, 255, 238), rgb(17, 0, 255), rgb(255, 111, 0), rgb(255, 242, 0), rgb(0, 255, 8), rgb(255, 0, 255), rgb(255, 0, 0) };

// 方块类，包含形状、颜色、位置、旋转等信息
class Tetromino {
public:
    Tetromino() {
        reset();
    }

    void reset() {
        // 随机选择方块形状
        static std::mt19937 rng(std::random_device{}());
        static int last_type = 0;
        type = rng() % SHAPES.size();
        if(type == last_type)
            type++;
        if(type > SHAPES.size())
            type = 0;
        last_type = type;
        shape = SHAPES[type];
        color = COLORS[type];

        // 初始位置
        x = 5 - shape[0].size() / 2;
        y = 0;
        rotation = 0;
    }

    void rotate() {
        vector<vector<int>> rotated(shape[0].size(), vector<int>(shape.size()));

        for (size_t i = 0; i < shape.size(); i++) {
            for (size_t j = 0; j < shape[0].size(); j++) {
                rotated[j][shape.size() - 1 - i] = shape[i][j];
            }
        }

        shape = rotated;
        rotation = (rotation + 1) % 4;
    }

    int getX() const { return x; }
    int getY() const { return y; }
    int getType() const { return type; }
    int getRotation() const { return rotation; }
    rgb getColor() const { return color; }
    const vector<vector<int>>& getShape() const { return shape; }

    void move(int dx, int dy) {
        x += dx;
        y += dy;
    }

private:
    int x, y;
    int type;
    int rotation;
    rgb color;
    vector<vector<int>> shape;
};

class Game {
private:
    int gridWidth;                   // 游戏区域宽度
    int gridHeight;                  // 游戏区域高度
    int score;                       // 当前分数
    int level;                       // 当前等级
    int linesCleared;                // 已消除的行数
    DotMatrixScreen screen;          // 点阵屏幕对象
    Tetromino current;               // 当前下落的方块
    Tetromino next;                  // 下一个方块
    vector<vector<rgb>> grid;        // 游戏区域网格，记录每个格子的颜色
    vector<vector<rgb>> displayBuffer; // 显示缓冲区，用于渲染
    bool gameOver;                   // 游戏结束标志
    int fallDelay;                   // 方块下落延迟（毫秒）

public:
    // 构造函数，初始化游戏参数和屏幕
    Game()
        : gridWidth(16), gridHeight(16),
        score(0), level(1), linesCleared(0),
        screen((gpio_num_t)48, 16, 16) // 这里传递参数给 DotMatrixScreen 构造函数
    {
        gameOver = false;
        grid = vector<vector<rgb>>(gridHeight, vector<rgb>(gridWidth, rgb(0,0,0)));
        displayBuffer = vector<vector<rgb>>(gridHeight, vector<rgb>(gridWidth, rgb(0,0,0))); // 新增
        reset();
    }
    // 重置游戏状态
    void reset() {
        for(auto &i : grid)
        {
            std::fill(i.begin(), i.end(), rgb(0,0,0));
        }
        for(auto &i : displayBuffer)
        {
            std::fill(i.begin(), i.end(), rgb(0,0,0));
        }

        // 创建新方块
        current.reset();
        next.reset();

        score = 0;
        level = 1;
        linesCleared = 0;
        gameOver = false;

        // 设置下落速度（毫秒）
        fallDelay = 300;
    }
    // 游戏主循环
void run() {
    auto lastFallTime = chrono::steady_clock::now();
    while (!gameOver) {
        // 处理输入
        processInput();
        // 更新游戏状态
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastFallTime).count();
        if (elapsed >= fallDelay) {
            if (!moveCurrent(0, 1)) {
                // 无法继续下落，锁定方块
                lockCurrent();
                clearLines();
                // 创建新方块
                current = next;
                next.reset();
                // 检查游戏结束
                if (!isValidPosition(current.getX(), current.getY(), current.getShape())) {
                    gameOver = true;
                }
            }
            lastFallTime = now;
        }
        // 渲染游戏
        render();
        // 控制游戏速度
        std::this_thread::sleep_for(chrono::milliseconds(10));
    }
}

private:
    // 处理输入缓冲区的按键
    void processInput() {
        if (buf[0]) {
            handleKey(buf[0]);
            buf[0] = 0;
        }
    }
    // 按键处理逻辑
    void handleKey(char c) {
        if (gameOver) return;

        switch (c) {
        case 'a': // 左移
        case 'A':
        case 75:  // Windows左箭头
            moveCurrent(-1, 0);
            break;

        case 'd': // 右移
        case 'D':
        case 77:  // Windows右箭头
            moveCurrent(1, 0);
            break;

        case 's': // 加速下落
        case 'S':
        case 80:  // Windows下箭头
            moveCurrent(0, 1);
            break;

        case 'w': // 旋转
        case 'W':
        case 72:  // Windows上箭头
            rotateCurrent();
            break;

        case ' ': // 硬降落
            hardDrop();
            break;

        case 'q': // 退出
        case 'Q':
            gameOver = true;
            break;

        case 'r': // 重新开始
        case 'R':
            reset();
            break;
        }
    }
    // 尝试移动当前方块，dx为横向移动，dy为纵向移动，返回是否移动成功
    bool moveCurrent(int dx, int dy) {
        int newX = current.getX() + dx;
        int newY = current.getY() + dy;
        // 检查新位置是否合法（不越界且不重叠）
        if (isValidPosition(newX, newY, current.getShape())) {
            current.move(dx, dy);
            return true;
        }
        return false;
    }
    // 尝试旋转当前方块，如果旋转后位置合法则应用旋转
    void rotateCurrent() {
        Tetromino rotated = current;
        rotated.rotate();
        // 检查旋转后的位置是否合法
        if (isValidPosition(rotated.getX(), rotated.getY(), rotated.getShape())) {
            current = rotated;
        }
    }
    // 方块硬降落到底部，直到不能再下落为止
    void hardDrop() {
        while (moveCurrent(0, 1)) {
            // 持续下落直到不能移动
        }
    }
    // 检查指定位置和形状的方块是否可以放置（不越界且不与已有方块重叠）
    bool isValidPosition(int x, int y, const vector<vector<int>>& shape) {
        for (size_t i = 0; i < shape.size(); i++) {
            for (size_t j = 0; j < shape[i].size(); j++) {
                if (shape[i][j]) {
                    int gridX = x + j;
                    int gridY = y + i;
                    // 检查边界
                    if (gridX < 0 || gridX >= gridWidth || gridY >= gridHeight) {
                        return false;
                    }
                    // 检查与已有方块的碰撞
                    if (gridY >= 0 && grid[gridY][gridX] != rgb(0,0,0)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }
    // 将当前方块锁定到网格
    void lockCurrent() {
        const auto& shape = current.getShape();
        for (size_t i = 0; i < shape.size(); i++) {
            for (size_t j = 0; j < shape[i].size(); j++) {
                if (shape[i][j]) {
                    int gridX = current.getX() + j;
                    int gridY = current.getY() + i;

                    if (gridY >= 0) { // 确保在网格范围内
                        grid[gridY][gridX] = current.getColor();
                    }
                }
            }
        }
    }
    // 检查并消除已满的行，更新分数和等级
void clearLines() {
    int linesClearedThisMove = 0;
    for (int i = gridHeight - 1; i >= 0; i--) {
        bool lineComplete = true;
        for (int j = 0; j < gridWidth; j++) {
            if (grid[i][j] == rgb(0, 0, 0)) { // 检查是否有空格
                lineComplete = false;
                break;
            }
        }
        if (lineComplete) {
            // 移除该行
            for (int k = i; k > 0; k--) {
                grid[k] = grid[k - 1];
            }
            // 顶部行设为空
            grid[0] = vector<rgb>(gridWidth, rgb(0, 0, 0));
            // 增加计数器
            linesClearedThisMove++;
            i++; // 重新检查当前行
        }
    }
    // 更新分数
    if (linesClearedThisMove > 0) {
        // 消行得分：1行=100，2行=300，3行=500，4行=800
        const int points[5] = { 0, 100, 300, 500, 800 };
        score += points[linesClearedThisMove] * level;
        linesCleared += linesClearedThisMove;
        // 每消10行升一级
        level = linesCleared / 10 + 1;
        // 提高下落速度
        fallDelay = 500 - (level - 1) * 40;
        if (fallDelay < 100) fallDelay = 100;
        ESP_LOGI("分数","%d", score);
    }
}
    // 渲染当前游戏状态到屏幕
    void render() {
        // 更新显示缓冲区
        updateDisplayBuffer();
        // 打印显示缓冲区
        printDisplayBuffer();
    }

    void updateDisplayBuffer() {
        // 清空缓冲区
        for (int i = 0; i < gridHeight; i++) {
            for (int j = 0; j < gridWidth; j++) {
                displayBuffer[i][j] = grid[i][j];
            }
        }

        // 将当前方块添加到缓冲区
        const auto& shape = current.getShape();
        int cx = current.getX();
        int cy = current.getY();

        for (size_t i = 0; i < shape.size(); i++) {
            for (size_t j = 0; j < shape[i].size(); j++) {
                if (shape[i][j]) {
                    int x = cx + j;
                    int y = cy + i;

                    if (y >= 0 && y < gridHeight && x >= 0 && x < gridWidth) {
                        displayBuffer[y][x] = current.getColor();
                    }
                }
            }
        }
    }
    void printDisplayBuffer() {
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                screen.setColor(x, 15 - y, displayBuffer[y][x]);
            }
        }
        screen.Matrix_show(); // 显示屏幕内容
    }
};
