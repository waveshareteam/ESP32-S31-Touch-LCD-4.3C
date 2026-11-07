# Codec SD WAV Player

[English](README.md)

## 功能

从 TF 卡读取 16 位 PCM WAV 文件，通过 ES8389 播放，单声道会转换为立体声。

## 硬件需求

开发板、USB 数据线、FAT 格式 TF 卡及接入扬声器接口的扬声器。

## 怎么运行

按 [ESP-IDF 使用说明](../README_ZH.md#构建与烧录) 准备环境，在本示例目录执行以下命令，将 `PORT` 替换为设备端口。

```bash
python -m pip install esp-bmgr-assist
idf.py bmgr -b esp32_s31_touch_lcd_4_3c
idf.py build
idf.py -p PORT flash monitor
```

## 预期现象

扬声器循环播放 test.wav，未找到时使用卡根目录中的第一个 WAV 文件。

## 常见问题

运行前将随附 test.wav 复制到卡根目录；测试音及立体声扫频按 [CC0-1.0](test.wav.LICENSE.txt) 提供，可用 generate_test_wav.py 重建；不支持压缩音频及非 16 位 PCM 文件。
