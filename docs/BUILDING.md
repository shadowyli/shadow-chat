# Windows 构建指南

[返回首页](../README.md) · [贡献指南](../CONTRIBUTING.md)

影聊的维护构建路径为 Windows x86。下面介绍编译、测试与安装包所需的工具和输入。
复现某次发布时，使用对应标签的源码，并对照该 Release 的 `build-info.json`。

## 前置环境

| 工具 | 要求 |
| --- | --- |
| Visual Studio | 2022，安装 C++ 桌面开发、Windows SDK 与 x86 MSVC 工具链 |
| Qt | 5.15.2 MSVC2019 **x86** SDK，包含 qmake、Widgets、Network、Multimedia 等模块 |
| Python | 3.9 或更高版本，可在命令行通过 `python` 调用 |
| Git | 用于获取源码和记录源码身份 |
| NSIS | 3.12，仅制作安装包时需要 |

常见的 `msvc2019_64` SDK 是 x64，不能代替这里的 x86 SDK。
本文 `C:\Qt\5.15.2\msvc2019` 是示例位置，请使用实际路径。

## 获取源码并构建

在 Windows `cmd.exe` 中运行：

```bat
git clone https://github.com/shadowyli/shadow-chat.git
cd shadow-chat
set "SHADOWCHAT_QT_DIR=C:\Qt\5.15.2\msvc2019"
scripts\release-windows.cmd build --environment local
```

入口会加载 VS 2022 Community 默认位置的 `vcvars32.bat` 并加入 Qt PATH。
若使用其他版本或安装位置，先设置实际文件路径，例如：

```bat
set "SHADOWCHAT_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat"
```

`SHADOWCHAT_QT_DIR` 应指向 SDK 根目录，包含 `bin\qmake.exe`。
可先运行 `scripts\release-windows.cmd configure --environment local` 检查配置；
该命令只生成并检查 qmake 配置。

成功构建的程序位于 `dist\local\production\bin\ShadowChat.exe`，
相应参数与源码身份在 `dist\local\build-configuration.json` 等回执中。
直接运行程序需要匹配的 Qt / C++ 运行库；TLS 更新请求另需 x86 TLS 运行库。
分发给其他电脑时，使用下述安装包流程收集运行组件。

## 测试的额外前置条件

现有测试入口是：

```bat
scripts\release-windows.cmd test --environment local
```

运行前需要同环境构建、匹配的源码快照和 TLS 运行库记录。
默认 TLS 目录是 `dist\tls-runtime-x86`，须包含 DLL 及 `TLS-DEPENDENCIES.json`；
可用 `SHADOWCHAT_TLS_DIR` 指向有效目录。

仓库 `packaging\prepare_tls_runtime.py` 在 x86 VS 开发者命令行下构建当前指定的
OpenSSL 1.1.1w 兼容运行库，下载固定摘要的上游源码及 Perl：

```bat
python packaging\prepare_tls_runtime.py
```

该脚本需要联网与 `nmake`，输出 DLL、许可证及依赖摘要清单。

完整测试中的 `update_network` 还需要合成 TLS fixture（`server.pem`、
`synthetic-server-key.pem`），可以通过测试入口的 `--tls-fixture` 指定。
首次准备 fixture 的步骤见本文“完整 TLS 与回归验证”。

已有前置资料后，可按脚本支持的 `--tests` 选择已有测试项目，例如：

```bat
scripts\release-windows.cmd test --environment local --tests local_accounts
```

这仍需要 TLS 目录及匹配构建记录。操作场景见 [本地测试指南](LOCAL-TEST-GUIDE.md)。

## 制作安装包

在相同环境完成构建、准备 TLS 运行库与恢复输入后，配置 NSIS 的实际路径：

```bat
set "SHADOWCHAT_MAKENSIS_PATH=C:\Program Files (x86)\NSIS\makensis.exe"
python packaging\build_installer.py --environment local
```

安装包完成后，可从同一份已验证载荷生成便携 ZIP：

```bat
python packaging\build_portable.py --environment local
```

脚本会逐文件核对 `release-build.json` 中的安装载荷清单，并写入
`dist\local\portable-build.json`。载荷不一致或同名 ZIP 已存在时会停止，避免覆盖候选包。

打包还需要已核验的 x86 TLS 运行库、Visual Studio x86 CRT 和本机私密恢复输入。
默认 `delivery-private` 目录须有匹配的恢复哈希配置及负责人文件，
也可用 `SHADOWCHAT_RECOVERY_DIR` 指定私密目录；不要提交或公开这些文件。
首次自建分发可在本机运行已有的 `python packaging\prepare_recovery.py` 创建自己的输入，
保管负责人密钥，不要索取或复用维护者的私密恢复资料。

输出路径、摘要、工具链与实际载荷以 `dist\local\release-build.json` 为准。
发布与源码归档要求见 [版本发布规则](VERSION-RELEASE-POLICY.md)。
测试包与正式包按构建环境区分；打包脚本会拒绝覆盖既有安装包。

## 完整 TLS 与回归验证

在仓库根目录、已配置的 x86 VS 开发者命令行中，先完成上述安装包流程。
再安装 Python 验证依赖、构建探针并核验安装包实际携带的 TLS 运行库：

```bat
python -m pip install cryptography
cmd /c scripts\build-tls-probe.cmd
python packaging\verify_tls.py --receipt dist\local\release-build.json
```

`verify_tls.py` 检查受信任 HTTPS 请求和不受信任证书的拒绝行为，
在 `dist\local` 下生成 `tls-fixture-*` 目录，并将路径写入
`tls-verification.json` 的 `fixtureDirectory` 字段。
把该字段的完整路径作为下面的占位值，执行全部现有回归项目：

```bat
scripts\release-windows.cmd test --environment local --tls-fixture "<fixtureDirectory 的完整路径>"
```

合成证书和私钥仅用于本机验证，不加入系统信任或发布包。
TLS 校验、部分回归、全部回归和真实多机网络验证应分别记录结果。
