# ShadowChat 1.1.2 local/test 构建与安装验证

日期：2026-10-29（Asia/Shanghai）。工作目录：`E:\ShadowChat\beebeep-code-r1557`。
分支：`codex/update-advisory-1-1-2`。构建基准提交：
`4da4b5b7c4feb5c73f03051455eef37905651f78`。
最终 commit、PR #2、标签、源码归档和 Release 由主代理在归档后关联，
不能把上述基准提交当成含全部工作区变更的源码版本。

**结论：local/test 包已构建并完成浏览器下载副本的安装复验，可以交主代理归档。**
真实已安装 EXE 已观察到空资料登录页和合成账号主窗口，不存在启动门禁。
独立最终对象回归共 22 个测试工程通过；截图及同安装 EXE 双端手动发消息仍有
Windows UI 权限限制，不能宣称真实办公网络或 dev/main 已验收。

## 最终产物与包体

| 产物 | 用途 | 精确字节数 | SHA-256 |
| --- | --- | ---: | --- |
| `dist/local/影聊-1.1.2-test-x86-安装包.exe` | 最终 Windows x86 当前用户安装包，local/test | 36,472,694 | `7f59a75bfae8b5d6e04d5af22307d07941fea7e2bb65dd1659b972c2f8266279` |
| `dist/local/production/bin/ShadowChat.exe` | 实际生产程序，包内及安装后相同 | 24,678,912 | `54ed58bba7ba6f9df2df064d4ca8e2ec4664e61e1b92c6b0ccbad1c26999094a` |
| `C:/Users/N32962/Downloads/ShadowChat-1.1.2-local-setup.exe` | 主代理通过浏览器真实下载、本子代理重新安装的副本 | 36,472,694 | `7f59a75bfae8b5d6e04d5af22307d07941fea7e2bb65dd1659b972c2f8266279` |

最终 payload 为 `dist/local/影聊-1.1.2-test-x86`，共 53 个文件：
生产 EXE、42 个 Qt/图形/插件/CRT/TLS DLL、中文翻译与提示音、固定公共说明、
GPL/第三方许可、OpenSSL 完整许可证和依赖来源 JSON、版本化 ICO、
负责人既有加盐恢复哈希。NSIS 安装时另生成卸载程序。
逐文件大小及 SHA-256 见 `dist/local/release-build.json` 的 `payloadInventory`。

`dist/local/SHA256SUMS.txt` 是安装包校验清单；
`release-build.json` 是本地构建回执，包含有效环境、通道、源、工具、运行库、
程序摘要与冻结源码快照；主代理另生成含最终 sourceCommit/sourceTag 的
`build-info.json` Release 附件。源码包应从最终 Git 标签归档，不打包本地 dist。
可用 `certutil -hashfile <安装包路径> SHA256` 与校验清单比对。

不含个人账号、聊天、附件、队列、负责人明文文件、测试证书/私钥、Perl、
源码树或编译目录。未重新生成或更换团队恢复密码。
已发布 1.1.1 包、标签及历史附件未覆盖。
中间候选 `6fe799fa…` 已作废并保留在 `dist/local/rejected-6fe799fa/`，不得发布。

## 有效环境与构建来源

版本唯一来源为 `src/core/Version.h`。Python builder 将版本、环境、通道、
下载页传给 NSIS；图标名由版本派生，不分散硬编码版本。

| 环境 | 通道 | 编译固定版本源 | 本轮结果 |
| --- | --- | --- | --- |
| local | test | `http://127.0.0.1:5173/updates/shadowchat.json` | 最终包已构建和安装验证 |
| dev | test | `https://dev.flowx.gz/updates/shadowchat.json` | 仅配置与客户端矩阵验证，无 dev 包、未部署 |
| main | stable | `https://flowx.gz/updates/shadowchat.json` | 仅配置与客户端矩阵验证，无 stable 包、未部署 |

三环境参数记录在 `dist/<env>/configured-parameters.json`，都明确标记
`configuredOnly=true`。local 实际宏含 `SHADOWCHAT_BUILD_LOCAL` 和
`SHADOWCHAT_LOCAL_UPDATE_PORT=5173`；dev/main 分别含其独占环境宏。
不能仅给 local 包改标签当作 dev/main。

最终安装后 `--build-info` 和 `--version-json` 都实际返回：

```json
{"channel":"test","environment":"local","updateSource":"http://127.0.0.1:5173/updates/shadowchat.json","version":"1.1.2"}
```

两次入口结果留存 `dist/local/installed-entry-identities.json`。
`--profile-root` 只隔离用户资料和实例，不修改版本源、通道或登录权限。
本轮没有停止用户 5173 FlowX 服务、替换其清单或关闭证书验证。

