<p align="center">
  <img src="branding/generated/shadowchat-128.png" width="80" height="80" alt="影聊图标">
</p>

<h1 align="center">影聊 · ShadowChat</h1>
<p align="center">同一局域网，直接沟通。</p>

<p align="center">
  <a href="LICENSE.txt"><img src="https://img.shields.io/badge/license-GPLv3-blue" alt="许可证 GPLv3"></a>
  <img src="https://img.shields.io/badge/platform-Windows%2010%20%2F%2011-0078D4" alt="Windows 10 和 11">
  <img src="https://img.shields.io/badge/status-preview-d97706" alt="测试版阶段">
</p>

<p align="center">
  <a href="https://github.com/shadowyli/shadow-chat/releases">下载</a> ·
  <a href="docs/GETTING-STARTED.md">使用指南</a> ·
  <a href="CONTRIBUTING.md">参与贡献</a> ·
  <a href="README.en.md">English</a>
</p>

影聊是面向办公内网和同网段小团队的开源 Windows 通讯工具。
无需部署中央聊天服务器，即可在局域网内进行私聊、群聊和文件传输，
聊天记录与账号资料保存在本机。

当前测试版本 **1.2.0** 提供更新检查备用通路、资料与弹层优化、
GIF／图片／视频预览、历史覆盖面板和精确定位，并修复启动偏好与静态通知；
详见 [1.2.0 版本说明](docs/RELEASE-1.2.0.md)。
安装包、源码和校验信息以本仓库 Releases 为准；`local / test` 客户端直接检查
对应 GitHub 版本，继承默认启动与建议更新。
维护与交接见 [版本发布规则](docs/VERSION-RELEASE-POLICY.md)。

项目由 **shadowyli** 个人维护，基于 **BeeBEEP r1557**，
使用 C++ / Qt 改进会话组织、消息展示和日常操作体验。

## 项目特点

- **局域网沟通**：设备间直接通信，支持私聊、群聊、@ 和引用。
- **顺手传资料**：支持图片、文件及文件夹，多张图片统一预览和发送。
- **本地整理信息**：常用会话、静态未读提醒、全局搜索与聊天记录分类检索。
- **保留与迁移资料**：本地账号资料隔离，支持工作区导出、导入及换机。

## 快速开始

当前提供 **Windows 10/11 x86 测试版**，安装包包含运行所需组件。

1. 从 [Releases](https://github.com/shadowyli/shadow-chat/releases) 下载 `*-setup.exe`，
   阅读该版说明，并按 `SHA256SUMS.txt` 核对文件。
2. 运行安装包并创建本地账号；覆盖升级前先从系统托盘退出影聊。
3. 在另一台同一局域网电脑启动影聊，等待发现对方后开始聊天。

完整步骤见 [使用指南](docs/GETTING-STARTED.md)。
版本变化、验证范围及已知限制以对应 Release 为准；当前测试包未进行商业代码签名。

使用前需确认网络允许设备间通信。邮箱仅作本地账号标识，项目不提供中央身份认证或云端历史同步；
本地资料与备份未加密，请妥善保管。更多信息见 [安全说明](SECURITY.md)。

## 文档

| 内容 | 入口 |
| --- | --- |
| 安装、升级、备份与换机 | [使用指南](docs/GETTING-STARTED.md) |
| 账号、离线消息与网络排障 | [常见问题](docs/FAQ.md) |
| Windows 源码构建、测试与打包 | [构建指南](docs/BUILDING.md) |
| 问题反馈与使用求助 | [支持说明](SUPPORT.md) |

## 参与贡献

欢迎文档纠错、翻译、问题复现和代码贡献。请先阅读 [贡献指南](CONTRIBUTING.md)，
通过 [Issues](https://github.com/shadowyli/shadow-chat/issues) 反馈问题或讨论建议。
社区协作遵循 [行为准则](CODE_OF_CONDUCT.md)。

## 许可证与致谢

项目采用 [GNU GPLv3](LICENSE.txt)，保留上游版权声明。
感谢 **Marco Mastroddi** 与 [BeeBEEP](https://www.beebeep.net/) 贡献者提供局域网通讯基础，
以及 Qt、Phosphor Icons 等组件。第三方归属见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)，
运行库版本及许可随对应 Release 附件提供。
