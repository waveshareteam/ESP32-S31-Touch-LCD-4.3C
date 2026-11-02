# 组件说明

[English](components.md)

16 个 ESP-IDF 工程均在 main/idf_component.yml 中声明托管 waveshare/waveshare_boards，main 依赖 waveshare_boards 并间接使用 Board Manager；不保留 components/esp_boards 副本。

## 准备板级定义

构建与烧录的统一步骤见 [ESP-IDF 使用说明](../examples/esp-idf/README_ZH.md#构建与烧录)。已发布到 ESP Component Registry 的组件必须包含 esp32_s31_touch_lcd_4_3c；CI 不复制本地板型，也没有缺失板型时的回退定义。

首次配置用于下载依赖，之后 bmgr 生成当前工程的 components/gen_bmgr_codes；修改板级 YAML 后需重新生成，不能复制其他示例的生成文件。构建输出、生成组件、托管依赖和锁文件均为本地状态。

## 本地适配层

02、03、04、09、10、13 和 16 示例保留 s31_board_support，用于通用 Board Manager API 以外的产品操作。15 示例保留应用专用 A2DP 及界面组件，各示例仍通过托管板包获取 Board Manager。

## 显示及音频版本

13、14、15 示例将 esp_lvgl_adapter 固定为 0.6.0；使用 LVGL 的四个示例均固定为 9.5.0。16 示例直接使用 LVGL，不使用 adapter。

Board Manager 的音频路径依赖 esp_codec_dev 2.x API，在当前依赖组合下不能直接替换为 1.x 稳定版。

## 验证范围

各示例在自己的清单中声明直接依赖。编译成功只验证源码及工具链集成；引脚和电气行为应结合 [原理图](../hardware/schematic/)、托管 YAML、生成配置和实板测试核实。
