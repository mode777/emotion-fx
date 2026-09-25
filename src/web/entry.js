/* eslint-disable */
// Native-JS runtime glue (f2b, ADR 0022): loads <root>/main.js with the
// host JS engine, exposes the same `efx` namespace contract as the desktop
// quickjs binding, and mirrors the desktop error/exit-code contract.
// Only hoisted function declarations live at top level: on synchronous
// (Node) builds the postRun boot fires before this file's statements run.

function __efxState() {
    var st = globalThis['__efx_state'];
    if (!st) {
        st = { api: null, quitSentinel: null, updateHooks: [], renderHooks: [], started: false };
        globalThis['__efx_state'] = st;
    }
    return st;
}

function __efxExitCode() {
    return Module['_efx_bridge_exit_code']();
}

function __efxSyncExit() {
    Module['efxExitCode'] = __efxExitCode();
}

function __efxMarkEnded() {
    Module['efxRunEnded'] = true;
}

function __efxCStr(v) {
    try {
        return String(v);
    } catch (e) {
        return null;
    }
}

function __efxReportError(e) {
    var msg = (e && typeof e.message === 'string') ? e.message : null;
    var printed = null;
    if (msg !== null) {
        printed = msg;
    } else {
        printed = __efxCStr(e);
    }
    console.error('uncaught exception: ' + (printed === null ? '<unprintable throw value>' : printed));
    if (e && e.stack) {
        console.error(String(e.stack));
    }
}

function __efxNumber(v, typeMsg) {
    try {
        return Number(v);
    } catch (e) {
        throw new TypeError(typeMsg);
    }
}

function __efxFinite(v, typeMsg) {
    var d = __efxNumber(v, typeMsg);
    if (!isFinite(d)) {
        throw new TypeError(typeMsg);
    }
    return d;
}

function __efxFloatArray(v, n) {
    var i, out;
    if (v instanceof Uint8Array) {
        if (v.length !== n) {
            throw new RangeError('wrong buffer length');
        }
        out = new Array(n);
        for (i = 0; i < n; i++) {
            out[i] = v[i];
        }
        return out;
    }
    if (Array.isArray(v)) {
        if (v.length !== n) {
            throw new RangeError('wrong array length');
        }
        out = new Array(n);
        for (i = 0; i < n; i++) {
            var d;
            try {
                d = Number(v[i]);
            } catch (e) {
                throw new RangeError('array elements must be finite numbers');
            }
            if (!isFinite(d)) {
                throw new RangeError('array elements must be finite numbers');
            }
            out[i] = d;
        }
        return out;
    }
    throw new TypeError('expected an array');
}

function __efxIsObject(v) {
    return v !== null && (typeof v === 'object' || typeof v === 'function');
}

