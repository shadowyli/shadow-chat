/** Stage an explicitly selected installer; never publishes to the LAN server. */
import { copyFile, mkdir, readFile, writeFile, stat } from 'node:fs/promises'
import { createHash } from 'node:crypto'
import { createReadStream } from 'node:fs'
import { resolve, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const [installer, version, ...options] = process.argv.slice(2)
const stable = options.includes('--stable')
const notes = options.filter(option => option !== '--stable')
if (!installer || !/^[0-9]+\.[0-9]+\.[0-9]+$/.test(version || '') || !installer.toLowerCase().endsWith('.exe')) {
  console.error('Usage: node scripts/stageShadowChatRelease.mjs <installer.exe> <numeric-version> [--stable] [release notes...]')
  process.exit(1)
}
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const source = resolve(installer)
const info = await stat(source)
if (!info.isFile() || !info.size) throw new Error('Installer must be a non-empty file')
const destination = resolve(root, 'public/downloads/shadowchat', `ShadowChat-${version}-setup.exe`)
const manifestPath = resolve(root, 'public/updates/shadowchat.json')
const previous = JSON.parse(await readFile(manifestPath, 'utf8'))
await mkdir(dirname(destination), { recursive: true })
// Do not silently replace a previously staged binary with different contents.
await copyFile(source, destination, 1)
const hash = createHash('sha256')
for await (const chunk of createReadStream(destination)) hash.update(chunk)
await writeFile(manifestPath, JSON.stringify({ ...previous, version, channel: stable ? 'stable' : 'test', published: true,
  publishedAt: new Date().toISOString(), installerUrl: `/downloads/shadowchat/ShadowChat-${version}-setup.exe`, sha256: hash.digest('hex'),
  size: info.size, notes: notes.length ? notes : [`${version} 版本发布。`], mandatory: true }, null, 2) + '\n')
console.log(`Staged ${destination}; run npm run build and review before deploying.`)
