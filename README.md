# 影聊（ShadowChat）

影聊是面向公司局域网办公的轻量聊天工具，基于 BeeBEEP r1557 改造。它保留局域网直连、文件传输和无需注册的使用方式，重点改善日常聊天的易用性、界面清晰度与数据边界。

## 下载

Windows 32 位安装包：

<https://github.com/shadowyli/shadow-chat/releases/latest>

安装包会以 GitHub Release 附件发布，安装后可从开始菜单或桌面启动“影聊”。卸载默认保留本地工作区数据，方便重新安装或迁移。

## 主要改进

- 深色导航栏、现代化标题栏和更清晰的“消息 / 联系人 / 群组”结构。
- 多张图片拖入后先进入预览，一次发送为一个图片合集，不再逐张刷屏。
- 超长消息默认折叠，点击后展开全文。
- 首次启动向导，设置昵称和头像；局域网内无需注册账号。
- 工作区导出 / 导入，用于换电脑迁移聊天偏好和本地设置。
- 保留局域网聊天与文件传输能力，默认关闭桌面共享、语音、插件和统计上报。

## 安全边界

影聊的目标是帮助公司管理好“这一个工具”的数据边界，而不是绕过企业网络策略：

- 核心聊天和文件传输在局域网内完成，不提供云端中转或外部文件 relay。
- 安全构建关闭第三方插件、遥测、桌面共享、语音和外部程序启动。
- ShareBox、自动下载和网络消息路径会做相对路径、目录 containment、大小和完整性校验。
- 更新只检查固定的 HTTPS 静态清单，不上传聊天内容、文件或用户信息。

请注意：局域网本身仍应使用公司的网络隔离、终端防护和访问控制策略；影聊不能限制用户使用其他软件传输文件。

## 从源码构建

构建环境：Windows、Visual Studio 2022（x86 工具链）、Qt 5.15.2 MSVC2019 x86、NSIS 3.12。

```powershell
cd E:\ShadowChat\beebeep-code-r1557
$env:PATH = 'E:\ShadowChat\Qt\5.15.2\msvc2019\bin;' + $env:PATH
qmake src\src.pro CONFIG+=release
nmake release
.\packaging\build-installer.ps1
```

最终安装包输出到 `dist\影聊-1.0.0-test-x86-安装包.exe`。发布前建议运行：

```powershell
.\tests\release\workspace_archive_test.exe
```

当前测试安装包 SHA-256：

`A71302D75A62442CB1E91B4A7075258C5D9B7ECB4EEA938B17647591D73CEB86`

## 许可证与致谢

本项目遵循 GPLv3，原始项目和第三方组件版权归各自作者所有。详见 [LICENSE.txt](LICENSE.txt) 与 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