function __efxEnsureApi() {
    var st = __efxState();
    if (st.api) {
        return st;
    }
    var bridge = Module;

    st.dispatch = function (which, dt) {
        var hooks = which ? st.updateHooks : st.renderHooks;
        for (var i = 0; i < hooks.length; i++) {
            var entry = hooks[i];
            if (!entry.active) {
                continue;
            }
            try {
                if (which) {
                    entry.fn(dt);
                } else {
                    entry.fn();
                }
            } catch (e) {
                if (st.quitSentinel !== null && e === st.quitSentinel) {
                    return 1;
                }
                bridge['_efx_bridge_set_error']();
                __efxReportError(e);
                __efxSyncExit();
                return 2;
            }
        }
        return 0;
    };
    globalThis['__efxDispatchHook'] = st.dispatch;

    // explicit hook registration (ADR 0016): entries are marked inactive on
    // unsubscribe instead of spliced, so a hook may unsubscribe itself while
    // it is running (desktop parity, design D1)
    function makeRegister(which) {
        return function (fn) {
            if (typeof fn !== 'function') {
                throw new TypeError('hook must be a function');
            }
            var entry = { fn: fn, active: true };
            (which ? st.updateHooks : st.renderHooks).push(entry);
            return function () {
                entry.active = false;
            };
        };
    }

    function EfxImageData(id) {
        this.__id = id;
        this.__alive = true;
    }
    EfxImageData.prototype.destroy = function () {
        if (!(this instanceof EfxImageData)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_imagedata_destroy'](this.__id);
    };

    function EfxTexture(handle, permanent) {
        this.__handle = handle;
        this.__alive = true;
        this.__permanent = !!permanent;
    }
    EfxTexture.prototype.destroy = function () {
        if (!(this instanceof EfxTexture)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        if (this.__permanent) {
            throw new TypeError('cannot destroy an engine-owned texture');
        }
        this.__alive = false;
        bridge['_efx_bridge_texture_destroy'](this.__handle);
    };

    /* read-only query properties, resolved through the render layer's
       texture registry at read time (parity with the desktop binding) */
    Object.defineProperty(EfxTexture.prototype, 'width', {
        get: function () {
            if (!(this instanceof EfxTexture)) {
                throw new TypeError('expected a Texture');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_texture_width'](this.__handle);
        },
    });
    Object.defineProperty(EfxTexture.prototype, 'height', {
        get: function () {
            if (!(this instanceof EfxTexture)) {
                throw new TypeError('expected a Texture');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_texture_height'](this.__handle);
        },
    });

    function EfxMeshData(id) {
        this.__id = id;
        this.__alive = true;
    }
    EfxMeshData.prototype.destroy = function () {
        if (!(this instanceof EfxMeshData)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_meshdata_destroy'](this.__id);
    };
    Object.defineProperty(EfxMeshData.prototype, 'surfaceCount', {
        get: function () {
            if (!(this instanceof EfxMeshData)) {
                throw new TypeError('expected a MeshData');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_meshdata_surface_count'](this.__id);
        },
    });

    function EfxMesh(handle) {
        this.__handle = handle;
        this.__alive = true;
    }
    EfxMesh.prototype.destroy = function () {
        if (!(this instanceof EfxMesh)) {
            throw new TypeError('not a resource object');
        }
        if (!this.__alive) {
            return;
        }
        this.__alive = false;
        bridge['_efx_bridge_mesh_destroy'](this.__handle);
    };
    Object.defineProperty(EfxMesh.prototype, 'surfaceCount', {
        get: function () {
            if (!(this instanceof EfxMesh)) {
                throw new TypeError('expected a Mesh');
            }
            if (!this.__alive) {
                throw new TypeError('using a destroyed resource');
            }
            return bridge['_efx_bridge_mesh_surface_count'](this.__handle);
        },
    });

    function liveMeshData(v) {
        if (!(v instanceof EfxMeshData)) {
            throw new TypeError('expected a MeshData');
        }
        if (!v.__alive) {
            throw new TypeError('using a destroyed resource');
        }
        return v;
    }

    function liveMesh(v) {
        if (!(v instanceof EfxMesh)) {
            throw new TypeError('expected a Mesh');
        }
        if (!v.__alive) {
            throw new TypeError('using a destroyed resource');
        }
        return v;
    }

    /* flat number array (JS array or typed array) -> Float32Array copy;
       element rules mirror the desktop binding: non-number TypeError,
       non-finite RangeError */
    function __efxFloat32Array(v, what) {
        if (!Array.isArray(v) && !ArrayBuffer.isView(v)) {
            throw new TypeError(what + ' must be an array');
        }
        var n = v.length;
        var out = new Float32Array(n);
        for (var i = 0; i < n; i++) {
            var d = v[i];
            if (typeof d !== 'number') {
                throw new TypeError('array elements must be numbers');
            }
            if (!isFinite(d)) {
                throw new RangeError('array elements must be finite numbers');
            }
            out[i] = d;
        }
        return out;
    }

    function __efxUint32Array(v) {
        if (!Array.isArray(v) && !ArrayBuffer.isView(v)) {
            throw new TypeError('indices must be an array');
        }
        var n = v.length;
        var out = new Uint32Array(n);
        for (var i = 0; i < n; i++) {
            var d = v[i];
            if (typeof d !== 'number') {
                throw new TypeError('indices must be numbers');
            }
            if (!isFinite(d) || d < 0 || d > 4294967295 || d !== Math.floor(d)) {
                throw new RangeError('indices must be integers in [0, 2^32-1]');
            }
            out[i] = d;
        }
        return out;
    }

    function mallocCopyF32(arr) {
        var ptr = bridge['_malloc'](arr.length * 4);
        HEAPF32.set(arr, ptr >> 2);
        return ptr;
    }

    function mallocCopyU32(arr) {
        var ptr = bridge['_malloc'](arr.length * 4);
        HEAPU32.set(arr, ptr >> 2);
        return ptr;
    }

    /* persistent scratch for per-draw uniforms (drawMesh is a hot path):
       16 floats transform + 4 floats color, allocated once */
    var drawScratch = 0;
    function drawScratchPtr() {
        if (!drawScratch) {
            drawScratch = bridge['_malloc'](20 * 4);
        }
        return drawScratch;
    }

    function liveImageData(v) {
        if (!(v instanceof EfxImageData)) {
            throw new TypeError('expected an ImageData');
        }
        if (!v.__alive) {
            throw new TypeError('using a destroyed resource');
        }
        return v;
    }

    function liveTexture(v) {
        if (!(v instanceof EfxTexture)) {
            throw new TypeError('expected a Texture');
        }
        if (!v.__alive) {
            throw new TypeError('using a destroyed resource');
        }
        return v;
    }

    var api = {
        log: function (msg) {
            var s = null;
            if (arguments.length > 0) {
                s = __efxCStr(msg);
            }
            console.log(s === null ? '' : s);
        },
        quit: function (code) {
            var c = 0;
            if (arguments.length > 0) {
                try {
                    c = Number(code) | 0;
                } catch (e) {
                    c = 0;
                }
            }
            bridge['_efx_bridge_quit'](c);
            throw __efxState().quitSentinel;
        },
        args: function () {
            var n = bridge['_efx_bridge_arg_count']();
            var out = new Array(n);
            for (var i = 0; i < n; i++) {
                out[i] = UTF8ToString(bridge['_efx_bridge_arg'](i));
            }
            return out;
        },
        registerUpdateHook: makeRegister(1),
        registerRenderHook: makeRegister(0),
        setClearColor: function (color) {
            if (arguments.length < 1) {
                throw new TypeError('setClearColor requires a [r,g,b,a] array');
            }
            var c = __efxFloatArray(color, 4);
            bridge['_efx_bridge_set_clear_color'](c[0], c[1], c[2], c[3]);
        },
        setCamera2D: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('setCamera2D requires an options object');
            }
            var frameW = 0, frameH = 0, x = NaN, y = NaN, zoom = 1, rotation = 0;
            var frame = opts['frame'];
            if (frame !== undefined) {
                var f = __efxFloatArray(frame, 2);
                if (!(f[0] > 0 && f[1] > 0)) {
                    throw new RangeError('frame must be positive');
                }
                frameW = f[0];
                frameH = f[1];
                if (isNaN(x)) {
                    x = frameW * 0.5;
                }
                if (isNaN(y)) {
                    y = frameH * 0.5;
                }
            }
            var xv = opts['x'];
            if (xv !== undefined) {
                x = __efxFinite(xv, 'camera fields must be finite numbers');
            }
            var yv = opts['y'];
            if (yv !== undefined) {
                y = __efxFinite(yv, 'camera fields must be finite numbers');
            }
            var zv = opts['zoom'];
            if (zv !== undefined) {
                zoom = __efxFinite(zv, 'camera fields must be finite numbers');
            }
            var rv = opts['rotation'];
            if (rv !== undefined) {
                rotation = __efxFinite(rv, 'camera fields must be finite numbers');
            }
            if (!(zoom > 0)) {
                throw new RangeError('zoom must be > 0');
            }
            bridge['_efx_bridge_set_camera'](frameW, frameH, x, y, zoom, rotation);
        },
        createImageData: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('createImageData requires an options object');
            }
            var w, h, bad = false;
            try {
                w = Number(opts['width']) | 0;
                h = Number(opts['height']) | 0;
            } catch (e) {
                bad = true;
            }
            if (bad || w <= 0 || h <= 0) {
                throw new RangeError('width and height must be positive');
            }
            var n = w * h * 4;
            if (n > 0x7fffffff) {
                throw new RangeError('image too large');
            }
            var pixels = opts['pixels'];
            if (pixels === undefined) {
                throw new TypeError('createImageData requires pixels');
            }
            var ptr = bridge['_efx_bridge_imagedata_alloc'](n);
            if (!ptr) {
                throw new Error('out of memory');
            }
            try {
                if (Array.isArray(pixels)) {
                    for (var i = 0; i < n; i++) {
                        var d;
                        try {
                            d = Number(pixels[i]);
                        } catch (e) {
                            throw new RangeError('pixel bytes must be integers 0..255');
                        }
                        if (d < 0 || d > 255 || d !== (d | 0)) {
                            throw new RangeError('pixel bytes must be integers 0..255');
                        }
                        HEAPU8[ptr + i] = d;
                    }
                } else if (pixels instanceof Uint8Array) {
                    if (pixels.length !== n) {
                        throw new RangeError('pixels length must be width*height*4');
                    }
                    HEAPU8.set(pixels, ptr);
                } else {
                    throw new TypeError('pixels must be an array or typed array');
                }
            } catch (e) {
                bridge['_efx_bridge_mem_free'](ptr);
                throw e;
            }
            var fmt = opts['format'];
            if (fmt !== undefined) {
                var fs = __efxCStr(fmt);
                if (fs === null || fs !== 'rgba8') {
                    bridge['_efx_bridge_mem_free'](ptr);
                    throw new RangeError("unsupported image format (only 'rgba8')");
                }
            }
            var known = { width: 1, height: 1, pixels: 1, format: 1 };
            var names = Object.getOwnPropertyNames(opts);
            var unknown = null;
            for (var k = 0; k < names.length; k++) {
                if (!known[names[k]]) {
                    unknown = names[k];
                }
            }
            if (unknown !== null) {
                bridge['_efx_bridge_mem_free'](ptr);
                throw new TypeError("unknown option '" + unknown + "'");
            }
            var id = bridge['_efx_bridge_imagedata_commit'](w, h, ptr);
            if (!id) {
                bridge['_efx_bridge_mem_free'](ptr);
                throw new Error('out of memory');
            }
            return new EfxImageData(id);
        },
        createTexture: function (imageData) {
            if (arguments.length < 1) {
                throw new TypeError('createTexture requires an ImageData');
            }
            var d = liveImageData(imageData);
            var handle = bridge['_efx_bridge_texture_create'](d.__id);
            if (!handle) {
                throw new Error('texture upload failed (no GPU context?)');
            }
            return new EfxTexture(handle, false);
        },
        drawQuad: function (x, y, texture, opts) {
            if (arguments.length < 3) {
                throw new TypeError('drawQuad requires (x, y, texture, opts?)');
            }
            var fx = __efxNumber(x, 'x and y must be numbers');
            var fy = __efxNumber(y, 'x and y must be numbers');
            if (!isFinite(fx) || !isFinite(fy)) {
                throw new RangeError('x and y must be finite');
            }
            var tex = liveTexture(texture);
            var color = [1, 1, 1, 1];
            var rotation = 0, scale = 1;
            var src = [0, 0, 0, 0];
            var hasSrc = false;
            var size = [0, 0];
            var hasSize = false;
            var origin = [0, 0];
            var hasOrigin = false;
            if (arguments.length >= 4 && opts !== undefined) {
                if (!__efxIsObject(opts)) {
                    throw new TypeError('opts must be an object');
                }
                var known = { color: 1, rotation: 1, scale: 1, sourceRect: 1, size: 1, origin: 1 };
                var names = Object.getOwnPropertyNames(opts);
                for (var i = 0; i < names.length; i++) {
                    if (!known[names[i]]) {
                        throw new TypeError('unknown drawQuad option');
                    }
                }
                var cv = opts['color'];
                if (cv !== undefined) {
                    color = __efxFloatArray(cv, 4);
                }
                var rv = opts['rotation'];
                if (rv !== undefined) {
                    rotation = __efxFinite(rv, 'rotation must be a finite number');
                }
                var sv = opts['scale'];
                if (sv !== undefined) {
                    scale = __efxFinite(sv, 'scale must be a finite number');
                    if (scale <= 0) {
                        throw new RangeError('scale must be > 0');
                    }
                }
                var zv = opts['size'];
                if (zv !== undefined) {
                    size = __efxFloatArray(zv, 2);
                    if (size[0] <= 0 || size[1] <= 0) {
                        throw new RangeError('size entries must be > 0');
                    }
                    hasSize = true;
                }
                var ov = opts['origin'];
                if (ov !== undefined) {
                    origin = __efxFloatArray(ov, 2);
                    hasOrigin = true;
                }
                var srcv = opts['sourceRect'];
                if (srcv !== undefined) {
                    if (!__efxIsObject(srcv)) {
                        throw new TypeError('sourceRect must be an object');
                    }
                    var skeys = ['x', 'y', 'w', 'h'];
                    for (var j = 0; j < 4; j++) {
                        src[j] = __efxFinite(srcv[skeys[j]], 'sourceRect fields must be finite numbers');
                    }
                    if (src[2] <= 0 || src[3] <= 0) {
                        throw new RangeError('sourceRect extent must be > 0');
                    }
                    var tw = bridge['_efx_bridge_texture_width'](tex.__handle);
                    var th = bridge['_efx_bridge_texture_height'](tex.__handle);
                    if (src[0] < 0 || src[1] < 0 ||
                        src[0] + src[2] > tw || src[1] + src[3] > th) {
                        throw new RangeError('sourceRect outside texture bounds');
                    }
                    hasSrc = true;
                }
            }
            var fw, fh;
            if (hasSize) {
                fw = size[0];
                fh = size[1];
            } else if (hasSrc) {
                fw = src[2];
                fh = src[3];
            } else {
                fw = bridge['_efx_bridge_texture_width'](tex.__handle);
                fh = bridge['_efx_bridge_texture_height'](tex.__handle);
            }
            var ox = hasOrigin ? origin[0] : fw * 0.5;
            var oy = hasOrigin ? origin[1] : fh * 0.5;
            var rc = bridge['_efx_bridge_draw_quad'](tex.__handle, fx, fy, fw, fh,
                color[0], color[1], color[2], color[3], rotation, scale,
                src[0], src[1], src[2], src[3], hasSrc ? 1 : 0, ox, oy);
            if (rc === 1) {
                throw new RangeError('display list budget exceeded');
            }
            if (rc === 4) {
                throw new Error('no render surface (draw calls need a window)');
            }
            if (rc !== 0) {
                throw new Error('drawQuad failed');
            }
        },
        setBlendMode: function (mode) {
            if (arguments.length < 1) {
                throw new TypeError('setBlendMode requires a mode string');
            }
            var s = __efxCStr(mode);
            if (s === null) {
                throw new TypeError('setBlendMode requires a mode string');
            }
            var m;
            if (s === 'alpha') {
                m = 0;
            } else if (s === 'additive') {
                m = 1;
            } else if (s === 'subtractive') {
                m = 2;
            } else {
                throw new TypeError('unknown blend mode');
            }
            bridge['_efx_bridge_set_blend'](m);
        },
        setCamera3D: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('setCamera3D requires an options object');
            }
            var known = { pos: 1, target: 1, fov: 1, near: 1, far: 1 };
            var names = Object.getOwnPropertyNames(opts);
            for (var i = 0; i < names.length; i++) {
                if (!known[names[i]]) {
                    throw new TypeError("unknown setCamera3D option '" + names[i] + "'");
                }
            }
            var pos = opts['pos'];
            var target = opts['target'];
            if (pos === undefined || target === undefined) {
                throw new TypeError('setCamera3D requires pos and target');
            }
            var p = __efxFloat32Array(pos, 'pos');
            var t = __efxFloat32Array(target, 'target');
            if (p.length !== 3 || t.length !== 3) {
                throw new RangeError('pos and target must hold 3 numbers');
            }
            var fov = opts['fov'];
            if (fov === undefined) {
                throw new TypeError('setCamera3D requires fov');
            }
            if (typeof fov !== 'number') {
                throw new TypeError('fov must be a number');
            }
            if (!isFinite(fov)) {
                throw new RangeError('fov must be finite');
            }
            var nearZ = 0.1, farZ = 100;
            var nv = opts['near'];
            if (nv !== undefined) {
                if (typeof nv !== 'number') {
                    throw new TypeError('near and far must be numbers');
                }
                if (!isFinite(nv)) {
                    throw new RangeError('near and far must be finite');
                }
                nearZ = nv;
            }
            var fv = opts['far'];
            if (fv !== undefined) {
                if (typeof fv !== 'number') {
                    throw new TypeError('near and far must be numbers');
                }
                if (!isFinite(fv)) {
                    throw new RangeError('near and far must be finite');
                }
                farZ = fv;
            }
            bridge['_efx_bridge_set_camera3d'](p[0], p[1], p[2],
                t[0], t[1], t[2], fov, nearZ, farZ);
        },
        createMeshData: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('createMeshData requires an options object');
            }
            var bagKnown = { surfaces: 1, positions: 1, normals: 1,
                uvs: 1, colors: 1, indices: 1 };
            var bagNames = Object.getOwnPropertyNames(opts);
            for (var bi = 0; bi < bagNames.length; bi++) {
                if (!bagKnown[bagNames[bi]]) {
                    throw new TypeError("unknown createMeshData option '" + bagNames[bi] + "'");
                }
            }
            var surfaces = opts['surfaces'];
            var shorthand = opts['positions'] !== undefined;
            if (surfaces !== undefined && shorthand) {
                throw new TypeError('pass either surfaces or single-surface fields');
            }
            if (surfaces === undefined && !shorthand) {
                throw new TypeError('createMeshData requires surfaces');
            }
            var list;
            if (surfaces !== undefined) {
                if (!Array.isArray(surfaces)) {
                    throw new TypeError('surfaces must be an array');
                }
                if (surfaces.length < 1 || surfaces.length > 16) {
                    throw new RangeError('surfaces must hold 1..16 entries');
                }
                list = surfaces;
            } else {
                list = [opts];
            }
            var surfKnown = { positions: 1, normals: 1, uvs: 1,
                colors: 1, indices: 1 };
            var id = bridge['_efx_bridge_meshdata_create'](list.length);
            if (!id) {
                throw new Error('out of memory');
            }
            for (var i = 0; i < list.length; i++) {
                var sv = list[i];
                var bad = null;
                if (!__efxIsObject(sv)) {
                    bridge['_efx_bridge_meshdata_destroy'](id);
                    throw new TypeError('surfaces must be objects');
                }
                var names = Object.getOwnPropertyNames(sv);
                for (var k = 0; k < names.length; k++) {
                    if (!surfKnown[names[k]]) {
                        bad = "unknown surface option '" + names[k] + "'";
                    }
                }
                if (bad !== null) {
                    bridge['_efx_bridge_meshdata_destroy'](id);
                    throw new TypeError(bad);
                }
                if (sv['positions'] === undefined) {
                    bridge['_efx_bridge_meshdata_destroy'](id);
                    throw new TypeError('surface requires positions');
                }
                var pos = __efxFloat32Array(sv['positions'], 'positions');
                var nrm = sv['normals'] !== undefined
                    ? __efxFloat32Array(sv['normals'], 'normals') : new Float32Array(0);
                var uvs = sv['uvs'] !== undefined
                    ? __efxFloat32Array(sv['uvs'], 'uvs') : new Float32Array(0);
                var cols = sv['colors'] !== undefined
                    ? __efxFloat32Array(sv['colors'], 'colors') : new Float32Array(0);
                var idx = sv['indices'] !== undefined
                    ? __efxUint32Array(sv['indices']) : new Uint32Array(0);
                var pPtr = mallocCopyF32(pos);
                var nPtr = mallocCopyF32(nrm);
                var uPtr = mallocCopyF32(uvs);
                var cPtr = mallocCopyF32(cols);
                var iPtr = mallocCopyU32(idx);
                var rc = bridge['_efx_bridge_meshdata_surface'](id, i,
                    pPtr, pos.length, nPtr, nrm.length, uPtr, uvs.length,
                    cPtr, cols.length, iPtr, idx.length);
                bridge['_efx_bridge_mem_free'](pPtr);
                bridge['_efx_bridge_mem_free'](nPtr);
                bridge['_efx_bridge_mem_free'](uPtr);
                bridge['_efx_bridge_mem_free'](cPtr);
                bridge['_efx_bridge_mem_free'](iPtr);
                if (rc !== 0) {
                    bridge['_efx_bridge_meshdata_destroy'](id);
                    throw new RangeError('invalid mesh data');
                }
            }
            rc = bridge['_efx_bridge_meshdata_commit'](id);
            if (rc !== 0) {
                bridge['_efx_bridge_meshdata_destroy'](id);
                throw new RangeError('invalid mesh data');
            }
            return new EfxMeshData(id);
        },
        createMesh: function (meshData) {
            if (arguments.length < 1) {
                throw new TypeError('createMesh requires a MeshData');
            }
            var md = liveMeshData(meshData);
            var handle = bridge['_efx_bridge_mesh_create'](md.__id);
            if (!handle) {
                throw new Error('mesh upload failed (no GPU context?)');
            }
            return new EfxMesh(handle);
        },
        drawMesh: function (opts) {
            if (arguments.length < 1 || !__efxIsObject(opts)) {
                throw new TypeError('drawMesh requires an options object');
            }
            var known = { mesh: 1, transform: 1, color: 1 };
            var names = Object.getOwnPropertyNames(opts);
            for (var i = 0; i < names.length; i++) {
                if (!known[names[i]]) {
                    throw new TypeError("unknown drawMesh option '" + names[i] + "'");
                }
            }
            var mesh = opts['mesh'];
            if (mesh === undefined) {
                throw new TypeError('drawMesh requires a mesh');
            }
            var m = liveMesh(mesh);
            var transform = null, color = null;
            var tv = opts['transform'];
            if (tv !== undefined) {
                transform = __efxFloat32Array(tv, 'transform');
                if (transform.length !== 16) {
                    throw new RangeError('transform must hold 16 numbers');
                }
            }
            var cv = opts['color'];
            if (cv !== undefined) {
                color = __efxFloat32Array(cv, 'color');
                if (color.length !== 4) {
                    throw new RangeError('color must hold 4 numbers');
                }
            }
            var tPtr = 0, cPtr = 0;
            if (transform !== null || color !== null) {
                var base = drawScratchPtr();
                if (transform !== null) {
                    tPtr = base;
                    HEAPF32.set(transform, tPtr >> 2);
                }
                if (color !== null) {
                    cPtr = base + 16 * 4;
                    HEAPF32.set(color, cPtr >> 2);
                }
            }
            var rc = bridge['_efx_bridge_draw_mesh'](m.__handle, tPtr, cPtr);
            if (rc === 1) {
                throw new RangeError('display list budget exceeded');
            }
            if (rc === 2) {
                throw new TypeError('expected a live Mesh');
            }
            if (rc !== 0) {
                throw new Error('drawMesh failed');
            }
        },
    };

    var whiteTex = null;
    Object.defineProperty(api, 'whiteTexture', {
        get: function () {
            if (!whiteTex) {
                var handle = bridge['_efx_bridge_white_texture']();
                if (!handle) {
                    throw new Error('white texture unavailable');
                }
                whiteTex = new EfxTexture(handle, true);
            }
            return whiteTex;
        },
    });

    /* engine-bundled pure-JS layer (F3 math + primitives): the same
       embedded source the desktop quickjs runtime evaluates (ADR 0022) */
    var preludeSrc = UTF8ToString(bridge['_efx_bridge_js_prelude']());
    new Function('efx', preludeSrc)(api);

    globalThis['efx'] = api;
    st.api = api;
    st.quitSentinel = new Object();
    return st;
}