环境：本次回执记录 Windows `10.0.19041`、Python 3.10.9、
Visual Studio 2022 Community 开发命令环境 17.14.39、x86 MSVC 工具
`14.44.35207`、Qt 5.15.2（MSVC SDK）、NSIS 3.12。
CRT 来自 VS Redist `14.44.35112/x86/Microsoft.VC143.CRT`。
编译器、qmake、nmake、部署工具及每个运行库摘要均在构建回执。

关键工具摘要：

- qmake：`dd94dcaf07c40e1a2f1d958da17839d99b25252c2803bca9ef4d65281997e18b`
- windeployqt：`b01c8e7afbc192ff2a7d63cac481a595f627e5d2ba1e2d70b494b331e198ff29`
- NSIS：`b043e554afefbfc56315669d0b4779793aeae67f0f2a7a790e2ea91f05298eff`

## 实际命令及干净输出

通过 `cmd.exe`，避开默认 PowerShell CET 问题；Windows COM 快捷方式读取
使用系统 WindowsPowerShell，未使用其控制用户窗口。

```bat
scripts\release-windows.cmd configure --environment local --local-update-port 5173
scripts\release-windows.cmd configure --environment dev
scripts\release-windows.cmd configure --environment main
scripts\release-windows.cmd rebuild --environment local --local-update-port 5173
scripts\release-windows.cmd build --environment local --local-update-port 5173
scripts\release-windows.cmd test --environment local --local-update-port 5173
scripts\release-windows.cmd test --environment local --local-update-port 5173 --tests update_build update_core_gate update_gate update_manifest update_network local_accounts
python -B -X utf8 packaging\build_installer.py --environment local
python -B -X utf8 packaging\verify_installation.py --receipt dist\local\release-build.json
python -B -X utf8 packaging\verify_installation.py C:\Users\N32962\Downloads\ShadowChat-1.1.2-local-setup.exe --receipt dist\local\release-build.json
python -B -X utf8 packaging\verify_tls.py --receipt dist\local\release-build.json --report dist\local\tls-payload-verification.json
python -B -X utf8 packaging\verify_tls.py E:\ShadowChat\beebeep-code-r1557\dist\local\installation-test-3bb08966\program --receipt dist\local\release-build.json --report dist\local\tls-installed-verification.json
```

旧 `src/release` 对象和根目录 moc/qrc/ui 生成物经检查后安全保留到
`dist/*-before-*`。生产输出独立在 `dist/local/production/`，
对象在 `objects/`，moc/ui/rcc 在 `generated/`。
qmake `-after DESTDIR=… OBJECTS_DIR=…` 防止项目内赋值覆盖最终输出目录；
`CONFIG+=no_batch` 防止 nmake 同名旧生成代码遮挡。
测试项目只在 dist 生成路径适配副本，链接最终生产对象，不改测试源码，
不从旧 `src/release` 误取对象；`/FC` 保证 `__FILE__` 入口检查的源码路径正确。
配置切换时隔离旧生产及测试对象，不串环境。

首次快照遇到 Git 已删除旧 Gate 文件停止，已改为记录 deleted；
首次独立编译成功后因 DESTDIR 被项目覆盖，身份探针找不到文件；
修复 `-after` 后只增量链接，未以复制 EXE 伪造构建位置。
后续 controller/Updater/profile-root 小改均只增量重编受影响对象。
最终生产构建、定向回归、打包、安装、TLS 命令均退出 0。

冻结编译快照：`dist/local/build-source-inputs.json`，2,478 个文件，
SHA-256 为 `5dbb5f0becfbf56a1764267241f0ac694681f04cf18cb90ee8c6214adb130383`。
最终逐项复核其中记录无变化。
之后仅新增验证辅助 `packaging/prepare_startup_fixture.py`，不参与生产程序或
payload，故未重编客户端；主代理归档和 build-info 应同时列明该辅助文件。

主要日志：`dist/local/build-1.1.2-final.log`、`build-1.1.2-link-final.log`、
`build-1.1.2-notes-final.log`、`build-1.1.2-profile-final.log`、
`package-1.1.2-final.log`、`installation-http-final.log`。

## 回归结果

最终对象验证的不同工程并集为 **22/22 通过**，详见
`dist/local/regression-results.json`。初轮三项配置/测试假设失败没有隐去：
`update_build` 缺三个必要实参；Core 入口的相对 `__FILE__` 提前返回 10；
网络注入测试误把合法 local 源当成应拒绝的公司 HTTPS 源。
修正驱动参数、绝对源码路径及由主代理修正的明确 HTTPS 注入用例后，
五更新工程通过；profile-root 变更后六项更新/账号/入口工程再次通过。

