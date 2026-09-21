#!/usr/bin/env node
/*
 * Emscripten golden-image driver (ADR 0020 / design D9):
 *  - serves the capture build over http,
 *  - loads player_web_golden.js in pinned headless Chrome (SwiftShader WebGL),
 *  - for each golden scene: callMain(['--capture-frame','2',...]), waits for
 *    the MEMFS capture, pulls it out as base64,
 *  - compares with the native efx_imgdiff binary against the committed golden.
 *
 * Usage: node tools/run_web_goldens.mjs
 * Env:   CHROME_PATH (optional path to a chrome binary)
 */
import { execFileSync } from 'node:child_process';
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import url from 'node:url';

let puppeteer;
try {
    puppeteer = (await import('puppeteer-core')).default;
} catch {
    console.error('puppeteer-core not installed: npm install puppeteer-core');
    process.exit(2);
}

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const BUILD = process.env.WEB_GOLDEN_BUILD ?? path.join(ROOT, 'build-web-golden');
const NATIVE_BUILD = process.env.NATIVE_BUILD ?? path.join(ROOT, 'build-golden-tools');
const SCENES_DIR = path.join(ROOT, 'tests', 'goldens');
const OUT_DIR = path.join(BUILD, 'goldens');
const PORT = 18123;

const PAGE_HTML = `<!doctype html>
<html><head><meta charset="utf-8"><title>efx golden capture</title></head>
<body><canvas id="canvas" width="640" height="480"></canvas>
<!-- keep the compositor producing BeginFrames in headless so rAF (and the
     emscripten main loop) keeps ticking; DOM is not part of the GL readback -->
<style>@keyframes k { from { transform: translateY(0); } to { transform: translateY(1px); } }</style>
<div style="position:fixed;width:1px;height:1px;background:#123;animation:k 0.016s linear infinite alternate;"></div>
<script>
window.__rafCount = 0;
const __raf = window.requestAnimationFrame.bind(window);
window.requestAnimationFrame = (cb) => { window.__rafCount++; return __raf(cb); };
</script>
<script src="/player_web_golden.js"></script></body></html>`;

const scenes = fs
    .readdirSync(SCENES_DIR)
    .filter((s) => fs.existsSync(path.join(SCENES_DIR, s, 'main.js')));
if (scenes.length === 0) {
    console.error('no golden scenes found');
    process.exit(2);
}
fs.mkdirSync(OUT_DIR, { recursive: true });

const MIME = { '.wasm': 'application/wasm', '.js': 'text/javascript', '.data': 'application/octet-stream' };
const server = http.createServer((req, res) => {
    if (req.url === '/' || req.url.startsWith('/?')) {
        res.setHeader('Content-Type', 'text/html');
        res.end(PAGE_HTML);
        return;
    }
    const p = path.join(BUILD, decodeURIComponent(req.url.split('?')[0].slice(1)));
    try {
        const data = fs.readFileSync(p);
        res.setHeader('Content-Type', MIME[path.extname(p)] ?? 'application/octet-stream');
        console.log('[serve]', req.url, data.length, 'bytes');
        res.end(data);
    } catch {
        console.log('[serve] 404:', req.url);
        res.statusCode = 404;
        res.end('nope');
    }
});
await new Promise((r) => server.listen(PORT, r));

const browser = await puppeteer.launch({
    executablePath: process.env.CHROME_PATH || undefined,
    headless: true,
    args: [
        '--no-sandbox',
        '--use-gl=angle',
        '--use-angle=swiftshader',
        '--enable-unsafe-swiftshader',
        '--disable-gpu-sandbox',
        '--disable-background-timer-throttling',
        '--disable-renderer-backgrounding',
        '--disable-backgrounding-occluded-windows',
    ],
});
const page = await browser.newPage();
page.on('console', (m) => console.log('[chrome]', m.text()));
page.on('pageerror', (e) => console.error('[pageerror]', e.message));

let failures = 0;
for (const scene of scenes) {
    process.stdout.write(`golden_web/${scene}: `);
    await page.goto(`http://localhost:${PORT}/?scene=${scene}`, { waitUntil: 'load' });
    const b64 = await page.evaluate(async (name) => {
        const M = window.Module;
        if (typeof M.FS?.readFile !== 'function') {
            throw new Error('FS missing');
        }
        for (let i = 0; i < 600; i++) {
            try {
                const bytes = M.FS.readFile(`/captures/${name}.png`);
                let bin = '';
                for (const b of bytes) bin += String.fromCharCode(b);
                return btoa(bin);
            } catch {
                await new Promise((r) => setTimeout(r, 100));
            }
        }
        console.log('[diag] raf ticks:', window.__rafCount,
            'webgl2:', !!document.createElement('canvas').getContext('webgl2'));
        return null;
    }, scene);

    if (!b64) {
        console.log('FAIL (capture timed out)');
        failures++;
        continue;
    }
    const actual = path.join(OUT_DIR, `${scene}-actual.png`);
    const diff = path.join(OUT_DIR, `${scene}-diff.png`);
    fs.writeFileSync(actual, Buffer.from(b64, 'base64'));
    const imgdiff = path.join(NATIVE_BUILD, 'tests', 'efx_imgdiff');
    try {
        execFileSync(imgdiff, [actual, path.join(SCENES_DIR, scene, 'golden.png'), diff], { stdio: 'inherit' });
        console.log('PASS');
    } catch {
        console.log('FAIL (mismatch; diff: ' + diff + ')');
        failures++;
    }
}

await browser.close();
server.close();
console.log(failures === 0 ? 'all web goldens pass' : `${failures} web golden failure(s)`);
process.exit(failures === 0 ? 0 : 1);
