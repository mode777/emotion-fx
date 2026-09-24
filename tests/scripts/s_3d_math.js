// F3 smoke: math layer + procedural primitives (portable). Asserts the
// same documented matrix values as the C math unit tests (tests/unit/
// math_tests.c) — the cross-check between the JS helpers and the GLM
// wrapper (design D10).
function near(a, b) { return Math.abs(a - b) < 1e-4; }
function fail(msg) { efx.log('FAIL ' + msg); efx.quit(2); }

// identity
const I = efx.mat4.identity();
const Iexpect = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
for (let i = 0; i < 16; i++) { if (!near(I[i], Iexpect[i])) fail('identity'); }

// perspective (GL-style RH): m[0] = f/aspect, m[5] = f, m[11] = -1
const f = 1 / Math.tan(Math.PI / 6); // fov 60
const Pm = efx.mat4.perspective(60, 4 / 3, 0.1, 100);
if (!near(Pm[0], f / (4 / 3))) fail('perspective m[0]');
if (!near(Pm[5], f)) fail('perspective m[5]');
if (!near(Pm[10], -(100 + 0.1) / (100 - 0.1))) fail('perspective m[10]');
if (!near(Pm[11], -1)) fail('perspective m[11]');
if (!near(Pm[14], -2 * 100 * 0.1 / (100 - 0.1))) fail('perspective m[14]');

// ortho: 2/width, 2/height
const Om = efx.mat4.ortho(4, 2, 1, 11);
if (!near(Om[0], 0.5)) fail('ortho m[0]');
if (!near(Om[5], 1)) fail('ortho m[5]');
if (!near(Om[10], -2 / 10)) fail('ortho m[10]');
if (!near(Om[14], -12 / 10)) fail('ortho m[14]');

// multiply order: multiply(t, r) applies r first
const T = efx.mat4.translate(efx.mat4.identity(), [5, 0, 0]);
const R = efx.mat4.rotate(efx.mat4.identity(), 90, [0, 1, 0]);
const TR = efx.mat4.multiply(T, R);
const RT = efx.mat4.multiply(R, T);
const v0 = [0, 0, 0];
function xform(m, p) {
    return [
        m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12],
        m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
        m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14],
    ];
}
let q = xform(TR, v0);
if (!near(q[0], 5) || !near(q[1], 0) || !near(q[2], 0)) fail('mul(T,R) order');
q = xform(RT, v0);
if (!near(q[0], 0) || !near(q[1], 0) || !near(q[2], -5)) fail('mul(R,T) order');

// rotate 90 about Y: x axis maps to -z (right-handed)
q = xform(R, [1, 0, 0]);
if (!near(q[0], 0) || !near(q[1], 0) || !near(q[2], -1)) fail('rotate 90 Y');

// purity: inputs untouched
const M0 = efx.mat4.identity();
const T1 = efx.mat4.translate(M0, [1, 2, 3]);
if (!near(M0[12], 0)) fail('translate mutated input');
if (!near(T1[12], 1)) fail('translate result');
const A = [3, 0, 4];
const N = efx.vec3.normalize(A);
if (!near(A[0], 3)) fail('normalize mutated input');
if (!near(N[0], 0.6) || !near(N[2], 0.8)) fail('normalize values');
if (efx.vec3.normalize([0, 0, 0]).some((x) => !near(x, 0))) fail('normalize zero');
const cr = efx.vec3.cross([1, 0, 0], [0, 1, 0]);
if (!near(cr[2], 1)) fail('cross');
if (!near(efx.vec3.dot([1, 2, 3], [4, 5, 6]), 32)) fail('dot');
const sm = efx.mat4.scale(efx.mat4.identity(), [2, 3, 4]);
if (!near(sm[0], 2) || !near(sm[5], 3) || !near(sm[10], 4)) fail('scale');

// quat: axis angle -> mat4 rotates the same way
const qid = efx.quat.identity();
if (!near(qid[3], 1)) fail('quat identity');
const qy = efx.quat.fromAxisAngle(90, [0, 1, 0]);
const Rq = efx.quat.toMat4(qy);
const qr = xform(Rq, [1, 0, 0]);
if (!near(qr[0], 0) || !near(qr[2], -1)) fail('quat toMat4 rotate');
const qid2 = efx.quat.multiply(efx.quat.identity(), qy);
if (!near(qid2[1], qy[1]) || !near(qid2[3], qy[3])) fail('quat multiply identity');
// 180+180 = identity (mod sign)
const q180 = efx.quat.fromAxisAngle(180, [0, 0, 1]);
const q360 = efx.quat.multiply(q180, q180);
if (!near(Math.abs(q360[3]), 1)) fail('quat 360');

