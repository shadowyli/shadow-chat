# 1.2.0 旧版 profile 实际读取 QA

日期：2026-11-03。测试文件：

- `tests/upgrade_profile_test.cpp`
- `tests/upgrade_profile_test.pro`

本测试不修改已冻结的 Settings、NSIS、安装验证器或启动测试文件。

## 运行方式

同一临时 fixture 使用以下顺序：

1. 1.1.9 生产对象运行 `seed`，通过实际账号、历史、未发消息和设置写入 API 生成数据。
2. 1.1.9 生产对象运行 `verify`，确认旧版能够读取自己写出的 profile。
3. 1.2.0 生产对象运行 `verify`，确认本轮对象能够读取同一旧版 profile。

fixture 路径：

`C:\Users\N32962\AppData\Local\Temp\ShadowChatUpgradeProfile-424c9e3cd06b95aec13cf357b31f4779`

证据日志：

- `dist/qa-startup/upgrade-profile/old/seed-424c9e3cd06b95aec13cf357b31f4779.log`
- `dist/qa-startup/upgrade-profile/old/verify-424c9e3cd06b95aec13cf357b31f4779.log`
- `dist/qa-startup/upgrade-profile/current/verify-424c9e3cd06b95aec13cf357b31f4779.log`
- fixture 内 `verify-1.1.9-result.json`
- fixture 内 `verify-1.2.0-result.json`

两版结果均为退出码 0。

## 实际读取结果

两版 reader 均完成以下读取断言：

- `.shadow-account.ini` 的账号存在性与 PBKDF2 密码认证；错误密码拒绝。
- `session.ini` 中的 `autoLoginEmail` 和 `autoLoginEnabled=false`。
- `beebeep.dat` 中 3 条历史记录，逐条验证正文、发送者、UTC 时间、文件消息类型和本地附件 URL。
- 附件 `received/原稿 & layout.psd` 的路径与二进制内容。
- `beebeep.off` 中 1 条有效待发消息，验证对端、chat id、消息 id、正文、重要标记和时间戳。
- `beebeep.ini` 中的持久设置：
  - `UseKeyReturnToSendMessage=false`
  - `BeepOnNewMessageArrived=false`
  - `ShowChatMessageOnTray=false`
  - `DownloadDirectory=<fixture>/accounts/<email>/received`
  - `ChatAutoSave=true`
  - `ChatSaveUnsentMessages=true`
- `User/List` 中的联系人实际解码为指定账号、域名和 hash。
- `Group/List` 中的群组实际解码为指定名称、private id、本人与对端成员。
- `conversation-preferences.ini` 中的 pinned/muted 值。
- `pending-transfers.json` 中的任务 id、batch、recipient、chat、source、group、state、文件元数据和源文件未变。
- `message-delivery.json` 中已投递/未投递状态；在 scratch 副本中完成最后一个 recipient，避免修改原始 fixture。
- HKCU 启动测试注册表始终由 `RegOverridePredefKey` 重定向；显式 `StartupEnabled=0` 在旧版 reader 和 1.2.0 reader 中均保持关闭。

每个 reader 结束前都重新计算账号根目录下全部文件 SHA-256。两版的结果完全一致，包含账号元数据、设置、历史、未发消息、队列、投递状态和附件。

## 固定策略

1.1.9 与 1.2.0 的 `Settings.cpp` load 逻辑在同一位置均为：

```cpp
m_confirmOnDownloadFile = false;
```

这个设置不是本次升级回归。seed 不再人为写入 `ConfirmOnDownloadFile=true`，verify 不把它当作持久设置验证项；报告只记录两版均观察到 `confirmOnDownloadFile=false`。

因此本测试支持的设置范围是实际可持久读取并跨两版比较的设置，不能宣称覆盖全部设置行为，也不证明 `ConfirmOnDownloadFile` 的用户选择迁移。

## 隔离范围

- 不启动 `Core::start()`。
- 不登录真实账号。
- 不建立网络连接。
- 账号、历史、附件和队列全部在随机临时目录。
- 两个进程分别使用随机 HKCU 子树的 `RegOverridePredefKey`。
- 生产用户 Run、Startup、真实聊天目录不参与测试。

这证明的是旧版实际 writer → 旧版实际 reader → 新版实际 reader 的本地迁移读取链路。完整旧客户端 UI 登录和真实网络行为不属于此 harness。

## 复现命令

在测试源码根目录、`cmd.exe`、`login:false` 环境中，先按仓库已有生产对象配置分别编译：

```bat
qmake tests\upgrade_profile_test.pro ^
  UPGRADE_SOURCE_ROOT=E:/ShadowChat/release-work/shadowchat-1.1.9 ^
  UPGRADE_PRODUCTION_ROOT=E:/ShadowChat/release-work/shadowchat-1.1.9/dist/local/production
nmake /nologo

qmake tests\upgrade_profile_test.pro ^
  UPGRADE_SOURCE_ROOT=E:/ShadowChat/release-work/shadowchat-1.2.0-update ^
  UPGRADE_PRODUCTION_ROOT=E:/ShadowChat/release-work/shadowchat-1.2.0-update/dist/qa-startup/production
nmake /nologo
```

运行顺序必须复用同一个随机 fixture：

```bat
old\upgrade_profile_test.exe seed <fixture> 1.1.9
old\upgrade_profile_test.exe verify <fixture> 1.1.9
current\upgrade_profile_test.exe verify <fixture> 1.2.0
```

`expectedObjectVersion` 参数只验证当前执行文件身份；fixture writer 版本从 `fixture.json` 读取，避免把 reader 版本硬编码成 1.2.0。

## 当前冻结状态

两文件已编译并完成双 reader 通过验证。当前源码指纹：

- `tests/upgrade_profile_test.cpp`: `a10514dabe49099bd758d2a0bd833c242231ae7d05fada0061ba80c6216dd28a`
- `tests/upgrade_profile_test.pro`: `02837bed2bde7169f213b390c940582585ac0ed360cc865d96b7eef818c04a5f`

主 agent 可将这两个文件加入 fresh build 的 snapshot 输入。
