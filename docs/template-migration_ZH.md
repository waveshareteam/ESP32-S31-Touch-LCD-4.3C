# 模板迁移记录

[English](template-migration.md)

## 已采用的规则

本次对照 waveshareteam/esp32-repo-template 的 v1，提交 c576a0a2d2d21c76fe05d84bb1ce1c3261bf4c44，整理了硬件目录、首页及双语文档、示例固定五节说明、构建工作流命名和忽略规则。16 个示例的源码、CMake、依赖清单、sdkconfig、分区及本地组件保持不变。

## 共享 CI 的接入前提

当前保留原有构建和发布工具，没有创建不符合实际工具链的 product.json，也没有替换成已知无法构建本产品的共享工作流。需要模板维护者 Wulu 协助解决：

1. **ESP-IDF 版本表达**：当前示例要求 >=6.2，构建固定到 [config.json](../releases/config.json) 的提交；模板仅接受正式版本标签，并从 espressif/idf 对应标签镜像构建，不能表达该提交及其工具链。
2. **Board Manager 生成**：新检出需要先解析托管依赖，再运行 bmgr 生成 gen_bmgr_codes，最后编译。模板当前只运行 set-target 和 build，没有对应生成流程。
3. **S31 烧录参数**：现有打包器保留 flasher_args.json 中 stub=false 对应的 --no-stub；模板打包器没有传递此参数。
4. **发布行为**：现有流程远端校验成功后自动公开正式版本并设为 Latest；模板生成草稿供人工发布。切换前需明确最终采用的发布方式。
5. **无 Arduino 的文档检查**：模板规范允许仅 ESP-IDF 产品，但默认首页检查及契约仍强制 Arduino 快捷链接；本仓库没有该框架，不添加空目录或无效链接。

上述能力具备后，才能按真实工具链填写 product.json、切换共享工作流，并在新提交上验证全部 16 个示例；通过前不声明达到 L1/L2。

## 暂时保留的内容

- releases、scripts、tests 及相关维护文档保留到共享 CI 能完整承接为止，避免丢失现有构建、打包及发布校验。
- GitHub 仓库当前为私有，组织公共社区文件的自动继承不适用，因此保留并补齐本地协作模板及贡献、支持、安全文档。
- docs/hardware 保留之前整理的引脚及图纸核对信息；运行时引脚配置仍以托管 BSP 为准。
- firmware 中的随附测试镜像来源、构建日期及出厂身份尚未确定，保持原名及内容，不编造 FactoryOnly 名称或日期；该镜像不作为 Release 附件。
- hardware/mechanical 中的 S3 文件仅移动目录，不改产品型号或宣称为 S31 已核实尺寸。

## 链接与仓库设置

中文商品页已核对到本产品及 N16R16 规格；中文文档候选地址返回通用首页，英文站请求受限，Gitee issue 页面未能在浏览器确认，因此不添加未经确认的 Wiki 或 Gitee 链接，支持入口使用微雪官方工单。

GitHub About 尚未修改；建议经授权后填写：

- Description：Waveshare ESP32-S31 4.3-inch 800 x 480 RGB capacitive touch development board: ESP-IDF examples, firmware and hardware resources
- Website：https://www.waveshare.net/shop/ESP32-S31-Touch-LCD-4.3C.htm
- Topics：waveshare、esp32、esp32-s31、esp32-s31-touch-lcd-4-3c、esp-idf

GitLab 是主仓库，所有分支及标签向 GitLab 推送，再由镜像同步到 GitHub；不直接向 GitHub 推送源码。

## 本次验证

- 发布工具 24 项测试全部通过，发现全部 16 个 ESP32-S31 工程。
- 与整改前基线比对，142 个源码、配置、固件及硬件资料文件的 SHA256 一致，硬件资料仅目录发生变化；16 对示例 README 均为规定的五节。
- 64 份 Markdown 使用默认配置审计，仅剩中英文首页缺少 Arduino 快捷链接两项错误，无其他问题；临时移除这一不适用要求后的严格审计为 0 错误、0 警告，未修改模板默认配置。
- 模板测试共 41 项，使用 unittest 执行同一套测试：38 项通过，3 项因缺少 product.json 报错；指定的 pytest 命令因本机未安装 pytest 无法启动。
- Git 差异空白检查通过。未进行本地固件编译、烧录或新提交的远端 CI，不把历史结果当作当前提交的构建结论。
