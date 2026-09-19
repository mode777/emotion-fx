class Twice {
    constructor(n) { this.n = n; }
    get v() { return this.n * 2; }
}

const set = new Set([1, 2]);
const [x, , z] = [1, 2, 3];
const fmt = (head, ...rest) => `${head}-${rest.length}`;
const pair = { a: 10, b: 20 };
const { a, ...others } = pair;
const promised = new Promise((resolve) => resolve(7));

const hostGlobals = ['window', 'document', 'require', 'process', 'fetch', 'XMLHttpRequest']
    .filter((k) => typeof globalThis[k] !== 'undefined');

if (hostGlobals.length > 0) {
    efx.log('host globals leaked: ' + hostGlobals.join(','));
    efx.quit(1);
}

const ok = set.has(2) && x === 1 && z === 3 && fmt(9, 8, 7) === '9-2'
    && a === 10 && others.b === 20 && (new Twice(21).v) === 42;
efx.quit(ok ? 0 : 2);
