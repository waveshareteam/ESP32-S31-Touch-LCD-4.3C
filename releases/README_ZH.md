# 固件版本发布

[English](README.md)

向 GitHub 推送附注版本标签后，工作流会编译全部 16 个 ESP-IDF 示例，
校验固件包，创建 Release 草稿并上传附件；远端附件校验通过后自动公开。
每个版本包含 **16 个固件 ZIP 和 1 个 SHA256SUMS**。

## 首次发布前

1. 在 ESP Component Registry 发布包含 `esp32_s31_touch_lcd_4_3c` 的
   `waveshare/waveshare_boards` 组件版本。仅向组件 Git 仓库提交板级目录还不够，
   发布包中也必须包含它。本产品仓库不保存板级副本。
2. 将产品仓库的源码和工作流提交到 GitHub 默认分支。
3. 在 GitHub 仓库启用 Actions。工作流使用内置 `github.token`，仅发布任务申请
   `contents: write`，无需配置个人 Token。

工具链、板名和关键依赖版本固定在 [`config.json`](config.json)。
`waveshare_boards` 按示例清单的 `"*"` 解析；每个固件包都会记录实际组件版本、
组件哈希和板级文件哈希。同一版本的 16 个构建需使用一致的组件和板级定义。

## 发布一个版本

先确认当前提交已进入 GitHub 默认分支，再创建尚未使用过的附注标签。
以下命令适用于 Ubuntu Bash，也适用于 Windows PowerShell：

```bash
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

`origin` 必须指向 GitHub，或通过 GitLab 镜像把标签同步到 GitHub。
如果标签只存在于 GitLab，GitHub 工作流不会启动。

在 **Actions → ESP32-S31 firmware and releases** 查看进度。
构建和校验全部通过后，进入 **Releases** 下载对应示例的固件 ZIP。
GitHub 自带的 **Source code** 压缩包是源码，不是可烧录固件。

仅接受 `v主版本.次版本.补丁版本` 格式，例如 `v1.0.0`、`v1.0.1`、`v1.1.0`。
标签需由 `git tag -a` 创建，并指向默认分支可达的提交。
不要提前手动创建公开 Release，也不要移动已公开版本的标签。

Release 按正式版本发布并标记为 **Latest**。ESP32-S31 当前使用固定的
ESP-IDF 工具链。产品版本、ESP-IDF 提交及组件版本会分别记录，
便于回溯。编译通过不代表已经完成实板测试。

## 失败与重跑

- 任一示例编译失败，或缺少附件、目标芯片不符、源码提交不符、SHA256 不符，
  都会阻止公开发布。
- 草稿上传失败后，可以在 Actions 中重跑失败任务；工作流会重新上传并核对草稿附件。
- 已公开的 Release 不会被覆盖。修复后应创建新的补丁版本，例如 `v1.0.1`。
- 组件包缺少本板时，先发布新的 `waveshare_boards` 组件版本，再重跑构建。
- 构建失败日志保存在对应 Actions 运行的 `diagnostics-*` 附件中。

## 单独编译与下载

普通分支、PR 和手动运行只生成 Actions 附件，不会自动创建 Release。
手动 **Run workflow** 可选择 `all`、示例名或仓库相对路径。

在已激活配置中指定的 ESP-IDF 环境后，从仓库根目录执行（Ubuntu Bash）：

```bash
python -m pip install "idf-component-manager==3.1.2"
python scripts/build_example.py 01_i2c_scan
python releases/package_release.py 01_i2c_scan
```

发布打包入口要求源码已提交。每个 ZIP 包含原始分段固件、从 `0x0` 烧录的
合并镜像、`manifest.json`、`SHA256SUMS`、`flash.sh`、`flash.bat` 和烧录参数。
工具会保留 ESP32-S31 的 `--no-stub` 等实际构建参数。
合并镜像结束于最后一个分段，不填充到整个 Flash 容量。

解压后的烧录命令（会写入开发板 Flash）：

Ubuntu Bash：

```bash
./flash.sh /dev/ttyACM0
```

Windows PowerShell：

```powershell
.\flash.bat COM3
```

请使用支持 ESP32-S31 的 esptool，并替换实际串口。
仓库随附的全 Flash 测试镜像不属于这 16 个源码构建产物，不会被自动重新发布。
