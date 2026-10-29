# FlowX 工具站接入

这是影聊在现有 FlowX Vue 3 / Vue Router 门户中的增量接入源码，不包含完整 FlowX、内部业务模块或安装包。保留宿主现有登录守卫与品牌导航。复制 `src/modules/tools`、`scripts`、`public/tools/shadowchat` 到宿主同名位置。

## 路由

在现有 AppShell 的 children 中增加以下入口，组件路径相对 `src/router/index.js`：

```js
{
  path: 'tools', name: 'tools',
  meta: { shellTitle: SHELL_TITLE, shellHideNavRail: true },
  component: () => import('../modules/tools/ToolsView.vue'),
  children: [
    { path: 'shadowchat', name: 'tools-shadowchat', component: () => import('../modules/tools/ShadowChatDetails.vue') },
    { path: 'shadowchat/changelog', name: 'tools-shadowchat-changelog', component: () => import('../modules/tools/ShadowChatChangelog.vue') },
  ],
}
```

## 顶部入口

在现有 `SHELL_TOP_NAV_ITEMS` 的“无限画布”后追加：

```js
{ id: 'tools', label: '工具站', routeName: 'tools', tone: 'default', exactMatch: false }
```

普通成员可见，沿用门户登录。路由名称前缀用于子页面激活状态，不要把影聊产品 logo 替换到 FlowX 的品牌位置：

```js
import { useRoute } from 'vue-router'
const route = useRoute()
const isCurrentSection = item => route.name === item.routeName ||
  (!item.exactMatch && String(route.name || '').startsWith(`${item.routeName}-`))
```

在原 RouterLink 上绑定现有激活样式及 `:aria-current="isCurrentSection(item) ? 'page' : undefined"`。不改变其它入口可见性规则。

## 发布资料

首次建立 `public/updates/shadowchat.json`：

```json
{"schemaVersion":1,"product":"shadowchat","version":"1.1.2","channel":"test","releaseEnvironment":"local","published":false,"downloadPage":"/tools/shadowchat","installerUrl":null,"sha256":null,"size":null,"notes":[],"mandatory":false}
```

验收后在宿主项目根目录运行：

```text
node scripts/stageShadowChatRelease.mjs <安装包绝对路径.exe> 1.1.2 --environment=local --receipt=<release-build.json绝对路径> "本次更新说明"
npm run build
```

脚本核对构建回执的版本、环境、通道、SHA-256 和大小后更新清单。local/dev 为
test，main 为 stable，不能把 local 测试包直接改标签当正式包。包按环境放入
`public/downloads/shadowchat/<环境>/`，同版本不同字节拒绝覆盖；相同字节可重复暂存。
安装包不提交 Git。先上传包后更新清单，清单应 `Cache-Control: no-cache`，
不能让 SPA fallback 用 HTML 首页冒充 JSON。脚本只暂存本机资料，不部署线上。

1.1.2 取消强更，脚本写入 `mandatory: false`。客户端只有发现同通道更高版本
才自动提醒，每天最多一次，最新版本不弹窗。版本服务异常不影响登录或收发，
手动检查可显示错误并重试。旧 1.1.1 用户需手动覆盖升级。

按本地 → dev → main 顺序发布。dev 与 main 使用对应环境构建回执，main 发布与
PR 合并须经用户确认；缺少服务器权限时提供材料，不把本地暂存称为已部署。
可运行 `node --test scripts/stageShadowChatRelease.test.mjs` 验证暂存保护。

`logo.png` 从当前客户端 `branding/generated/shadowchat-256.png` 技术缩放资产复制，未改用户提供的颜色。`chat-preview.png` 是当前真实 Qt 主窗口的合成会话验收截图，演示联系人并非真实同事；不用于证明真实网络连接。版本记录由团队根据已实现事实维护，不包含参考网站代码或素材。

本次宿主验证：导航测试 5 项；Vite 构建；真实浏览器顶部入口、列表、详情和版本记录；安装包 HTTP 全内容 SHA-256。浏览器保存回执是否通过应以最新交付说明为准，不将 HTTP 校验替代为浏览器保存证明。
