# 持续集成

[English](ci.md)

## 仓库检查

.github/workflows/repository-checks.yml 检查 Python 语法、示例发现、发布工具测试及模拟固件打包，不需要 ESP-IDF 或连接硬件。

## 构建与发布

.github/workflows/build.yml 负责 16 个 ESP-IDF 示例的构建。main、master 和拉取请求中相关源码或工具变更构建全部示例，并上传 Actions 附件；手动运行可选择 all、示例名或仓库相对路径，只构建和上传所选示例。

向 GitLab 推送附注格式 v主版本.次版本.补丁版本 标签并镜像到 GitHub 后，工作流构建全部示例、校验完整附件矩阵、上传 Release 草稿、校验远端名称/大小/摘要，再自动公开为正式版本并设为 Latest。标签提交必须可从默认分支到达；任一构建或校验失败都阻止公开发布。

草稿可重跑并替换附件，已经公开的版本不可覆盖。发布后还会再次下载校验；Release 包含 16 个固件 ZIP 及 SHA256SUMS，不重新发布仓库随附测试镜像。

## 固定工具链

[构建配置](../releases/config.json) 记录准确的 ESP-IDF 提交、Component Manager 版本、板名和组件约束。构建脚本核对工具链，解析托管组件并生成板级配置；所有矩阵任务必须使用相同的依赖版本/哈希和板级文件哈希。

每个固件清单记录源码、工具链及依赖身份。第三方 Actions 固定到完整提交，工作流默认只读，仅发布任务使用内置令牌获得写入权限。

发布步骤见 [发布说明](../releases/README_ZH.md)，模板迁移的未解决能力差异见 [迁移记录](template-migration_ZH.md)。构建成功不能替代硬件测试。
