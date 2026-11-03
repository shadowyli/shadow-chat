# 影聊 1.2.0 更新与开机启动说明

本文供 1.2.0 发布说明直接引用。范围：版本检查、公开备用通路、旧版获取修复的方式、Release 资产要求，以及开机启动默认值。

## 1. 1.2.0 修复了什么

1.1.8 / 1.1.9 的版本检查只请求 GitHub 的 `/repos/shadowyli/shadow-chat/releases` API。该接口未认证时的限额是每 IP 每小时 60 次；办公网经同一 NAT 出口时，几台机器就可能把额度耗尽。额度耗尽后接口返回 403，旧客户端只会记录 `rate limit exceeded`，不会提示 1.1.9 已经发布。

1.2.0 在 API 返回 403、429、5xx、超时或网络错误时，改用无需 Token 的公开通路：

`releases.atom` → 对应标签的 `build-info.json` → 同标签的 `SHA256SUMS.txt`

备用通路会核对版本、标签、通道、构建环境、精确安装包名称、元数据中的正整数大小和 SHA-256，并与校验清单交叉核对摘要；大小须为 JSON 数字，范围为 1–167772160 字节。客户端限制响应大小、总时长和跳转目标；TLS 校验失败时不会降级为明文请求。客户端不内置任何 GitHub 凭据。版本检查只读取发布元信息，下载由浏览器处理，不声称已经下载并校验安装包的实际字节。

API 返回 200 但内容里没有本通道的有效版本时，客户端会再尝试备用通路；两条通路都失败时显示“暂时无法检查更新”，不会显示成“当前已是最新版本”。

同时，自动检查从每 5 分钟一次改为每 6 小时一次（并带随机抖动），当天已经提醒过就不再查询；手动点击“查看最新版本”始终立即执行。API 返回 `Retry-After` 或 `X-RateLimit-Reset` 时，会在退避期内直接使用备用通路。

设置窗口里的“查看最新版本”现在会在设置卡片内立即显示检查状态和结果，并提供可直接操作的“下载安装包”。更新说明放在固定高度、可滚动的只读区域，设置内容可滚动，窗口最小高度按可用屏幕约束。检查期间关闭设置不会占用当天的提醒机会。其他入口和自动提醒仍使用独立的结果对话框。

## 2. 已经安装的 1.1.8 / 1.1.9 如何获得修复

旧版二进制中不包含上述备用通路，也不会自己下载尚未安装的新代码。因此：

- 1.1.8 / 1.1.9 只有在 GitHub API 限额恢复、并且该网络能访问 API 时，才可能提示 1.2.0。
- 如果旧版一直不提示，需要手动从 GitHub Releases 下载 1.2.0 安装包，退出影聊后覆盖安装。
- 覆盖安装按保留现有账号、聊天记录、接收文件和待发队列设计。本机安装组件测试证明合成数据文件在覆盖期间哈希不变；最终发布包读取旧账号、历史与队列的验收仍由主 agent 在最终构建后执行。升级前应退出影聊并备份重要资料。
- 升级后首次启动仍是 1.2.0，后续版本检查才会使用新的备用通路。

不能声称旧程序会自动获得 1.2.0 中尚未安装的代码。

## 3. 后续 Release 的资产要求

备用通路依赖每个 GitHub Release 同时附带以下文件，且文件名、大小和摘要必须与 `build-info.json` 一致：

- `build-info.json`：至少包含 `product`、`version`、`channel`、`releaseEnvironment`、`sourceCommit`、`sourceTag`、`binaryBuildIdentity`（含 `version`/`channel`/`environment`）以及 `installer`（`file`/`bytes`/`sha256`）。
- `SHA256SUMS.txt`：至少包含安装包文件名及对应 SHA-256；如果包含 `build-info.json` 行，其摘要也会被核对。
- 安装包：命名必须是 `ShadowChat-<版本>-<环境>-setup.exe`（例如 `ShadowChat-1.2.0-local-setup.exe`），并发布为同一标签的 Release 资产。

