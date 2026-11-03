# 通知、托盘与初始应用图标专项

本轮只修改 `GuiSystemTray.cpp`、`IconManager.cpp`、`BeeApplication.cpp`、
`beebeep.qrc`，新增未读资产及本目录专项测试。现有禁止闪烁逻辑保持不变。
未修改 GuiMain、GuiChat、Transcript、Sticker、desktop.pri 或 packaging。

## 资源与行为

- 正常托盘、通知图片、初始 QApplication/Windows 窗口图标使用现有红色影聊图。
- 未读状态使用用户提供并归档在 `src/images/shadowchat/shadowchat-unread-source.png` 的原图，
  按静态未读图标替换解释。
  没有恢复 timer、图标交替或任务栏关注闪烁。
- 原始 1080×1080 RGBA PNG 按字节归档为
  `src/images/shadowchat/shadowchat-unread-source.png`。
  SHA-256 为 `62b0336451868dbc018a7b857ace7a39bad6971d88c24b5a5c3a5f87a67235f2`。
- `shadowchat-unread.ico` 包含 16/20/24/32/40/48/64/128/256 像素帧；
  每帧独立从原始 PNG 做 LANCZOS 缩放并无损 PNG 编码，保留全部画面和透明度。
  没有裁切、重绘或改色。生成信息和校验值在同目录的 unread manifest 中。
- IconManager 在外部主题查找之前固定应用/未读品牌资源，防止旧主题重新覆盖品牌。
  `beebeep.png`、`beebeep-tray.png` 兼容名称指向红色图；
  `beebeep-message.png`、`beebeep-message-tray.png` 指向新未读图。
  QRC 中也覆盖这四个兼容别名，包含绕过 IconManager 的直接 QIcon/QPixmap 调用。
- 通知正文与标题参数顺序修正为标题“影聊”、正文实际消息。

## Windows 通知图片依赖

Qt 5.15.2 的 `QWindowsSystemTrayIcon::showMessage` 把传入的 QIcon 转为
`NOTIFYICONDATAW.hBalloonIcon`，使用 `NIIF_USER` 调用 `Shell_NotifyIconW(NIM_MODIFY)`。
该图片独立于 EXE 的 `.rc` 应用图标，也独立于当前托盘 `hIcon`。
因此仅更换 EXE 图标不能修复原先明确传入的黄色 BeeBEEP 未读图。

专项测试发现，高 DPI 下 Qt 的这条通知路径会先获取高 DPI pixmap，再按
`GetSystemMetrics(SM_CXICON/SM_CYICON)` 尺寸重新缩放。代码在 Windows 上先把
现有红色 ICO 用 QIcon::paint 绘制到该实际尺寸的单帧 pixmap，避免通知路径再次缩放。
托盘与应用窗口仍使用原有多尺寸 ICO。

依据：

- Qt 5.15.2 源码：
  https://github.com/qt/qtbase/blob/v5.15.2/src/plugins/platforms/windows/qwindowssystemtrayicon.cpp
- Microsoft NOTIFYICONDATAW：
  https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataw

## 本机验证（2026-11-02）

证据目录：`dist/branding-agent-20261102/`。只编译独立测试，复用已有
dev-preview 生产对象；没有执行主程序构建、提交或发布。

| 检查 | 结果 |
| --- | --- |
| 原图 SHA-256、RGBA、尺寸 | 通过 |
| 九个 ICO 帧与原图直接缩放结果逐像素比较 | 通过，完全相同 |
| 红色正常 ICO 256 帧与现有红图比较 | 通过，完全相同 |
| 四个旧资源别名直接读取与新资产比较 | 通过 |
| 人工旧主题覆盖应用/未读图标 | 被固定品牌映射阻止 |
| 100%、125%、150%、200% 缩放 | 全部通过 |
| BeeApplication 构造后、创建 GuiMain 之前的全局图标 | 红色影聊图 |
| 无 EXE 图标资源的合成窗口 WM_GETICON | 红色影聊图 |
| 正常→未读、未读会话更新、清除→正常、无效下一会话→正常 | 通过 |
| 未读静置 2600ms | 0 次托盘 timer event、0 次 native 图标更新 |
| 实际 qwindows.dll 的 Shell_NotifyIconW 图片参数 | 托盘状态正确、通知为红色影聊图 |
| 新消息、文件、状态通知的标题/图片；通知关闭时抑制 | 通过 |
| 一次真实 Shell_NotifyIconW 本机合成通知 | Windows Shell 返回成功 |
| 当前已安装 ShadowChat.exe 的 PE 应用图标 | 九帧与当前红色 ICO 的帧字节哈希全部一致 |
| 现有 dev-preview ShadowChat.exe 的 PE 应用图标 | 九帧与当前红色 ICO 的帧字节哈希全部一致 |

