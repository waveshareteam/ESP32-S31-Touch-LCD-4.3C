# RGB LCD

[English](README.md)

## 功能

通过 PSRAM 双 RGB 帧缓冲及整帧切换显示测试图案。

## 硬件需求

已连接 LCD 的开发板及 USB 数据线。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## 预期现象

800 × 480 屏幕循环显示纯色、彩条、棋盘格、渐变及几何图案。

## 常见问题

暂无。
