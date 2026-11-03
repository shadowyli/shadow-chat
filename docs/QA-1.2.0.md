# 影聊 1.2.0 QA 结果

状态：**自动验收通过，可进入测试版发布**。

本次冻结 run：`20261103T153226Z-7b57970c`

构建提交：`24da404fb75cec5cb2f0eef849b65c7c8bf6910f`

构建环境：`local/test`、Windows 10 x86、Qt 5.15.2、MSVC x86

生产 EXE SHA-256：`dda9ba3c8d466ac606dfe6ec738ea7d5b3893ab831943343f79bdf20af3d1fb0`

安装包 SHA-256：`ea109bb321b2518a7ee67ee6b33658901b536c20b6bc09b1a0a55330011052e6`
安装包大小：`60,998,472` 字节

## 自动结果

| 检查项 | 结果 | 证据 |
| --- | --- | --- |
| 全量生产构建 | 通过 | `build-passed.json` |
| TLS 正负探针 | 通过 | `tls-probe-passed.json`、`tls-passed.json` |
| 安装包生成 | 通过 | `package-passed.json`、`dist/local/release-build.json` |
| 常规回归 | 通过 | `general-passed.json` |
| 原生 Qt Windows UI | 通过 | `native-passed.json`，100%／150%／200% |
| 隔离安装与覆盖升级 | 通过 | `install-passed.json`，1.1.8→1.2.0、1.1.9→1.2.0 |

原生矩阵覆盖更新、登录、资料、表情、图片气泡、上下文跳转、头像资料卡、弹层裁切、会话提示、排版动画、视频预览、历史面板、传输、启动偏好和开发预览。失败的早期候选均已留档，不作为本版交付物。

## 已验证行为

- 更新检查支持公开备用查询；版本、通道、环境、文件名、大小和 SHA-256 不一致时不会误报最新版。
- 设置内检查更新立即显示状态；建议更新只在发现更高版本时每天最多提示一次，失败不消耗提醒机会。
- 资料、生日空态与日历、锚定弹层、圆角阴影、登录后自动创建账号、静态提醒和默认开机启动已验证。
- 100%／150%／200% 缩放下资料、日历、菜单、表情、历史、视频和弹层边界通过。
- 安装测试验证首次默认开机启动、用户关闭偏好在覆盖升级中保留、重新开启偏好生效、卸载清理，以及账号、聊天记录、附件、待发队列和设置哈希保留。
- 真实用户 Run、注册表、桌面快捷方式和开始菜单状态在隔离安装前后未改变。

## 仍需人工确认

- Windows 原生文件选择窗的实际视觉操作。
- Windows 通知中心最终图标呈现。
- 真实办公网络中的多机发现、收发和防火墙策略。
- 全新 Windows 10／11 机器、虚拟机和物理多显示器跨屏行为。
- 4K 视频及未覆盖编码、容器、损坏媒体的兼容性。

上述项目不是本次自动通过的阻断项，发布说明中不得写成已完成兼容性验收。

## 证据位置

- 当前 run：`dist/release-agent/runs/20261103T153226Z-7b57970c`
- 原生报告：`dist/local/release-tests/run-<id>/verification.json`
- 安装报告：当前 run 的 `steps/install-*/install-from-1.1.8.json` 和 `install-from-1.1.9.json`
- 构建与包体：`dist/local/build-configuration.json`、`dist/local/build-source-inputs.json`、`dist/local/release-build.json`

最终文档提交只允许更新 `docs/` 和根 `README.md`；归档前由发布脚本再次核对生产输入、EXE 和安装包哈希未变。
