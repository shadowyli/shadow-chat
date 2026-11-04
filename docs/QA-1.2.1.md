# 影聊 1.2.1 QA 状态

状态：**开发验证进行中，真实三机验收待执行**。

当前三机候选安装包：`ShadowChat-1.2.1-local-setup.exe`

- 大小：`61,011,455` 字节
- SHA-256：`32d4d9e4dceba5d4b47a8dd118e05a9486b89e128c546b5b3bb0c3f6042a4ad9`
- 生产 EXE SHA-256：`926e86c24ad29ea1da5c86bfe0d0a254a05eec047fce8d5e5aaef169a0af05b0`
- 测试交付目录：`E:\ShadowChat\delivery\ShadowChat-1.2.1-three-pc-candidate`

## 已有证据

- 1.2.0 DEV 正确 API 下载文件：`60,998,472` 字节，SHA-256 `ea109bb321b2518a7ee67ee6b33658901b536c20b6bc09b1a0a55330011052e6`。
- 当前机对该文件完成 QA 隔离安装，`--build-info` 返回 `1.2.0 / local / test`，真实登录窗口可见。
- 当前机下载目录中的同名文件通过 Windows ShellExecute 可弹出安装窗口；历史失败暂未稳定复现。
- 当前机开始菜单历史快捷方式指向 `E:\software\ShadowChat\ShadowChat.exe`，其 SHA-256 与 1.1.8、1.1.9、1.2.0 均不匹配，不得作为 1.2.0 或 1.2.1 结论。
- `tests/discovery_identity_contract_test.py` 通过，证明源码包含影聊广播标记、握手拒绝、用户列表过滤和缓存过滤契约。
- 1.2.1 候选在当前机完成真实 QA 隔离安装，`--build-info` 返回 `1.2.1 / local / test`，登录窗口可见；卸载清理成功，真实注册表、启动项和快捷方式前后不变。
- 最近私聊列表、按 chat id 打开私聊、消息回调和发送前检查已增加影聊身份门禁，旧缓存用户不能作为可用聊天入口。

## 最终交付门禁

- 使用最终 1.2.1 安装包完成当前机全新安装、覆盖安装、卸载重装和双击启动。
- A、B、C 三台真实电脑使用同一 SHA-256 包体完成六向发现和六向聊天。
- BeeBEEP／旧同源客户端不得出现在通讯录、搜索、最近联系人、头像列表和新建聊天入口。
- 收齐三台电脑的截图和诊断 ZIP，并填写 `QA-1.2.1-REAL-3PC.md`。
- 真实三机未完成前，不得创建 `v1.2.1-test` Release，也不得把辅助检查写成已通过三机验收。
