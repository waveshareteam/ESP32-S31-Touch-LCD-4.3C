# USB Camera

[English](README.md)

## 功能

接收外接 USB UVC 摄像头的 MJPEG 视频，解码后在 LCD 上显示预览。

## 硬件需求

开发板、USB 数据线及支持兼容分辨率 MJPEG 输出的 USB UVC 摄像头。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## 预期现象

摄像头支持对应模式时，640 × 480 图像居中显示在 800 × 480 屏幕上，串口输出所选视频格式及预览帧率。

## 常见问题

首选模式失败时会尝试640 × 480 下的其他帧率；摄像头需外接，开发板没有 DVP 摄像头接口。
