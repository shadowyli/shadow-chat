# 影聊 1.1.1 Windows 测试版构建与安装验证

日期：2026-10-29，Asia/Shanghai。工作目录：`E:\ShadowChat\beebeep-code-r1557`。
最终校验时间：2026-10-29 12:41:40（UTC+08:00）。
结论：最终生产构建、21/21 组回归、品牌、安装覆盖/卸载以及最终 payload/
实际安装目录的 HTTPS 正反例验证全部通过，安装包可交由主代理归档发布。
这不代表真实办公环境及公司版本服务已经完成现场验收。
本报告由构建子代理维护；commit、标签 `v1.1.1-test`、GitHub prerelease、PR、
FlowX 暂存与远程下载核验由主代理关联，本文不把未提交的共享工作区冒充已归档提交。

## 执行命令

从仓库根目录通过 `cmd.exe` 执行，避开本机 PowerShell CET 故障：

```bat
scripts\release-windows.cmd rebuild > dist\build-1.1.1-mocfix.log 2>&1
scripts\build-tls-runtime.cmd > dist\tls-runtime-build.log 2>&1
scripts\build-tls-probe.cmd > dist\tls-probe-build-final.log 2>&1
scripts\release-windows.cmd > dist\build-1.1.1-final.log 2>&1
scripts\release-windows.cmd test > dist\tests-1.1.1-final.log 2>&1
python -X utf8 packaging\build_installer.py > dist\package-1.1.1-final.log 2>&1
python -X utf8 packaging\verify_branding.py > dist\branding-1.1.1-final.log 2>&1
python -X utf8 packaging\verify_installation.py > dist\install-1.1.1-final.log 2>&1
python -X utf8 packaging\verify_tls.py > dist\tls-payload-verification.log 2>&1
python -X utf8 packaging\verify_tls.py E:\ShadowChat\beebeep-code-r1557\dist\installation-test-5e15c2bd\program
```

`release-windows.cmd` 设置 x86 MSVC 环境与 Qt PATH；可用
`SHADOWCHAT_VCVARS`、`SHADOWCHAT_QT_DIR` 指定其他相同目标架构的工具链。
`build-windows.cmd rebuild` 先检查并保留 `src` 根目录旧 moc/qrc/ui、对象、
资源及 qmake 缓存到 `dist/legacy-generated-before-*`，再完整保留旧
`src/release` 到 `dist/production-objects-before-*`，
执行 `qmake src.pro -o Makefile "CONFIG+=no_batch"` 和 `nmake /nologo /A`。
不删除旧对象或用户数据，避免测试工程通配生产对象时混入孤立旧对象。
生成物隔离脚本拒绝移动 Git 已跟踪文件和指向其他位置的链接。
最终 Makefile 使用显式源路径，例如
`-Forelease\moc_GuiChatList.obj release\moc_GuiChatList.cpp`，
避免 nmake 批量推断误选根目录同名旧代码。

初次仅隔离 release 对象仍未排除根目录生成代码遮挡：
两项界面回归报缺少新 slots，根目录旧 moc 日期为 2026-08-14，
release 新 moc 则包含这些 slots。该中间包
`968df5eb974df6f53c6f9894a9822991503ea10a8322c0c401fc1c8ce7ab5a2b`
已否决，不得发布；其旧回执及包保留在 `dist/rejected-build-968df5eb/`。
最终以根目录 319 个旧生成文件及 release 输出均隔离后的重建为准。

强更代码冻结后重新采集输入，再执行最终生产增量构建；包含
`src/core/CoreVoiceChat.cpp`，该文件 SHA-256 为
`ded38ee4d15123a56a77fc62196d57adbe13b21fc14ae9268c16dfbf7936e4c7`。
最终构建与全部回归命令退出码均为 0，最终打包、品牌、安装和两次 TLS 验证也均为 0。

## 环境与来源

- 主机：Windows 10，`10.0.19044`；应用目标为 x86。
- Visual Studio 2022 Community 17.14.39；MSVC 工具目录 `14.44.35207`；
  Windows SDK `10.0.22621.0`。
- 编译器：`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\HostX86\x86\cl.exe`。
  SHA-256：`82960727fe27e0c0b7790673aa2c075eaccd1e9204198bfc14f869e6ff7bb0dd`。
