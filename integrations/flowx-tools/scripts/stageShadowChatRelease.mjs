import { constants, createReadStream } from 'node:fs'
import { copyFile, mkdir, readFile, rename, stat, writeFile } from 'node:fs/promises'
import { createHash, randomUUID } from 'node:crypto'
import { resolve, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const [installer, version, ...options] = process.argv.slice(2)
const environmentOption = options.find(option => option.startsWith('--environment='))
const receiptOption = options.find(option => option.startsWith('--receipt='))
const environment = environmentOption?.slice('--environment='.length)
const receiptPath = receiptOption?.slice('--receipt='.length)
const unknownOptions = options.filter(option => option.startsWith('--') && option !== environmentOption && option !== receiptOption)
if (!installer || !/^[0-9]+\.[0-9]+\.[0-9]+$/.test(version || '') ||
    !installer.toLowerCase().endsWith('.exe') || !['local', 'dev', 'main'].includes(environment) ||
    !receiptPath || unknownOptions.length) {
  throw new Error('Usage: node scripts/stageShadowChatRelease.mjs <installer.exe> <version> --environment=local|dev|main --receipt=<release-build.json> [notes...]')
}
const channel = environment === 'main' ? 'stable' : 'test'
const notes = options.filter(option => !option.startsWith('--'))
const root = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const source = resolve(installer)
const info = await stat(source)
if (!info.isFile() || !info.size) throw new Error('Installer must be a non-empty file')
const receipt = JSON.parse(await readFile(resolve(receiptPath), 'utf8'))
const hashFile = async filename => {
  const hash = createHash('sha256')
  for await (const chunk of createReadStream(filename)) hash.update(chunk)
  return hash.digest('hex')
}
const digest = await hashFile(source)
if (receipt.version !== version || receipt.channel !== channel || receipt.releaseEnvironment !== environment ||
    receipt.sha256 !== digest || receipt.size !== info.size) {
  throw new Error('Installer differs from the validated version, environment, channel, size or SHA-256 receipt')
}
const installerUrl = `/downloads/shadowchat/${environment}/ShadowChat-${version}-${environment}-setup.exe`
const destination = resolve(root, `public${installerUrl}`)
const manifestPath = resolve(root, 'public/updates/shadowchat.json')
await mkdir(dirname(destination), { recursive: true })
try {
  await copyFile(source, destination, constants.COPYFILE_EXCL)
} catch (error) {
  if (error.code !== 'EEXIST' || await hashFile(destination) !== digest) throw error
}
const manifest = {
  schemaVersion: 1,
  product: 'shadowchat',
  version,
  channel,
  releaseEnvironment: environment,
  published: true,
  downloadPage: '/tools/shadowchat',
  installerUrl,
  sha256: digest,
  size: info.size,
  notes: notes.length ? notes : [`${version} 版本发布。`],
  mandatory: false,
  publishedAt: new Date().toISOString(),
}
await mkdir(dirname(manifestPath), { recursive: true })
const temporary = `${manifestPath}.${randomUUID()}.tmp`
await writeFile(temporary, JSON.stringify(manifest, null, 2) + '\n')
await rename(temporary, manifestPath)
console.log(`Staged ${environment}/${channel}: ${destination}. No server deployment performed.`)
