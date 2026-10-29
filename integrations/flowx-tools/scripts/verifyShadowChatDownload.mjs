import { createHash } from 'node:crypto'
import { readFile } from 'node:fs/promises'

const [base, receiptPath] = process.argv.slice(2)
if (!base || !receiptPath) throw new Error('Usage: node verifyShadowChatDownload.mjs <base-url> <release-build.json>')
const origin = new URL(base)
const local = origin.protocol === 'http:' && ['localhost', '127.0.0.1', '[::1]'].includes(origin.hostname)
const company = origin.protocol === 'https:' && ['dev.flowx.gz', 'flowx.gz'].includes(origin.hostname)
if ((!local && !company) || origin.username || origin.password || origin.search || origin.hash || origin.pathname !== '/') {
  throw new Error('Expected an explicit localhost or company origin')
}
const receipt = JSON.parse(await readFile(receiptPath, 'utf8'))
if (!local && ((origin.hostname === 'dev.flowx.gz' && receipt.releaseEnvironment !== 'dev') ||
    (origin.hostname === 'flowx.gz' && receipt.releaseEnvironment !== 'main'))) {
  throw new Error('Build environment does not match the deployment origin')
}
const manifestResponse = await fetch(new URL('/updates/shadowchat.json', origin), {
  cache: 'no-store', redirect: 'error', signal: AbortSignal.timeout(15000),
})
if (!manifestResponse.ok || !manifestResponse.headers.get('content-type')?.includes('application/json')) {
  throw new Error(`Expected version JSON, got HTTP ${manifestResponse.status} ${manifestResponse.headers.get('content-type')}`)
}
const manifest = await manifestResponse.json()
if (manifest.product !== 'shadowchat' || manifest.published !== true || manifest.mandatory !== false ||
    manifest.version !== receipt.version || manifest.channel !== receipt.channel ||
    manifest.releaseEnvironment !== receipt.releaseEnvironment || manifest.sha256 !== receipt.sha256 ||
    manifest.size !== receipt.size || manifest.downloadPage !== '/tools/shadowchat' ||
    !/^\/downloads\/shadowchat\/(?:local|dev|main)\/ShadowChat-[0-9]+\.[0-9]+\.[0-9]+-(?:local|dev|main)-setup\.exe$/.test(manifest.installerUrl || '')) {
  throw new Error('Manifest does not match the validated build receipt and advisory policy')
}
const file = await fetch(new URL(manifest.installerUrl, origin), {
  redirect: 'error', signal: AbortSignal.timeout(120000),
})
if (!file.ok || !file.body) throw new Error(`Installer HTTP ${file.status}`)
const hash = createHash('sha256')
let size = 0
for await (const chunk of file.body) {
  size += chunk.byteLength
  hash.update(chunk)
}
const sha256 = hash.digest('hex')
if (size !== receipt.size || sha256 !== receipt.sha256) throw new Error('Downloaded installer size or SHA-256 mismatch')
console.log(JSON.stringify({
  passed: true, origin: origin.origin, version: receipt.version, channel: receipt.channel,
  releaseEnvironment: receipt.releaseEnvironment, mandatory: false, size, sha256,
  manifestStatus: manifestResponse.status, downloadStatus: file.status,
  scope: 'Version JSON and complete installer bytes; does not prove installation or office network acceptance',
}, null, 2))