- Qt 5.15.2 / qmake 3.1，来源：`E:\ShadowChat\Qt\5.15.2\msvc2019`。
- CRT 来源：Visual Studio 官方本机 Redist 目录的
  `14.44.35112\x86\Microsoft.VC143.CRT`，逐 DLL 摘要写入构建元数据。
- NSIS 3.12，来源：本机 `.tools\NSIS\makensis.exe`。
- Python 3.10.9。
- qmake SHA-256：`dd94dcaf07c40e1a2f1d958da17839d99b25252c2803bca9ef4d65281997e18b`。
- windeployqt SHA-256：`b01c8e7afbc192ff2a7d63cac481a595f627e5d2ba1e2d70b494b331e198ff29`。
- NSIS 编译器 SHA-256：`b043e554afefbfc56315669d0b4779793aeae67f0f2a7a790e2ea91f05298eff`。

工具安装包及本机 SDK 不作为源码提交内容。Qt DLL/插件和 CRT 的逐文件
SHA-256 均在 `dist/release-build.json` 的 `payloadInventory`、`appLocalCrt`、
`tools` 中留档。

## 无旧 dist 依赖的打包步骤

`packaging/build_installer.py` 创建新的 1.1.1 payload，将当前
`test/ShadowChat.exe` 交给已安装 SDK 的：

```bat
windeployqt --release --no-compiler-runtime --no-translations --dir <新payload> <新payload>\ShadowChat.exe
```

然后复制指定公共说明、许可、中文翻译、提示音和 x86 CRT，再调用 NSIS。
不读取 `dist/影聊-1.0.0-test-x86`；已有同版本 payload 会保留到
`*-previous-*`，不会合并进新 payload。无需把旧 dist 纳入源码。
PowerShell 打包入口统一委托该 Python 实现，不能绕过 payload 检查。
Qt 根目录、NSIS、CRT 和 TLS 运行库可分别用 `--qt-dir`、`--makensis`、
`--crt-dir`、`--tls-dir` 显式指定。

## HTTPS 兼容运行库

原 SDK/旧 payload 未提供兼容 TLS 运行库；Qt 探针初始结果为
`buildVersion=OpenSSL 1.1.1g`、`supportsSsl=false`。
不复用旧 `ssleay32.dll` / `libeay32.dll`，不升级本轮 Qt。
使用官方 OpenSSL 1.1.1w 源码在本机 MSVC x86 环境编译兼容运行库：

- 源码：`https://github.com/openssl/openssl/releases/download/OpenSSL_1_1_1w/openssl-1.1.1w.tar.gz`
- 源码 SHA-256：`cf3098950cb4d853ad95c0841f1f9c6d3dc102dccfcacd521d93925208b76ac8`
- 配置：`perl Configure VC-WIN32 shared no-asm no-tests`
- 构建：`nmake /nologo build_libs`
- `libssl-1_1.dll`：`ecce7cd173a7ac157c39255913dfb67a84e22e03cad39b4144ec7a492a372ab6`
- `libcrypto-1_1.dll`：`db4898c55f3a6cff41312bf15965ec1fd42b7aef04caa329475743110a153f9c`
- 许可证：源码原始 `LICENSE`，随包名为 `LICENSE.OpenSSL-1.1.1w.txt`，
  SHA-256：`c32913b33252e71190af2066f08115c69bc9fddadf3bf29296e20c835389841c`。

构建辅助工具为 `dist/tls-tooling/native-perl` 内的 Strawberry Perl 便携版，
来源与归档摘要固定在 `packaging/prepare_tls_runtime.py` 和生成的
`TLS-DEPENDENCIES.json`；未安装系统 Perl、未安装系统 OpenSSL。
Perl、完整工具包和源码树不进入安装包。

`packaging/verify_tls.py` 复制目标目录内的实际 DLL，逐一记录 SHA-256，
清空开发工具 PATH 后执行 Qt 原生探针。可信测试 CA 只传给本次探针，
本地 HTTPS 成功返回 200；不传该 CA 的对照请求必须因证书错误失败。
不调用忽略证书错误接口，不修改系统信任库。
最终 payload 与实际安装目录均已重新执行并通过：
`supportsSsl=true`，编译版本为 1.1.1g，加载版本为 1.1.1w；
可信本地 HTTPS 返回 200、`networkError=0`、证书错误为空；
不提供测试 CA 的对照请求被拒绝，`networkError=6`，并记录不受信任证书错误。
两份回执分别为 `dist/tls-payload-verification.json` 和
`dist/tls-installed-verification.json`；`dist/tls-verification.json` 保留最后一次结果。
两次探针使用的 22 个根目录 DLL 的摘要均与最终 `payloadInventory` 一致。
合成服务器私钥只在 `dist/tls-fixture-*` 中用于本地验证，绝不进入 payload 或发布附件。