function __efxFail(msg) {
    console.error(msg);
    Module['_efx_bridge_fail']();
    __efxSyncExit();
    __efxMarkEnded();
    __efxNodeExit();
}

function __efxNodeExit() {
    try {
        if (typeof process !== 'undefined' && process.exitCode !== undefined) {
            process.exitCode = __efxExitCode();
        }
    } catch (e) {}
}

function __efxBoot() {
    var st = __efxState();
    if (st.started) {
        return;
    }
    st.started = true;
    __efxEnsureApi();
    __efxSyncExit();
    var root = UTF8ToString(Module['_efx_web_root']());
    var isDir = false;
    try {
        isDir = FS.isDir(FS.stat(root).mode);
    } catch (e) {
        isDir = false;
    }
    if (!isDir) {
        __efxFail('player: resource root is not a directory: ' + root);
        return;
    }
    var code = null;
    try {
        code = FS.readFile(root + '/main.js', { encoding: 'utf8' });
    } catch (e) {
        code = null;
    }
    if (code === null) {
        __efxFail('player: no main.js in resource root: ' + root);
        return;
    }
    var hostGlobals = ['window', 'document', 'require', 'process', 'fetch',
        'XMLHttpRequest', 'module', 'exports', 'Buffer', 'global'];
    var deny = {};
    for (var gi = 0; gi < hostGlobals.length; gi++) {
        deny[hostGlobals[gi]] = 1;
    }
    var shadowGlobal = new Proxy(globalThis, {
        has: function (t, k) {
            return !deny[k] && (k in t);
        },
        get: function (t, k) {
            if (k === 'globalThis') {
                return shadowGlobal;
            }
            if (deny[k]) {
                return undefined;
            }
            return t[k];
        },
        set: function (t, k, v) {
            t[k] = v;
            return true;
        },
    });
    var paramNames = ['efx'].concat(hostGlobals).concat(['globalThis']);
    var epilogue = ';return { u: typeof update === "function" ? update : null,'
        + ' r: typeof render === "function" ? render : null };';
    var hooks;
    try {
        var factory = new Function(paramNames.join(','), code + epilogue);
        var callArgs = [st.api];
        for (var i = 0; i < hostGlobals.length; i++) {
            callArgs.push(undefined);
        }
        callArgs.push(shadowGlobal);
        hooks = factory.apply(null, callArgs);
    } catch (e) {
        if (st.quitSentinel !== null && e === st.quitSentinel) {
            __efxSyncExit();
            __efxMarkEnded();
            __efxNodeExit();
            return;
        }
        Module['_efx_bridge_set_error']();
        __efxReportError(e);
        __efxSyncExit();
        __efxMarkEnded();
        __efxNodeExit();
        return;
    }
    if (hooks && typeof hooks.u === 'function') {
        st.updateHooks.push({ fn: hooks.u, active: true });
    }
    if (hooks && typeof hooks.r === 'function') {
        st.renderHooks.push({ fn: hooks.r, active: true });
    }
    __efxSyncExit();
    var dom = false;
    try {
        dom = typeof document !== 'undefined';
    } catch (e) {
        dom = false;
    }
    if (dom) {
        Module['_efx_web_start_loop']();
        return;
    }
    var maxFrames = 100000;
    try {
        if (typeof process !== 'undefined' && process.env && process.env['EFX_WEB_MAX_FRAMES']) {
            maxFrames = parseInt(process.env['EFX_WEB_MAX_FRAMES'], 10) || maxFrames;
        }
    } catch (e) {}
    var guard = 0;
    while (guard < maxFrames && Module['_efx_bridge_frame']() === 0) {
        guard++;
    }
    __efxSyncExit();
    __efxMarkEnded();
    __efxNodeExit();
}
