# WLED2 项目

## 项目概述
WLED2 是一个基于 ESP32 的项目，主要功能是在点阵屏幕上运行俄罗斯方块游戏，同时支持 BLE（蓝牙低功耗）通信。通过 BLE 可以与外部设备进行数据交互，比如接收亮度调节指令。

## 硬件要求
- **开发板**：ESP32 系列开发板
- **点阵屏幕**：支持 WS2812 协议的点阵屏幕
- **其他**：蓝牙接收设备（如手机）用于 BLE 通信

## 软件依赖
- **ESP-IDF**：版本 5.4.0
- **NimBLE**：用于 BLE 通信
- **FreeRTOS**：实时操作系统

## 编译与运行

### 环境准备
1. 安装 ESP-IDF 开发环境，版本 5.4.0。具体安装步骤可参考 [ESP-IDF 官方文档](https://docs.espressif.com/projects/esp-idf/en/v5.4.0/esp32/get-started/index.html)。
2. 配置 ESP-IDF 环境变量。

### 编译项目
1. 打开终端，进入项目根目录 `d:\code\mcu\esp32\WLED\WLED2`。
2. 运行以下命令初始化项目：
```bash
idf.py set-target esp32s3
```
其中PORT是 ESP32 开发板对应的串口端口号。

## 运行项目
烧录完成后，重启 ESP32 开发板，游戏将自动启动。同时，BLE 服务也会启动，可使用支持 BLE 的设备进行连接。

## 功能特性
- 俄罗斯方块游戏
- 在点阵屏幕上显示游戏界面
- 随机生成不同形状的方块
- 支持方块的移动、旋转操作
- 消行得分机制，每消 10 行提升游戏等级
- 显示当前分数和等级

## BLE 通信
- 作为 BLE 服务端，支持设备连接
- 接收外部设备的亮度调节指令
- 将串口数据通过 BLE 通知给客户端

代码结构

```plainText
WLED2/
├── .git/                  # Git 版本控制目录
├── .gitignore             # Git 忽略文件配置
├── CMakeLists.txt         # 项目 CMake 配置文件
├── Inc/                   # 头文件目录
│   ├── Dot_matrix_screen.hpp # 点阵屏幕控制头文件
│   ├── bluetooth.h        # BLE 通信头文件
│   ├── tetris.hpp         # 俄罗斯方块游戏逻辑头文件
│   └── ws2812.hpp         # WS2812 灯带控制头文件
├── Src/                   # 源文件目录
│   ├── Dot_matrix_screen.cpp # 点阵屏幕控制源文件
│   ├── bluetooth.c        # BLE 通信源文件
│   └── ws2812.cpp         # WS2812 灯带控制源文件
├── dependencies.lock      # 项目依赖锁定文件
├── main/                  # 主程序目录
│   ├── CMakeLists.txt     # 主程序 CMake 配置文件
│   ├── Kconfig.projbuild  # 项目配置文件
│   ├── idf_component.yml  # 组件依赖配置文件
│   └── main.cpp           # 主程序入口文件
├── sdkconfig              # SDK 配置文件
├── sdkconfig.defaults     # SDK 默认配置文件
└── sdkconfig.old          # 旧的 SDK 配置文件
```