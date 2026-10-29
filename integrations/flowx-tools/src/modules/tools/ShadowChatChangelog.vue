<script setup>
import { RouterLink } from 'vue-router'
import { onMounted, onUnmounted } from 'vue'
let favicon, previousFavicon
onMounted(() => { favicon = document.querySelector('link[rel="icon"]'); if (favicon) { previousFavicon = favicon.getAttribute('href'); favicon.setAttribute('href', `${import.meta.env.BASE_URL}tools/shadowchat/logo.png`) } })
onUnmounted(() => { if (favicon && previousFavicon) favicon.setAttribute('href', previousFavicon) })
const releases = [
  { version: '1.1.1', date: '2026-10-29', groups: [
    { label: '新增', items: ['采用新的影聊应用标识。', '启动和使用过程中强制检查更新，旧版本需升级后才能继续使用。'] },
    { label: '改进', items: ['统一个人资料、同事资料、建群、设置、聊天记录与文件传输界面。', '个人资料先显示资料卡，编辑时展开必要字段，附加资料可按需展开。', '统一常用按钮、字体和群聊图标，让操作层级更清楚。'] },
    { label: '修复', items: ['改善修改头像后的本地显示与重启保留。', '修复没有在线连接时，已知同事被空状态遮挡的问题。'] },
  ] },
  { version: '1.1.0', date: '2026-10-28', groups: [
    { label: '新增', items: ['本地账号与独立聊天记录空间。', '局域网私聊、群聊，以及图片、文件和工程文件夹传输。', '按消息、文件、图片和视频、链接查找聊天记录。', '聊天记录和附件备份迁移，以及覆盖安装升级。'] },
  ] },
]
</script>
<template>
  <RouterLink class="changelog-back" :to="{name:'tools-shadowchat'}">← 返回影聊</RouterLink>
  <header class="changelog-heading"><h1>版本记录</h1><p>影聊 · Windows</p></header>
  <article v-for="release in releases" :key="release.version" class="changelog-release">
    <div class="changelog-version"><h2>{{ release.version }}</h2><time :datetime="release.date">{{ release.date }}</time></div>
    <section v-for="group in release.groups" :key="group.label"><h3>{{ group.label }}</h3><ul><li v-for="item in group.items" :key="item">{{ item }}</li></ul></section>
  </article>
</template>
<style scoped>
.changelog-release ul{list-style:none;padding-left:0}.changelog-release li{position:relative;padding-left:16px}.changelog-release li::before{content:'-';position:absolute;left:0}
</style>
<style scoped>
.changelog-back{font-size:14px;color:#a6a8b4;text-decoration:none}.changelog-heading{margin:28px 0 44px}.changelog-heading h1{font-size:36px;margin:0 0 8px}.changelog-heading p{color:#a6a8b4;margin:0}.changelog-release{max-width:850px;border-top:1px solid #303138;padding:28px 0 36px}.changelog-version{display:flex;align-items:baseline;gap:20px;margin-bottom:26px}.changelog-version h2{font-size:26px;margin:0}.changelog-version time{font-size:13px;color:#8e94a3}.changelog-release section{margin:22px 0}.changelog-release h3{font-size:14px;color:#c4fd18;font-weight:600;margin:0 0 8px}.changelog-release ul{margin:0;padding-left:20px;color:#c3c6cf;line-height:1.9}.changelog-release li{margin:6px 0}
</style>
