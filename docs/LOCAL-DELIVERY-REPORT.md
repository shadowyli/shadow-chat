# 影聊 1.1.0 测试版：本地交付验收

本文是 2026-10-28 的 1.1.0 历史交付记录。1.1.1 的构建、强制更新与安装验证
见 `RELEASE-1.1.1-BUILD.md`，发布行为见 `VERSION-RELEASE-POLICY.md`；
不能将本报告的旧安装包摘要或“无强更”行为用于新版验收。

本地入口：http://127.0.0.1:5199/tools/shadowchat 。仅供本机访问，尚未部署公司 FlowX。

## 验证范围

| 完成标准 | 证据 | 范围 |
| --- | --- | --- |
| FlowX 顶部工具站、介绍页、真实下载 | `public/updates/shadowchat.json`、`test/final-release-integrity.log` | 实际浏览器顶部导航进入工具站和影聊详情通过；最终包已接入；页面同源链接 HTTP 下载与 SHA-256 通过。浏览器下载完成回执两次超时，不能声称浏览器保存已通过 |
| 源码构建、安装、覆盖保留数据 | `dist/release-build.json`、`dist/installation-verification.json`、`E:/ShadowChat/delivery/final-installed-startup.txt` | 最终安装包两次安装通过，程序与源码构建哈希相同；账号/历史/附件夹具字节不变；实际启动后读取到真实登录界面 |
| 本地邮箱账号、隔离、密码与恢复、自动登录 | `ACCOUNT-NETWORK-EVIDENCE.md` | 生产账号逻辑与真实登录窗口夹具；不验证邮箱所有权 |
| 私聊、群聊、离线与逐人送达 | `ACCOUNT-NETWORK-EVIDENCE.md`、`DELIVERY-EVIDENCE.md` | 独立本机进程真实 TCP；20 进程通讯通过，不等于 20 台办公电脑 |
| 图片、文件、工程文件夹 | `DELIVERY-EVIDENCE.md` | 两张 PNG 字节一致；503 文件目录重复两次共 1006 次完成，保留空目录与零字节文件 |
| 5GB 文件 | `ACCOUNT-NETWORK-EVIDENCE.md` | 5 GiB 完整传输与双方流式 SHA-256 一致；测试源为稀疏文件，不能推断真实素材传输速度 |
| 历史重启、搜索、保留与清空 | `ACCOUNT-NETWORK-EVIDENCE.md`、`UX-EVIDENCE.md` | 实际持久化后新进程加载；10051 条首尾保留；分类和定位使用生产 QWidget |
| 备份、恢复、附件迁移、损坏拒绝 | `ACCOUNT-NETWORK-EVIDENCE.md`、`test/root-final-tests.log` | 实际收发后迁移，新进程加载并核对附件；恢复保留新密码；会话设置一并迁移 |
| 置顶、免打扰、隐藏 | `test/conversation-preferences-build.log` | 实际生产会话列表操作与新进程持久化；未删除历史、文件、群成员 |
| 失败手动从头重试 | `test/transfer-failure.log` | 真实失败项菜单在关闭断点续传时仍可重试；残片截断、编号另存、原素材不变，共 15 项断言通过 |
| 素材通知响应 | `UX-EVIDENCE.md` | 最终生产对象：500 文件通知 920ms、500 独立图片通知 508ms，文档未被整页清空；批次图片仍保留分组原图范围 |
| 内网更新 | `ACCOUNT-NETWORK-EVIDENCE.md` | 生产网络客户端与本地 HTTP 服务验证 15 种成功及失败场景；公司站点尚未部署 |

## 必须在办公环境继续验证

两台真实办公电脑的发现与防火墙策略、20 人网络负载、Windows 10 与 Windows 11 分别安装、无管理员权限机器安装及企业限制。安装程序声明当前用户权限；本机测试不代表企业策略允许。

安装包附带 Qt 与 Microsoft 官方可再发行 x86 VC++ 运行库，采用程序目录内加载，不要求用户先装 Visual Studio 或以管理员身份安装运行库。最终包的运行库哈希随 `release-build.json` 保存，安装后逐项核对。

真实桌面鼠标截图框选、系统提示音和通知呈现、默认软件打开素材仍需人工操作。当前自动化环境拒绝桌面输入；合成图裁剪、真实剪贴板粘贴、生产拖放处理器、引用与 @ 已通过自动化测试。

测试流程见 `LOCAL-TEST-GUIDE.md`。统一恢复密码在项目 `delivery-private/负责人恢复密码.txt`，仅负责人保管，不上传工具站。安装包仅包含加盐密码哈希。

最终安装包：`E:/ShadowChat/beebeep-code-r1557/dist/影聊-1.1.0-test-x86-安装包.exe`，37,221,733 字节。

SHA-256：`ce63d2a72a6eeb86c534261b6df9e2f63a1d3b55ac62a55f2fd86363fb7e6c6a`。

页面链接实际 HTTP 下载副本：`E:/ShadowChat/delivery/ShadowChat-1.1.0-http-download.exe`。该副本安装两次通过，安装后程序与最终源码构建哈希相同，独立资料夹具内容不变。构建、原安装包、网站暂存包、清单、下载副本及安装程序一致。

FlowX `npm run build` 通过（40.55 秒），顶部导航测试 5/5 通过。最终页面截图：`E:/ShadowChat/delivery/flowx-tools-final-hero.png`。

浏览器保存验证限制：Chrome `waitForEvent('download')` 与下载链接 `downloadMedia` 分别等待 30 秒和 20 秒超时；不能据此断定产品链接下载失败。另一个可用浏览器 IAB 不可用。标准下载目录未找到本次浏览器文件；没有绕过浏览器 URL 访问策略或伪造浏览器下载成功。需要用户实际点击并确认保存，故该项仍未完成自动验收。

后续诊断：HEAD 返回 HTTP 200、`application/octet-stream`、Content-Length 37,221,733（`test/download-http-headers.json`）。支持的浏览器能力未提供下载管理接口；现有同源页面下载事件没有返回完成证据，不能据此判断浏览器已阻止。已请求用户实际点击并确认是否保存；等待期间不重复发布同一版本或改动有效安装包。

## 浏览器保存人工确认

2026-10-28，用户对“请在本地下载页点击下载安装包，是否能成功保存 ShadowChat-1.1.0-setup.exe？”明确回复：**能成功保存**。因此浏览器保存环节已由用户实测确认；此前自动化回执超时不再构成本地交付阻碍。该确认不扩展为多台办公电脑、企业权限或现场网络验收通过。
