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
