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
{"schemaVersion":1,"product":"shadowchat","version":"1.1.1","channel":"test","published":false,"downloadPage":"/tools/shadowchat","installerUrl":null,"sha256":null,"size":null,"notes":[],"mandatory":true}
```

验收后在宿主项目根目录运行：

```text
node scripts/stageShadowChatRelease.mjs <安装包绝对路径.exe> 1.1.1 "本次更新说明"
npm run build
```

脚本计算安装包 SHA-256、大小并更新清单；同版本文件不可覆盖，默认标为测试通道。只有正式验收后才使用 `--stable`。安装包放在 `public/downloads/shadowchat`，不提交 Git。部署先上传安装包再更新清单，版本清单配置 `Cache-Control: no-cache`；服务器保留 SPA fallback。脚本只暂存本机资料，不部署线上。

从 1.1.1 起强制更新，脚本始终写入 `mandatory: true`。客户端启动和运行中校验
版本，旧版或无法校验时不放行。正式试用前必须先部署可访问的版本清单、下载页
和安装包；GitHub Release 不替代公司版本服务。旧客户端需要先人工升级一次。

`logo.png` 从当前客户端 `branding/generated/shadowchat-256.png` 技术缩放资产复制，未改用户提供的颜色。`chat-preview.png` 是当前真实 Qt 主窗口的合成会话验收截图，演示联系人并非真实同事；不用于证明真实网络连接。版本记录由团队根据已实现事实维护，不包含参考网站代码或素材。

本次宿主验证：导航测试 5 项；Vite 构建；真实浏览器顶部入口、列表、详情和版本记录；安装包 HTTP 全内容 SHA-256。浏览器保存回执是否通过应以最新交付说明为准，不将 HTTP 校验替代为浏览器保存证明。