OpenSSL 1.1.1w 是匹配现有 Qt ABI 的兼容运行库。本轮验证不代表完成
Qt/TLS 长期升级或漏洞审计；后续版本需安排维护中的 Qt/TLS 升级。

## 恢复配置与资料边界

恢复配置是负责人提供的既有私密构建输入，默认从忽略的
`delivery-private/` 读取；可用 `--recovery-dir` 指定私密目录。
本次不执行 `prepare_recovery.py`，不生成新恢复密码。

只允许分发现有 `shadowchat-recovery.ini`：必须仅有 `[General]` 的
16 字节十六进制 salt 与 32 字节 PBKDF2-SHA256 哈希。
私有负责人文件仅在本机内存中读取，用于验证现有哈希对应关系和检查 payload
是否含该密码的 UTF-8/UTF-16LE 明文；不打印、不复制、不上传该私有文件。
种子/私有文件缺失或不匹配时直接失败。

payload 来自新 Qt 部署、当前程序和固定公共文件清单，不递归复制应用目录、
用户资料或私密目录；因此个人账号、历史、附件和凭据文件不进入安装包。
本地日志、旧对象归档、测试资料和 `delivery-private/` 不属于发布附件。

## 本轮验证

最终 21/21 个测试工程通过，失败项目为 0，完整日志为
`dist/tests-1.1.1-final.log`，汇总为 `dist/regression-results.json`。

| 范围 | 通过的测试工程 |
| --- | --- |
| 强更与更新 | `update_core_gate`、`update_gate`、`update_manifest`、`update_network` |
| 身份与迁移 | `local_accounts`、`workspace_archive` |
| 传输与送达 | `transfer_queue`、`transfer_failure`、`group_policy`、`message_delivery` |
| 历史 | `history_records`、`history_retention`、`history_ui` |
| 界面与偏好 | `avatar_refresh`、`profile_editor`、`dialog_consistency`、`conversation_preferences` |
| 表情 | `sticker_store`、`sticker_ui` |
| 真实服务回环 | `loopback_chat`、`group_loopback` |

强更测试覆盖真实 Core 启动/阻断/再启动、直接与 queued 收发入口、解析、
文件队列和磁盘持久化，以及版本入口与清单/HTTP 异常。启动与运行中的强更
行为由这些测试验证，不启动用户的真实账号作此检查。
群聊 seed、queue、resume、rejoin、roles 阶段全部通过，
离线素材元数据在发送端重启后保留、目录和空文件字节验证也通过。

回归驱动在受版本管理的 `scripts/test-windows.cmd`，使用独立
`dist/regression-bin` 和每项独立对象目录，不依赖被忽略的 `test/*.cmd`
历史驱动，也不优先执行可能过期的旧 exe。
驱动自动枚举 `update_*_test.pro`，明确包含 `update_gate` 和 `update_core_gate`。
本轮实际覆盖更新清单/HTTP 异常、工作区备份恢复、本地账号、传输队列/失败、
群规则/消息送达、历史分类/保留、头像、个人资料、对话框、会话偏好、表情库/
界面、历史界面以及基础双端私聊/群聊 loopback；不重跑 5GB 与 20 进程长测。

安装验证使用 `dist/installation-test-*` 的隔离程序目录与合成外部资料；
安装期间临时保存并最终恢复当前用户已有的 NSIS 32 位注册表项、桌面和开始菜单快捷方式。
检查 1.1.0 覆盖升级、1.1.1 再覆盖、逐 payload 文件与 CRT 哈希、注册版本、
恢复哈希、锁定程序时拒绝安装、卸载保留外部合成资料及卸载后重装。
上述项目全部通过；恢复私有输入经前后摘要对照未变，未生成新恢复密码。
品牌验证通过：PE 图标资源完整匹配 9 帧生成 ICO。
清空开发工具 PATH 后运行参数保护分支以检查程序导入依赖可解析；
该探针在 Qt 界面和账号登录之前退出，不算真实用户 GUI 启动通过。

## 产物

