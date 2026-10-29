# 影聊（ShadowChat）

影聊是面向公司局域网办公的轻量聊天工具，基于 BeeBEEP r1557 改造。它保留局域网直连、文件传输和无需注册的使用方式，重点改善日常聊天的易用性、界面清晰度与数据边界。

## 下载

当前开发版本为 Windows 10/11 x86 测试版 1.1.2，通过 FlowX「工具站」
`/tools/shadowchat` 分发。本地交付、dev 验收与 main 发布分别记录；
本地准备或 GitHub prerelease 不代表公司服务器已经部署。

每个交付版本同时归档源码和安装包，规则见 [逐版本发布规范](docs/VERSION-RELEASE-POLICY.md)。
本轮版本说明见 [1.1.2 测试版](docs/releases/1.1.2-test.md)。

安装后可从开始菜单或桌面启动“影聊”。覆盖安装与卸载默认保留独立账号目录中的数据。完整目标和验收范围见 [首版验收清单](docs/RELEASE-ACCEPTANCE.md)，测试步骤见 [本地测试指南](docs/LOCAL-TEST-GUIDE.md)。

## 主要改进

- 深色导航栏、现代化标题栏和更清晰的“消息 / 联系人 / 群组”结构。
- 多张图片拖入后先进入预览，一次发送为一个图片合集，不再逐张刷屏。
- 超长消息默认折叠，点击后展开全文。
- 首次启动向导，设置昵称和头像；无需联网注册或邮箱验证。
- 本地邮箱账号、密码登录与退出、默认自动登录，各账号数据隔离；邮箱无需验证，不提供中央账号管理。
- 负责人保管统一恢复密码；分发包仅包含加盐哈希，不包含明文恢复密码。
- 私聊、项目群、群主权限、@、引用、截图、粘贴和拖拽；离线文字与素材队列在本机保存。
- 历史记录按消息、文件、图片和视频、链接分类；工作区备份可选包含附件。
- 工作区导出 / 导入，用于换电脑迁移聊天偏好和本地设置。
- 1.1.2 取消强更，版本服务失败不影响启动或收发。只有更高同通道版本才自动提醒，
  每天最多一次；当前最新版不自动弹窗。手动检查可随时进行，旧 1.1.1 用户手动升级。
- 保留局域网聊天与文件传输能力，默认关闭桌面共享、语音、插件和统计上报。

## 安全边界

影聊的目标是帮助公司管理好“这一个工具”的数据边界，而不是绕过企业网络策略：

- 核心聊天和文件传输在局域网内完成，不提供云端中转或外部文件 relay。
- 安全构建关闭第三方插件、遥测、桌面共享、语音和外部程序启动。
- ShareBox、自动下载和网络消息路径会做相对路径、目录 containment、大小和完整性校验。
- 更新只读取构建环境固定的版本清单：local 为本机测试源，dev 为预发源，
  main 为正式源。不上传聊天内容、文件或用户信息，不自动安装；清单不能任意更换更新源。

请注意：局域网本身仍应使用公司的网络隔离、终端防护和访问控制策略；影聊不能限制用户使用其他软件传输文件。

## 从源码构建

构建环境：Windows、Visual Studio 2022（x86 工具链）、Qt 5.15.2 MSVC2019 x86、NSIS 3.12。

在 x86 Visual Studio Developer Command Prompt 中，将 Qt 的 `bin` 目录加入 PATH，然后运行：

```bat
scripts\build-windows.cmd
scripts\test-windows.cmd
python packaging\prepare_recovery.py
python packaging\build_installer.py
```

安装包输出路径和环境以 `dist\release-build.json` 为准，该回执记录版本、通道、
构建环境和 SHA-256。构建入口与环境参数见本轮构建报告。
可在已配置 Qt PATH 的 x86 VS 开发者命令提示符运行 `scripts\build-windows.cmd`
和 `scripts\test-windows.cmd`；后者覆盖更新提醒、账号、队列、历史、头像与备份等核心回归。发布前建议运行：

```powershell
.\tests\release\workspace_archive_test.exe
```

负责人恢复密码保存在被 Git 忽略的 `delivery-private` 目录。只向安装包复制 `shadowchat-recovery.ini` 哈希配置，明文负责人文件绝不能上传至网站。

## 许可证与致谢

本项目遵循 GPLv3，原始项目和第三方组件版权归各自作者所有。详见 [LICENSE.txt](LICENSE.txt) 与 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

界面参考 WeFlow 的通用布局思路，采用独立 Qt 实现，未分发其源码或素材。参考版本和许可证边界见 [界面参考说明](docs/UI-REFERENCE.md)。
