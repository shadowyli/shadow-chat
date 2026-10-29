# 本地账号与网络验收证据

日期：2026-10-28。验证环境：Windows、Qt 5.15.2、MSVC x86 Release。

## 账号行为

实现位置：`src/desktop/ShadowLocalAccounts.{h,cpp}`、`src/desktop/Main.cpp`、`Settings::setAccountDataFolder()`。

- 邮箱转小写并作为本机独立目录名，资料位于当前 Windows 用户的 `文档/影聊/accounts/<邮箱>/`。
- 登录前不加载该账号的聊天数据库；账号内设置、缓存、聊天记录和默认接收目录独立。
- 密码保存为随机盐和 PBKDF2-HMAC-SHA256 校验值，100,000 次迭代；不保存明文密码。此功能用于软件内账号隔离，不加密磁盘上的聊天或附件。
- 默认自动登录上次账号，菜单可关闭；主动退出清除记住的账号，正常停止收发、保存状态并重新打开登录页。
- 恢复密码由负责人设置。程序优先使用有效的本机恢复配置，否则读取安装目录中的 `shadowchat-recovery.ini`。导出的团队恢复配置仅包含加盐校验值，可随同一个安装包部署，不包含明文恢复密码。
- 原账号密码不是备份恢复的前提。导入后可以设置新的本机密码；备份内的邮箱与目标邮箱不一致时拒绝混入。账号备份不携带负责人恢复密码。
- 安装后的首次成功登录默认注册当前 Windows 用户的开机启动项；现有高级设置可以关闭开机启动。
- 对端宣告相同邮箱时发出提示，说明历史不会自动同步，不阻止本机登录。

测试：`tests/local_accounts_test.{cpp,pro}`；命令：`test/build-accounts.cmd`；日志：`test/accounts-build.log`。

通过的检查包括邮箱归一化、路径穿越拒绝、账号独立目录、错误密码拒绝、重复创建拒绝、加盐存储、恢复密码校验、重设密码保留历史、导出恢复配置不含明文、退出和自动登录开关，以及备份迁移后使用新密码。

测试也实际创建生产登录对话框，使用测试生成的账号和密码触发登录，验证后续自动登录，以及退出后重新显示登录页。没有使用真实用户凭据。截图为 `E:/ShadowChat/delivery/login-proof.png`，由对话框在填入测试账号前直接 `grab()` 生成。

## 两个真实进程的收发与重启

测试：`tests/loopback_chat_test.{cpp,pro}`；命令：`test/build-loopback.cmd`；日志：`test/loopback-build.log`。

两个进程链接生产代码中的 Core、Protocol、ConnectionSocket、文件传输、消息持久化和历史加载实现，使用独立临时数据目录。仅通过明确指定的 loopback 地址连接，关闭发现广播，不接触真实同事或用户记录。

验证结果：

- 双向文本消息成功接收。
- 双向文件完成接收和上传确认。
- 接收文件内容逐字节相等。
- 两个发送/接收进程正常退出后，分别启动新的独立进程，通过生产 `BuildSavedChatList` 重新加载两边历史，并找回双方发送的文本。

日志包含 `ALPHA exit 0`、`BETA exit 0`、`Received file byte verification: PASS` 和两条 `Restart history verification: PASS`。

普通小文件模式还把上述真实收发产生的 `beebeep.dat` 和实际接收目录通过生产 `exportWithAttachments` 导出，导入第三个全新目录，再启动独立进程恢复。验证双方实际文本仍存在、历史记录解析得到的接收文件链接指向新目录、不再指向原接收根目录、恢复文件内容与原接收文件相等。最终标记为 `Live chat -> backup -> new profile -> new-process restore: PASS`；这条证据使用真实聊天数据库，不是任意字节的伪造数据库。

测试中必须连接 `BeeApplication::tickEvent` 到 `Core::onTickEvent`，与生产主窗口提供的定时驱动保持一致。初版测试遗漏该连接会导致已缓冲帧不能及时继续读取；该测试问题已修正。

## 5 GiB 实际传输

命令：`test/build-loopback.cmd --large`；日志：`test/loopback-large.log`。

测试创建 5,368,709,120 字节的稀疏源文件，写入首尾标记，通过实际文件传输协议发送。接收端实际接收并写入完整文件；不是仅测试文件大小元数据。完成后按流读取全部源文件和接收文件，比较 SHA-256，随后执行上述独立进程历史重载。

结果：上传、接收、上传确认全部成功；接收尺寸为 5,368,709,120 字节；完整 SHA-256 相等；两个进程退出码为 0；历史重载通过。日志 225—236 行包含最终 PASS（若重跑，以日志中的文字标记为准）。

这是稀疏、以零字节为主的测试素材，压缩率高，能够验证大于 4 GiB 的长度、协议、流式读写及文件完整性，不能代表不可压缩的真实 PSD/视频在办公室网络上的耗时。

## 20 个同时运行的真实进程

命令：`test/build-loopback.cmd --twenty`；日志：`test/loopback-twenty.log`。

20 个进程使用独立临时目录和不同端口，同时运行生产 Core。组织者 `node0` 连接其余 19 个进程，分别发送文本；各参与者回复，验证发送确认。

结果：组织者记录 `sent=19 received=19 all ACKed`；其余 19 个进程均记录 `sent=1 received=1 all ACKed`；20 个进程退出码全部为 0；最终标记为 `20-process capacity verification: PASS`。

这证明同机 20 个客户端实例的通讯及确认逻辑通过。它不是 20 台办公电脑的现场验收，也未证明公司 Wi-Fi/有线互通、防火墙权限、跨网段发现或真实网络吞吐。现场仍须验证这些网络条件。

## 历史保留与主动清空