缺少 `build-info.json`（包括 404/410）、缺少 `SHA256SUMS.txt`、文件名不符或摘要不一致时，备用通路会报告无法确认最新版本，并停止本轮检查。即使存在较旧的完整版本，也不能用它覆盖高版本的损坏状态并显示“已是最新版本”。只有通过元信息完整性验证、且确认属于其他环境的候选才会跳过；其他通道由 Atom 标签筛选。API 路径同样不会用损坏高版本下方的旧版本证明“已是最新版本”。发布时必须把三件套作为同一 Release 的完整资产上传。

备用通路只允许元数据和校验清单跳转至 HTTPS 的 `release-assets.githubusercontent.com` 或 `objects.githubusercontent.com`，最多三次；拒绝明文、带凭据、片段、自定义端口或相似域名的跳转。Atom 列表不跟随跳转。所有请求禁用 Cookie，不发送 Authorization 或 Token。

备用请求完成时先停止计时器、终止未结束的 reply、安排 helper 延迟删除，最后调用完成回调。完成信号的接收者可以同步销毁父 Updater；回调之后不再访问 helper。销毁父对象会取消在途请求，不发送取消后的完成信号。

## 4. 开机启动

- 首次安装、且没有历史偏好时，默认勾选“登录 Windows 后自动启动影聊”，安装程序会同时写入 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 和 `StartupEnabled=1`。
- 用户主动关闭后，`StartupEnabled=0` 会在重启和覆盖升级后保留；安装程序不会因为升级重新打开。
- 在设置窗口点击“取消”不会修改启动项；点击“保存”后如果写入失败，会明确提示失败。
- 保存时同时比较复选框与实际启动状态、持久偏好；两者有任一不同就执行写入。这样即使偏好仍为开启、Run 命令却含非法尾参，重新勾选并保存也会修复实际命令。
- 卸载会移除 Run 项和影聊自己的 `HKCU\Software\shadowyli\ShadowChat` 偏好。理由：卸载视为本次安装结束，重新安装回到文档规定的默认值（默认开启）；用户数据目录不会被卸载程序删除。
- `--profile-root`、开发预览和隔离测试不会读写当前 Windows 用户的真实启动项；此时设置中的复选框会显示为不可用，避免“显示已勾选但实际未生效”。

## 5. 更新专项验证记录（2026-11-03）

- 使用 Qt 5.15.2 MSVC2019 x86 Kit 和 `scripts/dev-preview/runner.py` 导入的本机编译环境，完成独立全应用构建。构建信息为 `1.2.0 / test / local`，正常更新源为 GitHub，PE 架构为 x86。
- `github_release_test` 通过；覆盖版本顺序、通道、环境、精确包名、正整数大小、SHA-256、仓库地址限制，以及损坏高版本不能被旧版本覆盖。
- `update_network_test` 使用真实 QTcpServer/QNetworkAccessManager loopback 请求，覆盖 403、429、超时、退避、晚到响应、错误元信息、404 与旧版本并存、大小和摘要错误、恶意跳转、TLS 不受信证书、父对象同步销毁、在途取消和失败后重试。所有场景通过，无 Token。
- `update_gate_test` 在 offscreen 100% / 150% / 200% 每档为 34 通过、0 失败、1 跳过。跳过项是由跨进程测试单独调用的 child 入口，跨进程持久化测试本身已通过。测试进入真实 `GuiMain::showShadowSettings()` 模态窗口，验证即时状态、下载与重试按钮、长说明滚动区域、关闭在途检查不 claim、已有结果窗与在途请求的 inline 接管；同时覆盖每日提醒、重启、回拨、并发 claim 和按通道持久化。
- 启动 QA 补充指出的非法 Run 命令保存问题已修复。复用其现有 `startup_preferences_test --verify-invalid-command-repair`，以最新 GuiMain 对象重新编译，在 offscreen 和独立注册表子树下通过；日志为 `dist/update-qa/startup_preferences_test-invalid-command-repair.log`。未修改启动 QA 的历史失败报告，最终主副本仍需执行原生 probe。
- 日志、界面截图、构建身份和源码摘要在本工作区 `dist/update-qa/`；汇总文件为 `verification.json`。未提交 commit。原生 Windows 焦点与 100% / 150% / 200% 最终验收按协作安排留给主 agent 在整合后 fresh build 串行执行，不能将本次 offscreen 结果写成原生 Windows UI 已验收。
