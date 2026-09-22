#!/usr/bin/env node
/*
 * Browser runtime harness (f2b tasks 1.2/3.1/3.2, ADR 0022): drives the
 * golden capture build in pinned headless Chrome (SwiftShader WebGL) and
 * asserts the web entry-script lifecycle: trivial boot + one frame + quit,
 * hook/eval error surfacing, missing main.js, and loop-stop + exit-code
 * reporting via Module.efxExitCode.
 *
 * Usage: node tools/run_web_harness.mjs
 * Env:   CHROME_SHELL_PATH / CHROME_PATH, WEB_GOLDEN_BUILD
 */
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
const PORT = 18124;

const PAGE_HTML = `<!doctype html>
<html><head><meta charset="utf-8"><title>efx web harness</title></head>
<body><canvas id="canvas" width="640" height="480"></canvas>
<style>@keyframes k { from { transform: translateY(0); } to { transform: translateY(1px); } }</style>
<div style="position:fixed;width:1px;height:1px;background:#123;animation:k 0.016s linear infinite alternate;"></div>
<script>
window.__rafCount = 0;
const __raf = window.requestAnimationFrame.bind(window);
window.requestAnimationFrame = (cb) => {
    window.__rafCount++;
    return __raf((t) => { try { cb(t); } catch (e) { console.log('[raf-cb-throw]', e && (e.message || e)); throw e; } });
};
window.addEventListener('error', (e) => console.log('[page-err]', e.message));
</script>
<script src="/player_web_golden.js"></script></body></html>`;

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
        res.end(data);
    } catch {
        res.statusCode = 404;
        res.end('nope');
    }
});
await new Promise((r) => server.listen(PORT, r));

const browser = await puppeteer.launch({
    executablePath: process.env.CHROME_SHELL_PATH || process.env.CHROME_PATH || undefined,
    headless: 'shell',
    args: [
        '--no-sandbox',
        '--use-gl=angle',
        '--use-angle=swiftshader',
        '--enable-unsafe-swiftshader',
        '--disable-gpu-sandbox',
        '--enable-begin-frame-control',
        '--run-all-compositor-stages-before-draw',
    ],
});
const page = await browser.newPage();
const consoleLines = [];
page.on('console', (m) => consoleLines.push(`${m.type()}: ${m.text()}`));
page.on('pageerror', (e) => consoleLines.push(`pageerror: ${e.message}`));
const cdp = await page.createCDPSession();

async function beginFrame() {
    try {
        await cdp.send('HeadlessExperimental.beginFrame', {});
    } catch (e) {
        console.log('[beginFrame-error]', e.message);
    }
}

async function runScenario(scenario) {
    consoleLines.length = 0;
    process.stdout.write(`harness/${scenario.name}: `);
    await page.goto(`http://localhost:${PORT}/?root=${scenario.root}`, { waitUntil: 'load' });
    let ended = false;
    let exitCode = null;
    let raf = 0;
    for (let i = 0; i < 120 && !ended; i++) {
        ended = await page.evaluate(() => !!(window.Module && window.Module['efxRunEnded']));
        if (!ended) {
            await beginFrame();
        }
    }
    if (ended) {
        exitCode = await page.evaluate(() => {
            const v = window.Module && window.Module['efxExitCode'];
            return typeof v === 'number' ? v : null;
        });
    }
    if (exitCode === null) {
        console.log('FAIL (no exit code reported)');
        console.log(consoleLines.map((l) => '  | ' + l).join('\n'));
        return false;
    }
    // after a terminal state the rAF-driven loop must be stopped
    raf = await page.evaluate(() => window.__rafCount);
    for (let i = 0; i < 5; i++) {
        await beginFrame();
    }
    const rafAfter = await page.evaluate(() => window.__rafCount);
    const problems = [];
    if (exitCode !== scenario.exit) {
        problems.push(`exit ${exitCode} != ${scenario.exit}`);
    }
    for (const needle of scenario.consoleIncludes) {
        if (!consoleLines.some((l) => l.includes(needle))) {
            problems.push(`console missing ${JSON.stringify(needle)}`);
        }
    }
    if (scenario.expectLoopStop && rafAfter !== raf) {
        problems.push(`loop kept ticking (${raf} -> ${rafAfter})`);
    }
    if (problems.length > 0) {
        console.log('FAIL (' + problems.join('; ') + ')');
        console.log(consoleLines.map((l) => '  | ' + l).join('\n'));
        return false;
    }
    console.log(`PASS (exit ${exitCode}, rAF stopped at ${raf})`);
    return true;
}

const F = '/fixtures';
const scenarios = [
    { name: 'boot_ok', root: `${F}/root_ok`, exit: 0, consoleIncludes: ['entry-ok'], expectLoopStop: true },
    { name: 'one_frame_quit', root: `${F}/web/root_frame`, exit: 0, consoleIncludes: ['frame-1'], expectLoopStop: true },
    { name: 'hook_throw', root: `${F}/web/root_hook_throw`, exit: 1, consoleIncludes: ['uncaught exception: hook boom'], expectLoopStop: true },
    { name: 'eval_throw', root: `${F}/web/root_eval_throw`, exit: 1, consoleIncludes: ['intentional-smoke-throw'], expectLoopStop: true },
    { name: 'no_entry', root: `${F}/root_no_entry`, exit: 1, consoleIncludes: ['no main.js in resource root'], expectLoopStop: true },
    { name: 'missing_dir', root: `${F}/no_such_dir`, exit: 1, consoleIncludes: ['not a directory'], expectLoopStop: true },
];

let failures = 0;
for (const s of scenarios) {
    if (!(await runScenario(s))) {
        failures++;
    }
}

await browser.close();
server.close();
console.log(failures === 0 ? 'all web harness scenarios pass' : `${failures} web harness failure(s)`);
process.exit(failures === 0 ? 0 : 1);
