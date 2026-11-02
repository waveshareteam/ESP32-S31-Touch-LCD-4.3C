# 贡献指南

[English](CONTRIBUTING.md)

本仓库面向 ESP32-S31-Touch-LCD-4.3C，目前提供 16 个独立 ESP-IDF 工程，不包含 Arduino 示例。

## 修改范围

保持修改聚焦，并使用仓库相对路径。各示例通过 waveshare/waveshare_boards 加载板级定义，不应依赖相邻示例或复制其他示例生成的代码；准备步骤见 [组件说明](docs/components_ZH.md)。

涉及引脚、时钟、总线、供电、显示、触摸、音频、存储、USB、RS485 或 CAN-FD 的修改，应先对照 [原理图](hardware/schematic/) 和托管板级定义核实。

## 验证及合并

使用明确支持 esp32s31 的 ESP-IDF，并记录准确提交；板级 YAML 变化后，为相关示例重新运行 `idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c`。硬件修改需要记录板卡版本和实测结果，编译成功不能替代硬件验证。

将分支推送到主仓库 GitLab 并提交合并请求，GitHub 由镜像同步。说明受影响路径、工具链提交、构建命令、实际验证、硬件及固件影响和未验证项；可附串口日志或界面截图。

## 公开内容

不要提交凭据、本机路径、用户名、真实端口或客户数据。firmware 中的随附测试镜像与源码构建示例分开管理，不应将其描述为 CI 构建的出厂固件。