| 文件 | 用途 | 精确字节数 | SHA-256 |
| --- | --- | ---: | --- |
| `dist/影聊-1.1.1-test-x86-安装包.exe` | 最终 Windows x86 当前用户安装包 | 36,468,376 | `494354a4137edca66259163c0edff9010d8909fc8904e575fbb30fb9674dd577` |
| `test/ShadowChat.exe` | 冻结源码构建的生产程序，与 payload 和安装后的程序一致 | 24,671,232 | `34b4078938031f72a07aa8f02c37e81ac022b36b399c98121789e7a685819313` |

程序与安装包均核验为 PE32 / I386（`0x14c`）。
最终 payload 目录：`E:\ShadowChat\beebeep-code-r1557\dist\影聊-1.1.1-test-x86`。
保留的实际安装目录：
`E:\ShadowChat\beebeep-code-r1557\dist\installation-test-5e15c2bd\program`。

包体为 52 个明确记录摘要的 payload 文件：生产程序、31 个 Qt/图形 DLL
及插件、9 个 x86 CRT DLL、2 个兼容 OpenSSL DLL、6 个固定公共资源/
说明文件、OpenSSL 完整许可证与来源 JSON、现有团队加盐恢复哈希。
不含源码树、Perl、OpenSSL 构建树、测试 exe/证书/私钥、私密负责人文件、
账号、聊天资料或附件。卸载程序由 NSIS 安装时另行生成。

`dist/release-build.json`（12,149 字节）提供本机构建环境、工具来源与摘要、
逐 payload 文件清单和 TLS 来源，不含最终 sourceCommit；
主代理须另生成与已提交源码关联的 `build-info.json` Release 附件。
`dist/SHA256SUMS.txt`（102 字节，UTF-8）提供最终安装包的完整校验行。
可执行 `certutil -hashfile <安装包路径> SHA256` 并与该清单比对。

源码快照 `dist/build-source-inputs.json` 覆盖 2,466 个生产、测试、公共资源和
构建脚本输入，文件自身 SHA-256 为
`892ddfdf0eef8c6ee0be1c1defc5ed9d85e4eaefccae69855df43de033232f62`。
最终逐文件复核无修改、无新增输入；详见 `dist/source-input-verification.json`。

本构建子代理直接新增/编辑的源码交付文件如下；其他 C++ 生产源码及测试源码均未修改：

```text
packaging/build-installer.ps1
packaging/build_installer.py
packaging/verify_installation.py
packaging/使用说明.txt
packaging/prepare_tls_runtime.py
packaging/tls_probe.cpp
packaging/tls_probe.pro
packaging/verify_tls.py
scripts/build-windows.cmd
scripts/test-windows.cmd
scripts/release-windows.cmd
scripts/isolate-build-output.py
scripts/build-tls-runtime.cmd
scripts/build-tls-probe.cmd
docs/RELEASE-1.1.1-BUILD.md
```

`dist/` 另有日志、回执、SHA256 清单、依赖工具、隔离对象和合成测试资料，
均为本地构建/验证输出，不应整目录提交或发布。

## 限制与交接

- 本机安装与同机 loopback 不等于真实 20 人办公网络、企业受限账号、
  Windows 10/11 多机、干净虚拟机或现场验收。
- 未启动真实用户账号，不以本轮安装探针宣称真实账号完整 GUI 启动、历史、
  大文件传输、全链路 HTTPS 或联网分发通过。
- 本地可信 HTTPS fixture 通过不等于公司生产端点的路由、证书与部署验收。
  强更清单不可访问、未发布、版本过旧或校验失败时会阻断入口；
  试用前必须先由主代理部署并核对公司清单和下载服务。
- 最终回归存在非失败 Qt SVG 警告：缺少
  `:/images/shadowchat/icons/gear-six.svg`。该资源问题不在本子代理源码修改范围，
  未改动生产源码规避；不将自动化通过表述为全部视觉资源已经验收。
- 未执行 Git add/commit/push/tag、创建 PR/Release 或上传任何私密资料。
- `dist/build-source-inputs.json` 记录生产与测试输入指纹，主代理应与最终 Git
  归档逐项对照，并单独生成含 sourceCommit/sourceTag 的 Release 构建信息资产。
- 最终构建、测试、打包、安装与 TLS 命令均已结束；发布脚本及冻结输入保持稳定，
  可由主代理提交归档。旧包与旧回执不得替代本文列出的最终摘要。
