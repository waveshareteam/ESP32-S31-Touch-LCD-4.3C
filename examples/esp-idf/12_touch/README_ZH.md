# Touch

[English](README.md)

## 功能

读取 GT911 触摸坐标，并在 LCD 上绘制彩色标记。

## 硬件需求

已连接 LCD 及触摸组件的开发板及 USB 数据线。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist==0.8.3
idf.py --preview bmgr -b esp32_s31_touch_lcd_4_3c
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

## 预期现象

触摸屏幕显示标记，点击右上角区域清屏。

## 常见问题

暂无。
