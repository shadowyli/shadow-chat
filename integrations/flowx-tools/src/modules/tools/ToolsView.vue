<script setup>
import { useRoute, RouterLink, RouterView } from 'vue-router'
const route = useRoute()
const logoUrl = `${import.meta.env.BASE_URL}tools/shadowchat/logo.png`
</script>

<template>
  <main class="tools-page">
    <RouterView v-if="route.name !== 'tools'" />
    <template v-else>
      <header class="tools-heading"><h1>工具站</h1><p>FlowX 团队出品</p></header>
      <RouterLink class="software-card" :to="{ name: 'tools-shadowchat' }">
        <div class="software-mark" aria-hidden="true"><img :src="logoUrl" alt="" width="88" height="88" /></div>
        <div><h2>影聊 <span>ShadowChat</span></h2><p>内网协作通讯软件</p></div>
      </RouterLink>
    </template>
  </main>
</template>

<style>
.tools-page{--tools-accent:#c4fd18;--tools-line:#303138;--tools-muted:#a6a8b4;max-width:1200px;margin:0 auto;padding:48px 36px 80px;color:#f2f3f5;font-size:16px;line-height:1.65}.tools-heading{margin-bottom:36px}.tools-eyebrow{font-size:12px;font-weight:700;letter-spacing:.14em;color:var(--tools-accent)}.tools-heading h1{font-size:36px;margin:8px 0;font-weight:700}.tools-heading>p:last-child{color:var(--tools-muted);margin:0}.software-card{display:flex;align-items:center;gap:28px;padding:32px;border:1px solid var(--tools-line);border-radius:16px;background:#17181c;color:inherit;text-decoration:none;transition:border-color .15s,background .15s}.software-card:hover{border-color:#697b38;background:#1c1f18}.software-mark{width:88px;height:88px;flex-shrink:0;border-radius:22px;background:#c4fd18;color:#171b08;display:grid;place-items:center}.software-mark svg{width:50px;height:50px}.software-card h2{font-size:27px;margin:8px 0}.software-card h2 span{font-size:16px;font-weight:400;color:var(--tools-muted);margin-left:8px}.software-card p{margin:0;color:var(--tools-muted)}.tools-badge{display:inline-block;font-size:12px;padding:2px 9px;border:1px solid #5b613b;border-radius:5px;color:#d9e69b}.software-meta{font-size:13px;color:var(--tools-muted);margin-top:16px}.software-meta span{margin:0 9px}.software-action{margin-left:auto;white-space:nowrap;color:var(--tools-accent)}.tools-footnote{color:var(--tools-muted);font-size:13px;margin-top:24px}.tools-page a:focus-visible,.tools-page button:focus-visible{outline:2px solid var(--tools-accent);outline-offset:5px}@media(max-width:760px){.tools-page{padding:28px 18px}.software-card{align-items:flex-start;flex-wrap:wrap;gap:18px;padding:24px}.software-mark{width:64px;height:64px;border-radius:16px}.software-action{margin-left:0}.software-card h2 span{display:block;margin-left:0}.tools-heading h1{font-size:30px}}
</style>
<style scoped>
.software-mark{background:transparent;border:0;box-shadow:none}.software-mark img{display:block;width:100%;height:100%;object-fit:contain}.software-card:hover{border-color:var(--tools-line);background:#1a1b20}.software-mark:hover{box-shadow:none}
</style>
