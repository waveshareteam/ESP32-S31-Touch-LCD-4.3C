# LVGL Porting

[English](README.md)

## 功能

将托管 LCD、触摸及背光接入 esp_lvgl_adapter，运行随附界面与媒体演示。

## 硬件需求

已连接 LCD 及触摸组件的开发板及 USB 数据线。

## 怎么运行

先按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备固定版本工具链、解析依赖并生成本示例的板级配置，然后编译烧录。

## 预期现象

LCD 显示 LVGL 演示界面并响应触摸。

## 常见问题

清单固定 esp_lvgl_adapter 为 0.6.0、LVGL 为 9.5.0，升级时需一起核对兼容性。
