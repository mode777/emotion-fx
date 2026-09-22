#!/usr/bin/env node
/*
 * Cross-runtime comparison smoke test (f2b task 2.1, ADR 0022): runs the
 * same portable script through the desktop player (embedded quickjs,
 * --script mode) and the web player (native bridge, host engine = Node,
 * resource-root mode), then compares exit code and stdout line-for-line.
 * Stderr is compared on the first line only (stack traces differ by
 * engine).
 *
 * Usage: node tools/run_web_compare.mjs
 * Env:   NATIVE_PLAYER    (default build/player)
 *        WEB_PLAYER_BUILD (default build-em)
 */
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import url from 'node:url';

const ROOT = path.resolve(path.dirname(url.fileURLToPath(import.meta.url)), '..');
const NATIVE = process.env.NATIVE_PLAYER ?? path.join(ROOT, 'build', 'player');
const WEB_BUILD = process.env.WEB_PLAYER_BUILD ?? path.join(ROOT, 'build-em');
const WEB = path.join(WEB_BUILD, 'player.js');

if (!fs.existsSync(NATIVE)) {
    console.error(`native player not found: ${NATIVE} (set NATIVE_PLAYER)`);
    process.exit(2);
}
if (!fs.existsSync(WEB)) {
    console.error(`web player not found: ${WEB} (set WEB_PLAYER_BUILD)`);
    process.exit(2);
}

const CASES = [
    { name: 'log', script: 'tests/scripts/s_log.js', args: [] },
    { name: 'quit3', script: 'tests/scripts/s_quit3.js', args: [] },
    { name: 'args', script: 'tests/scripts/s_args.js', args: ['one', 'two'] },
    { name: 'portable', script: 'tests/scripts/s_portable.js', args: [] },
    { name: 'throw', script: 'tests/scripts/s_throw.js', args: [], stderrFirstLine: true },
    { name: '2d_validation', script: 'tests/scripts/s_2d_validation.js', args: [] },
    { name: 'resource_lifecycle', script: 'tests/scripts/s_resource_lifecycle.js', args: [] },
];

function run(cmd, args) {
    let code = 0;
    let out = '';
    let err = '';
    try {
        out = execFileSync(cmd, args, { encoding: 'utf8', timeout: 60000 });
    } catch (e) {
        code = e.status ?? 1;
        out = e.stdout ?? '';
        err = e.stderr ?? '';
    }
    const lines = (s) => s.split('\n').map((l) => l.replace(/\r$/, '')).filter((l) => l.length > 0);
    return { code, out: lines(out), err: lines(err) };
}

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'efx-compare-'));
let failures = 0;

for (const c of CASES) {
    process.stdout.write(`compare/${c.name}: `);
    const script = path.join(ROOT, c.script);
    const native = run(NATIVE, ['--script', script, ...c.args]);
    const rootDir = path.join(tmp, c.name);
    fs.mkdirSync(rootDir, { recursive: true });
    fs.copyFileSync(script, path.join(rootDir, 'main.js'));
    const web = run(process.execPath, [WEB, rootDir, ...c.args]);

    const problems = [];
    if (native.code !== web.code) {
        problems.push(`exit ${native.code} vs ${web.code}`);
    }
    if (JSON.stringify(native.out) !== JSON.stringify(web.out)) {
        problems.push(`stdout ${JSON.stringify(native.out)} vs ${JSON.stringify(web.out)}`);
    }
    if (c.stderrFirstLine) {
        const a = native.err[0] ?? '';
        const b = web.err[0] ?? '';
        if (!a.startsWith('uncaught exception:') || !b.startsWith('uncaught exception:') ||
            a.split(':')[1] !== b.split(':')[1]) {
            problems.push(`stderr-first ${JSON.stringify(a)} vs ${JSON.stringify(b)}`);
        }
    }
    if (problems.length > 0) {
        console.log('FAIL (' + problems.join('; ') + ')');
        failures++;
    } else {
        console.log(`PASS (exit ${native.code}, ${native.out.length} stdout line(s))`);
    }
}

fs.rmSync(tmp, { recursive: true, force: true });
console.log(failures === 0 ? 'all cross-runtime comparisons match' : `${failures} comparison failure(s)`);
process.exit(failures === 0 ? 0 : 1);
