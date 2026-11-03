# 嵌入式视频运行库与发布验证

视频使用官方 VLC/libVLC 3.0.24 win32 二进制，随影聊暂存在 `video-runtime/`，
不安装或调用独立播放器。视频窗口源码为 `ShadowVideoPreview.cpp/.h`。
GuiMain 的最新历史 overlay/context 路由由主 agent 冻结后单独集成，不能使用 Video 作者的早期主窗口版本覆盖。

本文文件路径均相对仓库根目录，准备与库存检查命令从仓库根目录执行。
`<新的暂存目录>`、`<本次回执>` 是本次独立目录／文件名占位，使用前替换；
不依赖任何维护者私人目录。

## 固定来源与许可

- 官方 ZIP：`https://download.videolan.org/pub/videolan/vlc/3.0.24/win32/vlc-3.0.24-win32.zip`
- ZIP SHA-256：`8511356afd680817f3aea624c63032d0936f3d77b2175fb36c8fc16adf9744e8`
- 对应 VLC 源码及摘要在 `video-runtime/runtime-manifest.json` 的 `sources` 中记录，
  并随 `SOURCE-AND-LICENSES.txt` 交付。
- 完整保留 `COPYING.txt`、`COPYING.LIB`、原有作者/说明文件及 runtime manifest。
  libVLC 和插件的许可证以随包原文为准，不能把所有组件笼统写为同一种许可证。
- 作者核对的 runtime inventory 为 308 项、104,837,717 字节，另有 runtime manifest；
  22 种 PE 导入依赖在 manifest 中逐项标记为 bundled 或 Windows system。

## Cache 和 fresh payload

公开 cache 默认为 `dist/deps/vlc-3.0.24-win32/`，不提交 Git。
离线暂存要求 cache 中已有官方 ZIP、`official-download-verification.json` 和已固定摘要的 `COPYING.LIB`。
脚本会重新核对 ZIP/许可字节摘要及官方验证记录，直接从 ZIP 生成新目录，
不会把旧 Qt payload、预览 executable 或资料目录复制进新包。

```text
python -B -X utf8 packaging/prepare_video_runtime.py --offline --cache dist/deps/vlc-3.0.24-win32
python -B -X utf8 packaging/stage_video_runtime.py --cache dist/deps/vlc-3.0.24-win32 --payload dist/local/<新的暂存目录> --report dist/release-agent/<本次回执>.json
python -B -X utf8 tests/video_runtime_test.py
```

`stage_video_runtime.py` 本身不构建主程序或安装包；目标 `video-runtime` 已存在时拒绝复用。
完整安装包仍由 `build_installer.py` 构造新鲜 Qt/CRT/TLS payload，再调用 prepare 脚本，
可通过 `--video-cache` 或 `SHADOWCHAT_VIDEO_CACHE` 指定 cache。
所有运行库文件纳入最终 `payloadInventory`，build receipt 另记 `videoRuntime` 的来源、
manifest 摘要、数量、字节数、PE 依赖和许可证。

dev-preview runner 先校验本副本 cache/runtime，再暂存到本次隔离预览目录并复核；
发布副本保留动态版本号/环境判断，不能退回开发区脚本的 1.1.2 固定值。

## 测试边界

Video 作者报告 clean PATH 下 100%/200% 的原生播放、seek、关闭、无外部播放器调用，
及原先六项 packaging 检查通过。对应文件及作者哈希保存在 release-agent 的 Video 冻结快照。
video_preview_test.pro 的最新生产对象分支较作者早期清单新增，已单独审查并归档。

本轮发布代理新增离线 cache 验证、fresh payload 暂存、库存/许可/架构验证及对应 Python 回归。
最终生产对象的视频专项已在真实 Qt Windows 100%／150%／200% 执行通过，
证据见 `dist/primary-ui-proof/VERIFICATION.json` 和 `video_preview_test/` 子目录。
这些专项与安装包验证分别记录，不能将作者早期独立测试当作最终安装包验证。

在已配置 MSVC x86、Qt 5.15.2 的独立构建目录复现生产对象测试时使用：

```text
qmake "<repo-root>/tests/video_preview_test.pro" "SHADOWCHAT_PREVIEW_BUILD=<repo-root>/dist/local/production" QT+=multimediawidgets
nmake /nologo
```

将 `<repo-root>` 替换为当前源码仓库绝对路径；运行前按仓库测试流程暂存并校验
与发布包相同的 `video-runtime/`，设置 `QT_QPA_PLATFORM=windows` 及所需
`QT_SCALE_FACTOR`，运行 `video_preview_test.exe` 并记录实际加载的 runtime 路径。
GUI 输出上限 1920×1080；2560×1440 已实播，4K 未实播。