Windows 原生 HICON 的透明色预乘转换存在最多 1/255 的单通道舍入；
原生图像比较允许此误差。源 PNG/ICO 派生及 Qt 资源比较不允许像素误差。

各缩放目录保存 `branding-results.json`、初始 Windows 图标、正常/未读/清除托盘图、
通知图片和测试日志。`native-shell/` 保存真实转发给 Windows Shell 的通知参数证据。
`pe-icon-results.json` 保存安装版及既有预览版 EXE 的只读验证结果。

真实通知测试只实例化 BeeApplication/GuiSystemTray 和临时设置，不启动 Core、
服务器、真实会话或消息发送。没有向他人发送消息。没有清理系统通知、图标缓存或用户资料。

未捕获到可访问的 Shell 通知弹窗，不能把 Shell 返回成功当作通知中心最终视觉验收。
本次证明了原生 API 收到的图像和参数，并执行了一次真实通知调用。
现有安装 EXE 没有被替换，本轮资源/逻辑需要后续发布构建才能进入安装版。

## 复现

以下命令从仓库根目录执行；Python 环境需要 Pillow。资产校验：

```text
python tests/branding_assets_test.py
python tests/branding_assets_test.py --executable dist/local/production/bin/ShadowChat.exe
```

若需从已归档原图重新生成未读 ICO 和 manifest：

```text
python tests/branding_assets_test.py --generate
```

在独立构建目录、配置好仓库已有 MSVC x86 + Qt 5.15.2 工具链后：

```text
qmake "<repo-root>/tests/branding_icons_test.pro" "SHADOWCHAT_PREVIEW_BUILD=<repo-root>/dist/local/production"
nmake /nologo
```

`<repo-root>` 替换为当前源码仓库的绝对路径，qmake 和 nmake 在独立构建目录执行；
不依赖维护者的开发机目录。为测试进程设置 `QT_QPA_PLATFORM=windows`、
`QT_SCALE_FACTOR=1`（或 1.25/1.5/2），
以及 `SHADOWCHAT_BRANDING_PROOF_DIR=<独立证据目录>`，运行 `branding_icons_test.exe`。
默认只在测试进程拦截 qwindows.dll 的 Shell_NotifyIconW 参数，不显示系统通知。
显式附加 `--native-notification` 会向 Windows Shell 转发并显示一条本机合成通知。

## 给安装／发布代理的具体建议

1. 继续使用发布侧现有红色 `.rc`/安装器/快捷方式图标。无需用未读 ICO 替换 EXE 图标。
   不要执行全局清通知、删除通知数据库或清空 Shell 图标缓存。已有通知不作为新代码的图片证据；
   覆盖安装并重新启动后，新增一条本机合成通知检查来源小图、正文图片、名称与点击归属。
2. 当前程序继续走 Qt 的传统 Shell_NotifyIcon 路径。本轮仅交付图标和通知参数修复；
   不为系统来源栏可能显示的 `ShadowChat.exe` 引入 AUMID、现代 toast 或新的安装注册。
   来源栏名称在实际 Windows 环境中的表现单独记录，不承诺未经验证的身份显示行为。
3. 本文所列专项结果来自 agent 独立测试；最终发布仍须对本次完整生产对象、打包后的
   运行库和安装版新通知复测，不能将既有预览或旧安装程序的证据当作新版本验证通过。

本轮没有单独设置一个与安装器不一致的进程 AUMID，也没有修改安装注册。
