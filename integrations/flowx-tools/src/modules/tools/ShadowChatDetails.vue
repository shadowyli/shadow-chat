<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { RouterLink } from 'vue-router'
const release = ref(null)
const error = ref(false)
const previewUrl = `${import.meta.env.BASE_URL}tools/shadowchat/chat-preview.png`
const logoUrl = `${import.meta.env.BASE_URL}tools/shadowchat/logo.png`
let favicon, previousFavicon
onMounted(async () => {
  favicon = document.querySelector('link[rel="icon"]')
  if (favicon) { previousFavicon = favicon.getAttribute('href'); favicon.setAttribute('href', logoUrl) }
  try {
    const response = await fetch(`${import.meta.env.BASE_URL}updates/shadowchat.json`, { cache: 'no-store' })
    if (!response.ok) throw new Error('manifest unavailable')
    const data = await response.json()
    // 下载只接受本站静态路径，未发布版本永远不显示可下载按钮。
    if (data.product !== 'shadowchat' || !Array.isArray(data.notes)) throw new Error('invalid manifest')
    release.value = data
  } catch { error.value = true }
})
onUnmounted(() => { if (favicon && previousFavicon) favicon.setAttribute('href', previousFavicon) })
const canDownload = () => release.value?.published === true && /^\/downloads\/[^?#]+\.exe$/i.test(release.value?.installerUrl || '')
</script>

<template>
  <RouterLink class="tools-back" :to="{ name: 'tools' }">← 返回工具站</RouterLink>
  <div class="product-hero">
  <header class="tools-heading shadowchat-heading">
    <h1><img :src="logoUrl" alt="" width="56" height="56" style="vertical-align:middle;margin-right:16px;border-radius:12px" />影聊</h1>
    <p>FlowX团队出品 · 内网协作通讯软件</p>
  <section id="shadowchat-download" class="platform-card" aria-label="下载影聊">
    <div class="platform-heading"><svg aria-hidden="true" viewBox="0 0 24 24" width="36" height="36" fill="currentColor"><path d="M2 4.2 10.7 3v8H2zm10-1.5L22 1.4V11h-9.9zM2 12.4h8.7v8L2 19.2zm10 0H22V22l-9.9-1.4z" /></svg><div><h2>Windows</h2><p>Windows 10 / 11</p></div></div>
    <p v-if="release" class="platform-meta">v{{ release.version }} · x86<span v-if="release.size"> · {{ (release.size / 1024 / 1024).toFixed(1) }} MB</span></p>
    <a v-if="canDownload()" class="download-button" :href="release.installerUrl" download>下载安装包</a>
    <p v-else class="release-status" role="status">{{ error ? '暂时无法读取版本信息，请稍后刷新。' : release ? '验收通过后开放下载' : '正在读取版本信息…' }}</p>
  </section>
  <RouterLink :to="{name:'tools-shadowchat-changelog'}" class="version-history-link">查看完整版本记录 →</RouterLink>
  </header>
  <figure class="product-showcase">
    <img :src="previewUrl" alt="影聊 Windows 客户端界面：左侧消息和通讯录，右侧聊天与输入框" width="1280" height="820" />
  </figure>
  </div>
</template>
<style scoped>
.version-history-link{display:inline-block;margin-top:16px;color:#c3c6cf!important}.version-history-link:focus-visible{outline:2px solid #c3c6cf;outline-offset:5px}
</style>

<style scoped>
.tools-back{color:#b4b7c3;text-decoration:none;font-size:14px}.shadowchat-heading{margin-top:28px}.release-panel{display:flex;justify-content:space-between;align-items:center;gap:28px;padding:30px 32px;border:1px solid var(--tools-line);border-radius:16px;background:#17181c}.release-panel h2{font-size:24px;margin:10px 0}.release-panel p{color:var(--tools-muted);margin:5px 0}.release-version{font-size:13px}.release-status{font-size:14px}.download-button{background:var(--tools-accent);padding:12px 24px;border-radius:8px;color:#171b08;font-weight:700;text-decoration:none;white-space:nowrap}.download-button:hover{background:#d2ff55}.details-columns{display:grid;grid-template-columns:1fr 1fr;gap:56px;margin:36px 0}.details-columns h2,.release-notes h2{font-size:19px}.details-columns ul,.details-columns ol,.release-notes ul{padding-left:22px;color:var(--tools-muted)}li{margin:10px 0}.detail-note{font-size:13px;color:#d9c58e}.release-notes{border-top:1px solid var(--tools-line);padding-top:18px}.release-notes p{color:var(--tools-muted)}@media(max-width:760px){.release-panel{align-items:flex-start;flex-direction:column;padding:24px}.details-columns{grid-template-columns:1fr;gap:12px}}
.shadowchat-heading{padding:28px 0 20px;max-width:900px}.shadowchat-heading h1{font-size:58px;line-height:1.2;letter-spacing:-.035em;margin:18px 0 24px}.shadowchat-heading h1 span{display:block;color:#e4e6ec;font-size:42px;font-weight:500;margin-top:18px;letter-spacing:-.02em}.shadowchat-heading>p:last-of-type{font-size:17px;line-height:1.8;color:#aaaeba}.product-tags{display:flex;gap:10px;flex-wrap:wrap;margin:24px 0}.product-tags span{font-size:12px;padding:4px 10px;background:#202126;color:#c9cbd2;border:1px solid #32343c;border-radius:4px}.showcase-cta{display:inline-flex;align-items:center;gap:36px;text-decoration:none;color:var(--tools-accent);font-weight:700;border-bottom:1px solid #5b6e24;padding:8px 0}.showcase-cta:hover{color:#deff82}.product-showcase{margin:8px 0 48px}.showcase-caption{display:flex;justify-content:space-between;gap:16px;padding:14px 18px;color:#c7cbd4;font-size:12px;background:#1c1e24;border:1px solid #363943;border-bottom:0;border-radius:12px 12px 0 0}.showcase-caption span:last-child{color:#999eac}.product-showcase img{display:block;width:100%;height:auto;border:1px solid #363943;box-sizing:border-box}.product-showcase figcaption{font-size:12px;color:#8f95a4;margin-top:12px}#shadowchat-download{scroll-margin-top:100px}@media(max-width:760px){.shadowchat-heading{padding-top:8px}.shadowchat-heading h1{font-size:44px}.shadowchat-heading h1 span{font-size:29px}.shadowchat-heading>p:last-of-type{font-size:15px}.product-showcase{margin-bottom:28px}}
.product-hero{display:grid;grid-template-columns:minmax(0,.85fr) minmax(0,1.35fr);align-items:center;gap:44px;margin:24px 0 44px}.product-hero .shadowchat-heading{margin:0;padding:20px 0}.product-hero .shadowchat-heading h1 span{font-size:30px;line-height:1.4}.product-hero .shadowchat-heading>p:last-of-type{font-size:15px}.product-hero .product-showcase{margin:0}.product-hero .showcase-caption{font-size:11px;padding:12px}.product-hero .product-showcase figcaption{font-size:11px}@media(max-width:940px){.product-hero{grid-template-columns:1fr;gap:20px;margin-top:10px}.product-hero .shadowchat-heading h1 span{font-size:32px}.product-hero .product-showcase{margin-bottom:8px}}
</style>
<style scoped>
.product-hero{grid-template-columns:minmax(300px,.75fr) minmax(0,1.65fr);align-items:start}.product-hero .shadowchat-heading{padding:0}.product-hero .shadowchat-heading h1{font-size:48px;margin:0 0 20px}.product-hero .platform-card{margin:28px 0 0}.product-showcase img{border-radius:10px}.product-tags{gap:7px;margin:20px 0}.product-tags span{padding:3px 8px;font-size:11px}@media(max-width:940px){.product-hero{grid-template-columns:1fr;gap:30px}.product-hero .platform-card{width:300px}.product-hero .shadowchat-heading h1{font-size:40px}}
</style>
<style scoped>
.platform-card{width:300px;max-width:100%;box-sizing:border-box;padding:24px;border:1px solid var(--tools-line);border-radius:12px;background:#17181c;margin:12px 0 36px}.platform-heading{display:flex;gap:16px;align-items:center}.platform-heading svg{color:#d7dbe5;flex-shrink:0}.platform-heading h2{font-size:23px;margin:0;line-height:1.3}.platform-heading p{font-size:12px;color:var(--tools-muted);margin:4px 0 0}.platform-meta{font-size:11px;line-height:1.7;color:var(--tools-muted);margin:18px 0}.platform-card .download-button{display:block;text-align:center;padding:10px 14px;font-size:14px}.platform-card .release-status{color:var(--tools-muted);font-size:13px}.version-history-link{color:var(--tools-accent);font-size:14px;text-decoration:none}.version-history-link:hover{text-decoration:underline}
</style>