| 范围 | 测试工程 |
| --- | --- |
| 更新与入口 | update_build、update_core_gate、update_gate、update_manifest、update_network |
| 账号/迁移 | local_accounts、workspace_archive |
| 传输/送达 | transfer_queue、transfer_failure、group_policy、message_delivery |
| 历史 | history_records、history_retention、history_ui |
| 界面/偏好 | avatar_refresh、profile_editor、dialog_consistency、conversation_preferences |
| 表情 | sticker_store、sticker_ui |
| 双端 | loopback_chat、group_loopback |

最新 UI 更新测试：21 PASS、0 FAIL、1 个子进程辅助 SKIP。
覆盖最新/旧版/新版、通道、每日提醒持久化、跨天、回拨、当天重启、
手动关闭/Esc/下载后当天不再自动提醒、失败不消耗机会、重复请求与退出等。
网络覆盖 HTML、非法 JSON、HTTP 错误、未发布、超时、证书异常、重定向、
版本边界、数组版本说明、构造器源约束及异步完成。
`mandatory=true` 不再阻断使用。
Core 测试验证双向文字/文件、ACK、失败更新服务不停止通讯、历史备份与恢复。
群 loopback seed/queue/resume/rejoin/roles 及目录/空文件校验通过。
本轮没有为凑数量重跑 5GB、20 进程长测或无关全量测试。

## 安装、升级、图标与资料

浏览器下载副本安装回执：`dist/local/installation-verification.json`。
保留的最终实际安装目录：
`E:\ShadowChat\beebeep-code-r1557\dist\local\installation-test-3bb08966\program`。
该目录 payload 的全部 53 个文件与最终回执一致，RC 也完整匹配。

通过：记录中已发布 1.1.1 安装及 EXE 摘要核验 → 1.1.2 覆盖、
再次覆盖、运行文件锁定时返回 2、卸载、资料保留和重新安装。
外部合成资料包含历史、账号元数据、接收附件、头像、会话偏好和
`pending-transfers.json` 队列，前后逐字节摘要一致。
这些是合成安装保留 fixture，不将其误称真实历史/队列格式端到端验收；
真实队列逻辑由客户端/Core 测试覆盖。

安装期间暂存当前用户两个注册表项、桌面/开始菜单三个快捷方式，
结束后恢复原值与原字节并刷新 Shell。未重装用户真实默认目录，
未停止 Explorer，未修改真实 Documents 注册表或登录真实账号。

桌面、开始菜单和卸载入口实际 `.lnk` 的目标、IconLocation、
卸载注册 DisplayIcon 均检查，使用 `shadowchat-1.1.2.ico,0`。
ICO 为 57,958 字节，SHA-256：
`416b5c634305f485606d9541ae0f8a44cffa5b2b1b6c833898b513e35e7ebc6f`。
程序 PE 图标资源九帧匹配同一红色 ICO。仅验证 EXE 图标不再作为快捷方式验收替代。

## 同一已安装 EXE 的实际登录和主窗口

不是参数保护探针，也不是测试替身。使用最终安装目录
`installation-test-198b04b1/program/ShadowChat.exe`（与最终包 EXE 摘要相同）：

```bat
ShadowChat.exe --profile-root E:\ShadowChat\beebeep-code-r1557\dist\local\startup-fixture\empty\accounts
python -B packaging\prepare_startup_fixture.py --profile-root <绝对合成root> --name alpha --port 48641
ShadowChat.exe --profile-root <绝对合成root>
```

`--profile-root` 为生产正常资料配置：要求绝对非根目录；账号、提醒状态、
单实例标识和退出重启保留该 root，跳过写入系统开机项，不读取安装恢复配置。
不修改更新源，不绕过本地账号认证。
合成账号按真实 PBKDF2 格式 seed，随机密码不打印，session 正常自动登录。

实际观察回执：`dist/local/startup-fixture/startup-verification.json`。

| 进程 | 实际窗口/句柄 | 结果 |
| --- | --- | --- |
| PID 12468 | `影聊 · 登录`，窗口 6169746 | 空 root 真实登录页，无版本门禁，主代理也独立观察 |
| PID 78644 | `Release Alpha Fixture - 影聊`，窗口 3215858 | 真实主窗口，但初次网络隔离不完整，发现一个非合成联系人，未发送消息，立即只停本测试进程 |
| PID 59384 | `Release Alpha Fixture - 影聊`，窗口 2691630 | hosts-only 重启，观察空联系人状态 |
| PID 1880 / 67824 | Alpha/Beta 主窗口 468088 / 12331924 | 两个独立 root 的同安装 EXE 同时进入主界面，仅配置 loopback 同伴 |

