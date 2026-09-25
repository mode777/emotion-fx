/* EmotionFX engine-bundled pure-JS layer (F3).
 *
 * Runs identically on both bindings: desktop quickjs evaluates this against
 * the `efx` namespace at runtime init; the web bridge exports the same
 * source and the page's JS engine evaluates it in entry.js.
 *
 * Rules (js-api two-layer contract): plain ES6 only — no host APIs; plain
 * JS data in/out (ADR 0010); angles in degrees; matrices are flat 16-number
 * column-major arrays; every helper is a pure function (never mutates its
 * arguments).
 */

function __efxM4Mul(a, b) {
    /* column-major: out[c*4+r] = sum_k a[k*4+r] * b[c*4+k]  (a·b) */
    var out = new Array(16);
    for (var c = 0; c < 4; c++) {
        for (var r = 0; r < 4; r++) {
            out[c * 4 + r] = a[r] * b[c * 4] +
                             a[4 + r] * b[c * 4 + 1] +
                             a[8 + r] * b[c * 4 + 2] +
                             a[12 + r] * b[c * 4 + 3];
        }
    }
    return out;
}

function __efxM4Identity() {
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
}

function __efxM4Perspective(fovYDeg, aspect, nearZ, farZ) {
    /* right-handed, GL depth range (-1..1) — matches the engine camera */
    var f = 1 / Math.tan((fovYDeg / 2) * Math.PI / 180);
    var m = new Array(16);
    m[0] = f / aspect; m[1] = 0; m[2] = 0; m[3] = 0;
    m[4] = 0; m[5] = f; m[6] = 0; m[7] = 0;
    m[8] = 0; m[9] = 0; m[10] = (farZ + nearZ) / (nearZ - farZ); m[11] = -1;
    m[12] = 0; m[13] = 0; m[14] = (2 * farZ * nearZ) / (nearZ - farZ); m[15] = 0;
    return m;
}

function __efxM4Ortho(width, height, nearZ, farZ) {
    var m = new Array(16);
    m[0] = 2 / width; m[1] = 0; m[2] = 0; m[3] = 0;
    m[4] = 0; m[5] = 2 / height; m[6] = 0; m[7] = 0;
    m[8] = 0; m[9] = 0; m[10] = 2 / (nearZ - farZ); m[11] = 0;
    m[12] = 0; m[13] = 0; m[14] = (nearZ + farZ) / (nearZ - farZ); m[15] = 1;
    return m;
}

function __efxM4Translate(m, v) {
    var out = m.slice();
    for (var r = 0; r < 4; r++) {
        out[12 + r] = m[12 + r] + m[r] * v[0] + m[4 + r] * v[1] + m[8 + r] * v[2];
    }
    return out;
}

function __efxM4Rotate(m, deg, axis) {
    /* Rodrigues; right-handed CCW about axis (looking down the axis toward
       the origin); composes m·R (rotation applied first) */
    var len = Math.sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (len === 0) {
        return m.slice();
    }
    var x = axis[0] / len, y = axis[1] / len, z = axis[2] / len;
    var rad = deg * Math.PI / 180;
    var c = Math.cos(rad), s = Math.sin(rad), t = 1 - c;
    var r = new Array(16);
    r[0] = t * x * x + c;     r[1] = t * y * x + s * z; r[2] = t * z * x - s * y; r[3] = 0;
    r[4] = t * x * y - s * z; r[5] = t * y * y + c;     r[6] = t * z * y + s * x; r[7] = 0;
    r[8] = t * x * z + s * y; r[9] = t * y * z - s * x; r[10] = t * z * z + c;    r[11] = 0;
    r[12] = 0; r[13] = 0; r[14] = 0; r[15] = 1;
    return __efxM4Mul(m, r);
}

function __efxM4Scale(m, v) {
    var out = m.slice();
    for (var r = 0; r < 4; r++) {
        out[r] = m[r] * v[0];
        out[4 + r] = m[4 + r] * v[1];
        out[8 + r] = m[8 + r] * v[2];
    }
    return out;
}

