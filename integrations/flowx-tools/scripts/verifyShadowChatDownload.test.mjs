import assert from 'node:assert/strict'
import { createHash } from 'node:crypto'
import { execFile } from 'node:child_process'
import { mkdtemp, writeFile } from 'node:fs/promises'
import { createServer } from 'node:http'
import { tmpdir } from 'node:os'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { promisify } from 'node:util'
import test from 'node:test'

const execute = promisify(execFile)
const verifier = join(dirname(fileURLToPath(import.meta.url)), 'verifyShadowChatDownload.mjs')
test('verifies complete bytes and rejects an HTTP 200 SPA fallback in place of JSON', async () => {
  const bytes = Buffer.from('synthetic complete installer')
  const receipt = {
    version: '1.1.2', channel: 'test', releaseEnvironment: 'local', size: bytes.length,
    sha256: createHash('sha256').update(bytes).digest('hex'),
  }
  const folder = await mkdtemp(join(tmpdir(), 'shadowchat-http-'))
  const receiptPath = join(folder, 'receipt.json')
  await writeFile(receiptPath, JSON.stringify(receipt))
  let html = false
  const manifest = {
    ...receipt, schemaVersion: 1, product: 'shadowchat', published: true, mandatory: false,
    downloadPage: '/tools/shadowchat',
    installerUrl: '/downloads/shadowchat/local/ShadowChat-1.1.2-local-setup.exe',
  }
  const server = createServer((request, response) => {
    if (request.url === '/updates/shadowchat.json') {
      response.setHeader('Content-Type', html ? 'text/html' : 'application/json')
      response.end(html ? '<html>SPA fallback</html>' : JSON.stringify(manifest))
    } else if (request.url === manifest.installerUrl) {
      response.end(bytes)
    } else {
      response.writeHead(404).end()
    }
  })
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve))
  try {
    const base = `http://127.0.0.1:${server.address().port}/`
    const result = await execute(process.execPath, [verifier, base, receiptPath])
    assert.equal(JSON.parse(result.stdout).passed, true)
    html = true
    await assert.rejects(execute(process.execPath, [verifier, base, receiptPath]), /Expected version JSON, got HTTP 200 text\/html/)
  } finally {
    await new Promise(resolve => server.close(resolve))
  }
})