首次测试网络设置遗漏如实保留，不作为隔离通过证据。
之后在**隔离安装目录**临时将 RC 配为 hosts-only、禁 multicast、
Hive/subnet，并只允许 `127.0.0.1:48641/48642`；不是修改用户安装配置，
也不是改变发布二进制。主窗口可访问性文本保存在
`main-window-text.txt`、`main-isolated-window-text.txt`。
空 root/seeded/loopback 进程日志均保留，但 Qt 日志仅输出有限提示；
以真实窗口标题和可访问性文本为实际 UI 证据，不虚构 `Enter main event loop` 日志。

截图 API 报 `FrameArrived timed out`，UI 输入报
`GetCursorPos access denied (0x80070005)`。因此没有假称拿到截图，
也没有声称通过同安装 EXE 的双端手动发送；双向数据证明来自最终对象 Core 测试。
所有本任务创建的合成进程已停止，不影响用户原 BeeBEEP/影聊实例。
启动测试结束已恢复原 RC、移除临时 hosts，再逐项核验两处安装目录的全部
53 个 payload 文件摘要一致。最终 RC SHA-256：
`25146d97692cf17e2f55813cddf9ea8ccf7fb9d72ba46196f1266602b44efc83`。

## TLS 与私密输入

最终 payload 和浏览器副本实际安装目录分别验证：
`dist/local/tls-payload-verification.json`、
`dist/local/tls-installed-verification.json`。
清空开发 PATH，探针加载实际 payload DLL；22 个根目录 DLL 摘要关联回执。
`supportsSsl=true`，Qt 编译 OpenSSL 1.1.1g，运行 OpenSSL 1.1.1w。
本地可信 HTTPS 200、证书错误为空；不信任测试 CA 的对照请求被拒绝。
没有调用忽略证书错误，没有修改系统信任库。

OpenSSL 1.1.1w 官方源码：
`https://github.com/openssl/openssl/releases/download/OpenSSL_1_1_1w/openssl-1.1.1w.tar.gz`；
归档 SHA-256：`cf3098950cb4d853ad95c0841f1f9c6d3dc102dccfcacd521d93925208b76ac8`。
复现入口 `scripts/build-tls-runtime.cmd` 使用 dist 内便携 Perl，
`Configure VC-WIN32 shared no-asm no-tests`、`nmake /nologo build_libs`。
完整许可证和源码/工具来源随包 `TLS-DEPENDENCIES.json` 与
`LICENSE.OpenSSL-1.1.1w.txt`。兼容 ABI 不代表完成长期安全维护，
后续应安排维护中的 Qt/TLS 升级。

## dev/main 材料与状态

独立材料：`dist/deployment-1.1.2/`，包含准备脚本、环境说明和
`published=false` 占位清单，不含伪造 dev/main 安装包。
不能复用 `delivery/flowx-shadowchat-update-service` 的 1.1.1 强更材料。

`packaging/prepare_deployment.py` 固定 dev 主机 `dev.flowx.gz`、
main 主机 `flowx.gz`，核对 receipt/清单环境、通道、版本、大小和摘要，
拒绝强更；不同摘要的已有 EXE 拒绝覆盖，相同则跳过。
只操作列出的安装包和清单，先包后清单，清单备份并原子替换，
不覆盖整站。main 执行要求显式 `-MainApproved`。
本轮未执行服务器脚本，dev 接口 HTML 响应仍不是有效版本服务；
无已授权可用凭据/通道，因此 dev 待部署/待验收，main 待 dev 验收和用户确认。

## 修改范围与交接

构建子代理修改限定 `packaging/**`、`scripts/**`、本文，
保留上轮未提交图标修复并纳入。核心新增工具为
`release_config.py`、`source_snapshot.py`、`prepare_regression_project.py`、
`prepare_startup_fixture.py`、`prepare_deployment.py`、`windows_build.py`；
其余安装、快捷方式、TLS 和构建 cmd 按对应范围更新。
生产 src/tests 由主代理/Curie 修改，构建子代理未越界编辑。

最终可交主代理提交和发布；Git add/commit/push/tag/Release/FlowX 操作
不由本子代理执行。尚未完成部分须保留：
dev/main 部署验收、真实办公网络、更多企业权限/Windows 版本、
同安装 EXE 手动双端消息与截图（Windows UI 权限限制）。
