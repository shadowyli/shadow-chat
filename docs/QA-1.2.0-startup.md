# 1.2.0 启动偏好与安装数据保留 QA

日期：2026-11-03。源码：`E:/ShadowChat/release-work/shadowchat-1.2.0-update`。
本轮不提交 commit，不修改 Updater、ShadowUpdateController 或 GuiMain。
最终发布安装包由主 agent 统一 fresh build；本轮产物均为 QA 证据。

## 交付与结论

- `src/core/Settings.cpp/.h`：保留已有默认开启、持久 opt-out、失败回滚与隔离保护；修复带引号启动命令丢弃尾部参数的问题。引号后只允许空白；多余参数和未闭合引号均不能被识别为正常启动项。
- `packaging/shadowchat-installer.nsi`：同一个编译产物接受显式 `/SHADOWCHAT-QA=<32位小写hex>`。没有 QA 专用编译条件，安装、覆盖、卸载的 section 共用。
- `packaging/verify_installation.py`：使用随机 QA namespace；删除旧版“修改生产注册表后再恢复”的执行方式。开始执行前验证安装包 SHA-256 与 PE 资源中的 `ShadowChatIsolation=1`，拒绝不支持隔离的旧包。
- `tests/startup_preferences_test.cpp`、`tests/run_startup_qa.py`：Qt 5.15.2、MSVC x86、Windows QPA 下执行真实 Settings 方法和真实设置弹窗。
- `tests/startup_installer_test.py`：生产 NSIS 源码搭配不可执行的小 payload 编译并实跑。
- `tests/startup_verifier_test.py`：使用实际本轮 EXE、fresh Qt/CRT、合成恢复资料自测完整验证器，不是发布包构建流程。

**组件 QA 通过，发布验收尚未完成。** 主 agent 仍需处理下述 GuiMain 集成缺陷，并对最终发布字节执行新验证器。

## 隔离机制

| 对象 | 正常安装 | 显式 QA 安装 |
| --- | --- | --- |
| 安装目录 | 正常 `$LOCALAPPDATA/Programs/ShadowChat` 或 `/D` | 强制 `$TEMP/ShadowChat-QA-<id>/program`，`/D` 不能改变 |
| 应用注册 | `HKCU/Software/shadowyli/ShadowChat` | `HKCU/Software/shadowyli/ShadowChat-QA/<id>/Preferences` |
| 开机启动 | 真实 `HKCU/.../CurrentVersion/Run/ShadowChat` | 同 namespace 的 `Startup/ShadowChat`；不是系统 Run 路径 |
| 卸载注册 | 真实 `HKCU/.../Uninstall/ShadowChat` | 同 namespace 的 `Uninstall` |
| 快捷方式 | 真实桌面和开始菜单 | fixture 下 `shell/Desktop`、`shell/Programs/影聊` |
| 应用快捷方式参数 | 空 | 强制合成 `--profile-root` |
| 安装完成页启动 | 正常执行 | 不启动应用 |
| 卸载隔离身份 | 无 | 从安装目录 `shadowchat-qa.ini` 读取并校验 namespace 与目录一致 |

namespace 只接受 32 个小写十六进制字符。无效值返回 3。
QA 安装目录缺失标记、卸载标记损坏或 namespace 与路径不匹配时拒绝执行，不能回退到生产注册。
正常自复制卸载和带 `_?=` 的同步卸载都已经测试。

生产状态只读快照包括：32/64 位视图下的完整用户 Run、ShadowChat 注册与卸载注册；用户和公共 Startup；影聊桌面与开始菜单快捷方式 SHA-256。
测试不做生产状态“恢复写入”。前后不一致直接失败并保留证据。

原生 Qt 测试在创建 QApplication 之前，对该进程执行 `RegOverridePredefKey`，指向随机 `Software/ShadowChatStartupTest-<uuid>` 子树。
重启子进程分别重新建立重定向，不依赖父进程重定向继承。所有账号文件均在临时目录，测试没有调用 `Core::start()`、登录真实账号或建立聊天连接。

## 已执行证据

根目录：`dist/qa-startup`。

1. `installer-contract/result.json`：14 个正常/故障场景与 6 个非法 namespace 通过。覆盖首次默认开启、显式关闭的两次覆盖升级、关闭偏好覆盖陈旧启动项、旧安装 opt-out、开启偏好修复缺失项、锁定 EXE 返回 2、卸载/重装、缺失/错误标记拒绝执行、`/D` 不能越出 fixture、正常自复制卸载。每一阶段检查生产状态和合成数据。
2. `preferences-result.json`：100% / 150% / 200% 和实际含空格 EXE 路径通过。包括大小写、分隔符、外部空白、非法额外参数、未闭合引号、独立进程重启、账号/预览动态属性及命令行保护、无关 Run 值保留。
3. 权限测试在临时注册表子树设置拒绝访问 ACL。Run 写入失败、偏好写入失败都返回 false；偏好写入失败时原有效启动状态恢复。未对生产 key 改权限。
4. `dpi-1/`、`dpi-1.5/`、`dpi-2/`：各有日志和设置截图。通过点击实际启动 checkbox、取消/保存按钮完成断言；取消前后账号目录文件哈希一致，真实 Run 和偏好不变。
5. `verifier-contract/verification-final/result.json`：完整新验证器在本地 QA 安装包上自测。包内为实际本轮 EXE；旧 1.1.9 payload 按历史 receipt 校验后暂存，再由当前安装器覆盖。历史安装器只校验文件哈希，绝不执行。

