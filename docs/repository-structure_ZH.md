# 仓库结构

[English](repository-structure.md)

## 目录

| 路径 | 内容 |
| --- | --- |
| examples/esp-idf/ | 16 个独立 ESP32-S31 源码工程 |
| hardware/schematic/ | 原理图 |
| hardware/mechanical/ | 随附机械参考资料 |
| firmware/ | 全 Flash 测试镜像及说明 |
| docs/images/ | 产品文档图片 |
| docs/ | 保留的硬件核对、组件及验证说明 |
| releases/ | 源码固件打包及发布工具 |
| scripts/ | 示例发现及构建入口 |
| tests/ | 发布工具验证 |
| .github/ | 构建工作流及私有仓库协作模板 |

当前没有 Arduino 工程，不创建空 Arduino 目录。示例列表及用途见 [示例索引](../examples/esp-idf/README_ZH.md)。

## 示例及板级配置

各示例都在顶层 CMakeLists.txt 声明 IDF_TARGET esp32s31，独立持有本地功能组件及依赖声明。板级定义通过托管 waveshare/waveshare_boards 加载，配置步骤见 [组件说明](components_ZH.md#准备板级定义)。

每个示例分别生成 components/gen_bmgr_codes，修改板级 YAML 后重新生成，不跨示例复制。源码编译及 CI 的工具链以 [构建配置](../releases/config.json) 为准。

## 固件边界与模板差异

firmware 中的随附镜像与 16 个源码工程分开管理，不作为 CI 生成的 Release 附件。模板暂不能承接当前工具链及板级生成流程，因此保留必要工具和记录，详见 [迁移记录](template-migration_ZH.md)。
