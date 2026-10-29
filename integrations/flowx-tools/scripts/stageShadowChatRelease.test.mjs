import assert from 'node:assert/strict'
import { createHash } from 'node:crypto'
import { execFileSync } from 'node:child_process'
import { copyFile, mkdtemp, mkdir, readFile, writeFile } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import test from 'node:test'

const script = join(dirname(fileURLToPath(import.meta.url)), 'stageShadowChatRelease.mjs')
async function fixture(environment = 'local') {
  const root = await mkdtemp(join(tmpdir(), 'shadowchat-stage-'))
  await mkdir(join(root, 'scripts'))
  await copyFile(script, join(root, 'scripts/stage.mjs'))
  const bytes = Buffer.from('synthetic installer fixture')
  const receipt = {
    version: '1.1.2',
    releaseEnvironment: environment,
    channel: environment === 'main' ? 'stable' : 'test',
    size: bytes.length,
    sha256: createHash('sha256').update(bytes).digest('hex'),
  }
  await writeFile(join(root, 'fixture.exe'), bytes)
  await writeFile(join(root, 'receipt.json'), JSON.stringify(receipt))
  return { root, receipt }
}
function run(root, environment) {
  return execFileSync(process.execPath, [join(root, 'scripts/stage.mjs'), join(root, 'fixture.exe'),
    '1.1.2', `--environment=${environment}`, `--receipt=${join(root, 'receipt.json')}`],
  { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] })
}
test('stages the exact local package with advisory metadata and immutable environment path', async () => {
  const { root, receipt } = await fixture()
  run(root, 'local')
  run(root, 'local')
  const manifest = JSON.parse(await readFile(join(root, 'public/updates/shadowchat.json'), 'utf8'))
  assert.equal(manifest.mandatory, false)
  assert.equal(manifest.channel, 'test')
  assert.equal(manifest.releaseEnvironment, 'local')
  assert.equal(manifest.sha256, receipt.sha256)
  assert.equal(manifest.installerUrl, '/downloads/shadowchat/local/ShadowChat-1.1.2-local-setup.exe')
})
test('refuses relabeling a local test build as main stable', async () => {
  const { root } = await fixture()
  assert.throws(() => run(root, 'main'), /Installer differs/)
})
test('refuses bytes that do not match the validated build receipt', async () => {
  const { root } = await fixture()
  await writeFile(join(root, 'fixture.exe'), 'changed fixture')
  assert.throws(() => run(root, 'local'), /Installer differs/)
})
test('refuses replacing an existing same-version installer with different bytes', async () => {
  const { root } = await fixture('dev')
  run(root, 'dev')
  await writeFile(join(root, 'public/downloads/shadowchat/dev/ShadowChat-1.1.2-dev-setup.exe'), 'different published bytes')
  assert.throws(() => run(root, 'dev'), /EEXIST/)
})