NSIS 小夹具本轮安装器 SHA-256：
`defb39a621e7143a53f9373f360e601413db5db6864cd23ee85a3a53e43bea9e`

对应 NSIS 源码 SHA-256：
`b8bf52a42b37f932c83996e6152feb6ffbf139143575f936075f483a2dfa09f4`

最终参考：各目录的 `commands.json`、`host-before.json`、`host-after.json`；
原生测试的主目录对应 `host-startup-before.json`、`host-startup-after.json`。
日志路径记录的是本轮机器路径，不应被当成发布安装目录。

## 主 agent 待修的集成问题

`GuiMain::showShadowSettings()` 保存时只判断 checkbox 是否不同于 `startup_preference`。
可复现：

1. 持久 `StartupEnabled=1`。
2. Run 为 `"当前EXE" --invalid-flag`；修复后的 `hasStartOnSystemBoot()` 正确返回 false，UI 不勾选。
3. 用户勾选开启并点击保存。
4. checkbox 为 true，等于持久偏好 true，跳过 `addStartOnSystemBoot()`；错误命令仍存在。

建议保存条件同时比较 `startup_enabled` 和 `startup_preference`，使用户保存的 checkbox 状态能修复不一致。
不在本 agent 的文件边界内改 GuiMain。

独立真实复现日志：`dist/qa-startup/integration-invalid-command/test.log`。
`preferences-result.json` 的 `invalidCommandRepairIntegrationProbe.passed` 当前为 false。
这是明确的集成未通过项，不能被同一报告的组件 `passed=true` 掩盖。

## 数据保留审查

旧验证器的问题：

- `/D` 只隔离安装目录，仍临时删写真实 Run、应用注册和快捷方式，然后恢复；本轮没有执行旧版脚本。
- 原夹具缺少 `beebeep.ini`、`session.ini`、`message-delivery.json`。
- 原 `pending-transfers.json` 是对象，实际 `ShadowTransferQueue::open()` 要求数组。
- 合成账号元数据、历史是哨兵字节，不构成“账号可登录、历史可解析”的证明。

新夹具对下列 12 个文件逐阶段比较 SHA-256：

| 类别 | 文件 |
| --- | --- |
| 账号 | `.shadow-account.ini` |
| 历史与未发消息 | `beebeep.dat`、`beebeep.off` |
| 附件 | `received/asset.psd` |
| 文件待发队列 | 合法数组形式的 `pending-transfers.json`，含源文件大小与修改时间 |
| 消息投递状态 | `message-delivery.json` |
| 应用与登录会话设置 | `beebeep.ini`、账号根目录 `session.ini` |
| 其他用户设置 | `conversation-preferences.ini`、`peer-avatars.ini`、`.shadow-paths.ini`、`beehosts.ini` |

覆盖新装、保留关闭的两次覆盖、保留开启、锁定失败、卸载、重装。
证明范围是“安装器没有改变 fixture 外部数据文件”，不证明应用 migration/读取行为。
最终应用读取旧账号、历史、附件与队列，以及真实旧安装器到新版的完整流程，由主 agent 在可丢弃 Windows 用户/VM 用合成账号另行验收。

## 复现命令

工作目录为本源码根目录；使用 `cmd.exe`，`login:false`。

```bat
C:\Users\N32962\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe -B tests\startup_installer_test.py
C:\Users\N32962\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe -B tests\run_startup_qa.py --probe-invalid-command-repair
```

第二条会独立构建 `dist/qa-startup/production`，不写主 agent 的 `dist/local/production`。
集成 probe 失败会单独记录；必须检查其结果，不能只看组件测试进程退出码。

主 agent fresh build 最终安装包并得到最终 `release-build.json` 后，执行：

```bat
C:\Users\N32962\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe -B packaging\verify_installation.py ^
  --receipt dist\local\release-build.json ^
  --previous-installer E:\ShadowChat\release-work\shadowchat-1.1.9\dist\local\影聊-1.1.9-test-x86-安装包.exe ^
  --previous-receipt E:\ShadowChat\release-work\shadowchat-1.1.9\dist\local\release-build.json ^
  --previous-payload E:\ShadowChat\release-work\shadowchat-1.1.9\dist\local\影聊-1.1.9-test-x86 ^
  --recovery-dir "<主agent已验证的私有恢复资料目录>" ^
  --output dist\qa-startup\final-package
```

`--receipt` 必须对应最终发布文件；脚本检查安装器 SHA-256，不会重打包或修改它。
`--previous-payload` 是按历史 receipt 全文件验哈希后的旧版目录，不会执行旧安装器。
`--recovery-dir` 仅本地读取已有恢复校验资料，内容不输出。
最终交付应保留 `final-package/result.json`、命令、前后生产快照、逐阶段数据哈希与最终包 receipt。

## 执行记录说明

首次完整验证器自测曾间接引用历史 `windows_shortcuts.py`，其内部使用 PowerShell 只读解析合成 `.lnk`。
发现后已移除这一依赖，改为标准 Python `ctypes` 直接调用只读 ShellLink COM；随后重新验证通过。
当前交付的验证器和测试链不调用 PowerShell，也不调用快捷方式 `Save()`。
本轮没有修改真实启动设置或真实聊天。
