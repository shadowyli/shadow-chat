# 影聊（ShadowChat）

影聊是由 **shadowyli** 个人维护的开源局域网通讯工具，提供私聊、群聊、图片与文件传输、
本地聊天记录和账号资料管理。

## 下载与版本

准备中的测试版本：**1.1.6**，支持 Windows 10/11（x86 应用）。
安装包、源码、版本说明和校验信息统一在本仓库的 **Releases** 发布。
其他网站可以提供下载镜像，版本记录以本仓库已发布的 Release 为准。

计划交付 `local / test` 安装包，更新源指向本机测试服务；当前尚未完成冻结、构建和验收。
公开发布构建从本仓库 Releases 查询对应通道的版本。
每个版本保留对应源码、安装包、发布说明、SHA-256 和构建信息。
本轮更新见 [1.1.6 版本说明草稿](docs/releases/1.1.6-test.md)；
整合状态与后续命令见 [1.1.6 发布准备](docs/RELEASE-1.1.6-PREPARATION.md)。

## 功能

- 局域网私聊、群聊，支持图片、文件和文件夹传输。
- 圆角聊天气泡、通讯录分类、全局搜索、常用会话和静态未读提醒。
- 本地账号与数据隔离，聊天记录分类检索、导出及迁移。
- 个人资料、头像、签名、电话号码及工作状态管理。
- 截图预览、消息发送反馈、离线消息与待发送队列。
- 建议更新：只有发现更高版本才提醒，每天最多一次；最新版不自动弹窗。
- 更新服务不可用不影响启动、登录、聊天和文件传输。

## 安装与升级

从 Releases 下载适合的安装包；先退出托盘中的影聊，再运行安装程序。
覆盖安装保留安装目录外的账号、聊天记录和接收文件，重要资料请提前备份。
本地账号不提供云端历史同步或邮箱所有权验证。

## 构建

构建工具：Windows、Visual Studio 2022 x86 工具链、Qt 5.15.2 MSVC2019 x86、
Python 3 和 NSIS 3.12。配置相应工具后运行：

```bat
scripts\release-windows.cmd build --environment local
scripts\release-windows.cmd test --environment local
python packaging\build_installer.py --environment local
```

私密恢复资料与本机构建目录不得提交到 Git。构建回执记录实际版本、
源码身份、程序摘要和验证结果，详见 [发布规则](docs/VERSION-RELEASE-POLICY.md)。

## 许可证

项目遵循 GPLv3。原有版权声明、许可证及实际使用的组件许可随源码和安装包保留，
详见 [LICENSE.txt](LICENSE.txt) 与 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