测试：`tests/history_retention_test.{cpp,pro}`；命令：`test/build-history-retention.cmd`；日志：`test/history-retention-build.log`。

测试先写入旧配置的 9000 行上限，然后通过生产 Settings 加载，确认影聊保留全部历史策略生效。使用生产 `MessageManager::saveMessages` 保存 10,051 条实时聊天和另外 10,051 条仅存档历史；启动新进程，通过 `BuildSavedChatList` 读取并验证首条、末条及完整条数，再重新保存已恢复历史。

随后另一个新进程确认重新保存未截断，并通过生产 `Core::clearMessagesInChat` 主动清空指定聊天。最后一个新进程确认指定聊天已清空，另一份存档仍完整，预先保存的本地素材内容未变。四个阶段和总测试均通过、退出码为 0。旧的静默保留 9000 行实现会丢失首条，不能通过这个测试。

最后一轮还验证关闭未来历史保存后，生产保存入口返回未保存，但已有数据库文件的字节保持完全一致，不再删除原历史。最终日志第 9—14 行全部 PASS。

## 会话置顶、免打扰与隐藏

实现：`ShadowConversationPreferences.h`、`GuiChatList`、`GuiChatItem`，以及主窗口的提醒／重新打开会话接入。偏好保存于当前账号的 `conversation-preferences.ini`；私聊使用对端稳定 hash，项目群使用稳定群标识，不依赖本次运行的数字 ID。

测试：`tests/conversation_preferences_test.{cpp,pro}`；命令：`test/build-conversation-preferences.cmd`；日志：`test/conversation-preferences-build.log`。

测试实际显示生产会话列表并触发真实右键菜单：置顶后排在最新未读会话前，免打扰状态显示在行数据中，移除后私聊和群会话仅被隐藏。验证聊天数据库字节完全没有改变、群成员未改变、现存消息未被清空。选择第一条会话也不会发出清空聊天请求。

同一菜单还实际测试取消置顶、关闭免打扰，再重新启用，确认状态切换与勾选一致。

随后在新的独立进程中改变所有数字会话／用户 ID，验证同一 peer hash 和群标识仍恢复置顶、免打扰和隐藏状态。验证主窗口所用 reveal API 能恢复显示、切换到另一账号目录不继承偏好、原聊天记录及本地素材仍完整。测试退出码 0。主窗口另外负责免打扰抑制通知、被 @ 时例外，并在真实新内容和用户主动打开会话时调用 reveal；连接状态或历史加载不会自行恢复隐藏会话。

## 实际更新检查网络客户端

测试：`tests/update_network_test.{cpp,pro}`；命令：`test/build-update-network.cmd`；日志：`test/update-network-build.log`。

测试启动真正的临时 `QTcpServer`，通过生产 `Updater` 和 `QNetworkAccessManager` 发出 HTTP 请求，不替换网络客户端，也不向 FlowX 网站发布虚假版本。生产默认地址仍为 `https://flowx.gz/updates/shadowchat.json`。新增的可选构造参数只接受这个 HTTPS FlowX 地址或 HTTP loopback/localhost，路径必须精确匹配，无用户名、密码、查询参数或片段。

15 个场景全部通过：有效已发布新版本、localhost 地址、旧版本、未发布版本、无效 JSON、无效版本、HTTP 503、HTTP 重定向、超限响应、8 秒不响应超时、连接拒绝、外部 URL 拒绝、错误路径拒绝、URL 凭据拒绝、查询参数拒绝。每个场景只发出一次 `jobCompleted`。有效响应返回同源下载页；失败场景不留下版本或下载地址；请求不发送 Cookie 或 Authorization。旧版本由客户端返回供现有 Core 的版本比较逻辑判断，测试同时验证它不满足新版本条件。

更新检查改动后完整客户端再次编译通过，集成网络测试退出码为 0。此结果验证客户端实现，不代表正式 FlowX 域名、证书、已部署清单或现场下载已验收。

## 构建与证据边界

最终冻结源代码构建及上述保留／会话偏好测试均退出码 0。对应 `test/ShadowChat.exe` 为 25,136,128 字节，文件时间 2026-10-28 19:34，SHA-256 为 `2bdb90ff8af4a0abfb089c1a01f7d4a1f77719cab5411ae96d1b10902850bc0e`。安装包应使用该次构建；如果后续重新编译，应重新记录二进制身份。

客户端通过 `test/build-integrated.cmd` 调用同一 Qt/MSVC 工具链编译；日志为 `test/integrated-build.log`。协议或模型布局修改后，必须先完整更新关联对象，再运行链接生产对象的测试，避免把混用旧对象导致的失败误判为产品行为。

最终图片批次 UUID、退群重新邀请和文件夹接收目录修改完成后，完整客户端编译通过（退出码 0），随后再次链接最新生产对象运行普通双进程收发测试，文本、文件、接收字节校验和双方新进程历史恢复再次全部通过。

随后生产网络层修正合并帧继续读取，并移除每个文件的随机建立连接延迟；由于这会影响实际网络路径，在关联对象全部重新编译后，重新执行了 20 个真实进程与 5 GiB 实际传输。两项均再次退出码 0：20 个进程全部通过，组织者 19 发／19 收并全部确认；5 GiB 文件完整 SHA-256 相等，双方新进程历史重载通过。最新证据覆盖该网络修正，日志仍为 `test/loopback-twenty.log` 与 `test/loopback-large.log`（最终 PASS 分别在约 2263 行、232—235 行）。

本文记录已完成的账号、实际网络、文件大小及本机并发验证；群离线补发、退群/重新邀请、图片批次、安装包和 FlowX 站点验证由各自测试记录补充，不以本文件代替。