function __efxV3Add(a, b) { return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]; }
function __efxV3Sub(a, b) { return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]; }
function __efxV3Scale(v, s) { return [v[0] * s, v[1] * s, v[2] * s]; }
function __efxV3Cross(a, b) {
    return [a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]];
}
function __efxV3Dot(a, b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
function __efxV3Normalize(v) {
    var len = Math.sqrt(__efxV3Dot(v, v));
    if (len === 0) {
        return [0, 0, 0];
    }
    return [v[0] / len, v[1] / len, v[2] / len];
}

function __efxQuatIdentity() { return [0, 0, 0, 1]; }

function __efxQuatFromAxisAngle(deg, axis) {
    var len = Math.sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (len === 0) {
        return [0, 0, 0, 1];
    }
    var half = (deg / 2) * Math.PI / 180;
    var s = Math.sin(half) / len;
    return [axis[0] * s, axis[1] * s, axis[2] * s, Math.cos(half)];
}

function __efxQuatMultiply(a, b) {
    return [
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
    ];
}

function __efxQuatToMat4(q) {
    var x = q[0], y = q[1], z = q[2], w = q[3];
    var x2 = x + x, y2 = y + y, z2 = z + z;
    var xx = x * x2, xy = x * y2, xz = x * z2;
    var yy = y * y2, yz = y * z2, zz = z * z2;
    var wx = w * x2, wy = w * y2, wz = w * z2;
    return [
        1 - (yy + zz), xy + wz, xz - wy, 0,
        xy - wz, 1 - (xx + zz), yz + wx, 0,
        xz + wy, yz - wx, 1 - (xx + yy), 0,
        0, 0, 0, 1,
    ];
}

/* option validation shared by the primitives (spec: finite positive
   size/radius -> RangeError; positive integer segments -> RangeError;
   unknown fields -> TypeError; missing object takes all defaults) */
function __efxPrimOpts(opts, keys, floats, ints, defaults) {
    var out = {};
    for (var d = 0; d < keys.length; d++) {
        out[keys[d]] = defaults[d];
    }
    if (opts === undefined || opts === null) {
        return out;
    }
    if (typeof opts !== 'object') {
        throw new TypeError('primitive options must be an object');
    }
    var names = Object.getOwnPropertyNames(opts);
    for (var i = 0; i < names.length; i++) {
        var known = false;
        for (var k = 0; k < keys.length; k++) {
            if (names[i] === keys[k]) {
                known = true;
                break;
            }
        }
        if (!known) {
            throw new TypeError("unknown primitive option '" + names[i] + "'");
        }
    }
    for (var f = 0; f < floats.length; f++) {
        var key = floats[f];
        if (opts[key] !== undefined) {
            var d = Number(opts[key]);
            if (!isFinite(d) || d <= 0) {
                throw new RangeError(key + ' must be > 0');
            }
            out[key] = d;
        }
    }
    for (var n = 0; n < ints.length; n++) {
        var ikey = ints[n];
        if (opts[ikey] !== undefined) {
            var iv = Number(opts[ikey]);
            if (!isFinite(iv) || iv <= 0 || iv !== Math.floor(iv)) {
                throw new RangeError(ikey + ' must be a positive integer');
            }
            out[ikey] = iv;
        }
    }
    return out;
}

/* axis-aligned cube centered on the origin; per-face normals and per-face
   0..1 uvs; outward CCW winding; 24 verts / 36 indices */
function __efxMakeCube(opts) {
    var o = __efxPrimOpts(opts, ['size'], ['size'], [], [1]);
    var h = o.size / 2;
    /* normal, edge u, edge v with cross(u, v) = normal */
    var faces = [
        [[1, 0, 0], [0, 0, -1], [0, 1, 0]],
        [[-1, 0, 0], [0, 0, 1], [0, 1, 0]],
        [[0, 1, 0], [1, 0, 0], [0, 0, -1]],
        [[0, -1, 0], [1, 0, 0], [0, 0, 1]],
        [[0, 0, 1], [1, 0, 0], [0, 1, 0]],
        [[0, 0, -1], [-1, 0, 0], [0, 1, 0]],
    ];
    var positions = [], normals = [], uvs = [], indices = [];
    var cornerUVs = [[0, 0], [1, 0], [1, 1], [0, 1]];
    for (var f = 0; f < 6; f++) {
        var n = faces[f][0], u = faces[f][1], v = faces[f][2];
        var base = f * 4;
        for (var c = 0; c < 4; c++) {
            var su = (c === 1 || c === 2) ? 1 : -1;
            var sv = (c === 2 || c === 3) ? 1 : -1;
            positions.push(
                n[0] * h + u[0] * h * su + v[0] * h * sv,
                n[1] * h + u[1] * h * su + v[1] * h * sv,
                n[2] * h + u[2] * h * su + v[2] * h * sv);
            normals.push(n[0], n[1], n[2]);
            uvs.push(cornerUVs[c][0], cornerUVs[c][1]);
        }
        indices.push(base, base + 1, base + 2, base, base + 2, base + 3);
    }
    return efx.createMeshData({
        positions: positions, normals: normals, uvs: uvs, indices: indices,
    });
}

/* plane in the XZ plane facing +Y, centered; segments x segments quads;
   uv spans 0..1; CCW seen from above */
function __efxMakePlane(opts) {
    var o = __efxPrimOpts(opts, ['size', 'segments'], ['size'], ['segments'], [1, 1]);
    var S = o.segments, h = o.size / 2;
    var step = o.size / S;
    var positions = [], uvs = [], indices = [];
    for (var r = 0; r <= S; r++) {
        for (var c = 0; c <= S; c++) {
            positions.push(-h + c * step, 0, -h + r * step);
            uvs.push(c / S, r / S);
        }
    }
    for (var rr = 0; rr < S; rr++) {
        for (var cc = 0; cc < S; cc++) {
            var a = rr * (S + 1) + cc;
            var b = a + 1;
            var d = a + (S + 1);
            var e = d + 1;
            indices.push(a, d, e, a, e, b);
        }
    }
    return efx.createMeshData({ positions: positions, uvs: uvs, indices: indices });
}

/* UV sphere centered on the origin; segments latitude rings x segments
   longitude slices; normals = normalized positions; equirectangular uv */
function __efxMakeSphere(opts) {
    var o = __efxPrimOpts(opts, ['radius', 'segments'], ['radius'], ['segments'], [1, 16]);
    var S = o.segments, R = o.radius;
    var positions = [], normals = [], uvs = [], indices = [];
    for (var i = 0; i <= S; i++) {
        var vRow = i / S;
        var theta = vRow * Math.PI;
        var sinT = Math.sin(theta), cosT = Math.cos(theta);
        for (var j = 0; j < S; j++) {
            var uCol = j / S;
            var phi = uCol * 2 * Math.PI;
            var x = sinT * Math.cos(phi);
            var y = cosT;
            var z = sinT * Math.sin(phi);
            positions.push(x * R, y * R, z * R);
            normals.push(x, y, z);
            uvs.push(uCol, vRow);
        }
    }
    for (var ii = 0; ii < S; ii++) {
        for (var jj = 0; jj < S; jj++) {
            var a = ii * S + jj;
            var b = ii * S + ((jj + 1) % S);
            var c2 = (ii + 1) * S + jj;
            var d = (ii + 1) * S + ((jj + 1) % S);
            indices.push(a, d, c2, a, b, d);
        }
    }
    return efx.createMeshData({
        positions: positions, normals: normals, uvs: uvs, indices: indices,
    });
}

function __efxPreludeInstall(efx) {
    efx.mat4 = {
        identity: __efxM4Identity,
        perspective: __efxM4Perspective,
        ortho: __efxM4Ortho,
        translate: __efxM4Translate,
        rotate: __efxM4Rotate,
        scale: __efxM4Scale,
        multiply: __efxM4Mul,
    };
    efx.vec3 = {
        add: __efxV3Add,
        sub: __efxV3Sub,
        scale: __efxV3Scale,
        normalize: __efxV3Normalize,
        cross: __efxV3Cross,
        dot: __efxV3Dot,
    };
    efx.quat = {
        identity: __efxQuatIdentity,
        fromAxisAngle: __efxQuatFromAxisAngle,
        multiply: __efxQuatMultiply,
        toMat4: __efxQuatToMat4,
    };
    efx.makeCube = __efxMakeCube;
    efx.makePlane = __efxMakePlane;
    efx.makeSphere = __efxMakeSphere;
}

__efxPreludeInstall(efx);