// primitives: pinned layouts (design D9) — inspected by wrapping the
// public createMeshData entry the primitives call
function spyPrim(fn) {
    const orig = efx.createMeshData;
    let captured = null;
    efx.createMeshData = function (data) { captured = data; return orig(data); };
    const md = fn();
    efx.createMeshData = orig;
    if (md.surfaceCount !== 1) fail('primitive surfaceCount');
    return captured;
}

// cube: 24 verts / 36 indices; corners at +-size/2
const cube = spyPrim(() => efx.makeCube());
if (cube.positions.length !== 72 || cube.indices.length !== 36) fail('cube layout');
if (!cube.normals || cube.normals.length !== 72) fail('cube normals');
if (!cube.uvs || cube.uvs.length !== 48) fail('cube uvs');
// face +X first corner is (h, -h, h) for size 1
if (!near(cube.positions[0], 0.5) || !near(cube.positions[1], -0.5) ||
    !near(cube.positions[2], 0.5)) fail('cube corner');
const cube2 = spyPrim(() => efx.makeCube({ size: 2 }));
if (!near(cube2.positions[0], 1) || !near(cube2.positions[1], -1) ||
    !near(cube2.positions[2], 1)) fail('cube size');

// plane: (segments+1)^2 grid on XZ facing +Y; z rows from -h to +h
const plane = spyPrim(() => efx.makePlane());
if (plane.positions.length !== 4 * 3) fail('plane 1x1 grid'); // (1+1)^2 = 4
if (!near(plane.positions[1], 0)) fail('plane on XZ');
const plane3 = spyPrim(() => efx.makePlane({ size: 4, segments: 3 }));
if (plane3.positions.length !== 16 * 3) fail('plane 3x3 grid'); // (3+1)^2
const np = plane3.positions.length / 3;
let maxY = 0, minZ = 1e9, maxZ = -1e9;
for (let i = 0; i < np; i++) {
    maxY = Math.max(maxY, plane3.positions[i * 3 + 1]);
    minZ = Math.min(minZ, plane3.positions[i * 3 + 2]);
    maxZ = Math.max(maxZ, plane3.positions[i * 3 + 2]);
}
if (!near(maxY, 0)) fail('plane flat');
if (!near(minZ, -2) || !near(maxZ, 2)) fail('plane size 4 extent');

// sphere: (segments+1)*segments verts; radius-2 points on the sphere
const sphere = spyPrim(() => efx.makeSphere());
if (sphere.positions.length !== (16 + 1) * 16 * 3) fail('sphere 16 bands');
const sphere24 = spyPrim(() => efx.makeSphere({ radius: 2, segments: 24 }));
if (sphere24.positions.length !== (24 + 1) * 24 * 3) fail('sphere 24 bands');
// every vertex lies on the radius-2 sphere and equals its normal
for (let i = 0; i < sphere24.positions.length; i += 3) {
    const x = sphere24.positions[i], y = sphere24.positions[i + 1], z = sphere24.positions[i + 2];
    if (!near(Math.sqrt(x * x + y * y + z * z), 2)) fail('sphere radius');
    if (!near(x / 2, sphere24.normals[i]) || !near(y / 2, sphere24.normals[i + 1]) ||
        !near(z / 2, sphere24.normals[i + 2])) fail('sphere normals');
}

// primitives draw like any MeshData
efx.drawMesh({ mesh: efx.createMesh(efx.makeCube({ size: 2 })) });
efx.drawMesh({ mesh: efx.createMesh(efx.makePlane({ size: 4, segments: 3 })) });
efx.drawMesh({ mesh: efx.createMesh(efx.makeSphere({ radius: 2, segments: 24 })) });

function expectThrow(name, kind, fn) {
    try { fn(); efx.log('FAIL no-throw ' + name); efx.quit(1); }
    catch (e) {
        if (!(e instanceof kind)) {
            efx.log('FAIL kind ' + name + ': ' + e); efx.quit(2);
        }
    }
}
expectThrow('cube-zero', RangeError, () => efx.makeCube({ size: 0 }));
expectThrow('plane-frac', RangeError, () => efx.makePlane({ segments: 1.5 }));
expectThrow('sphere-neg', RangeError, () => efx.makeSphere({ radius: -1 }));
expectThrow('cube-unknown', TypeError, () => efx.makeCube({ radius: 1 }));
expectThrow('plane-seg-type', RangeError, () => efx.makePlane({ segments: 'many' }));

efx.log('s-3d-math-ok');
