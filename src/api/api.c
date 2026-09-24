#include "api/api.h"
#include "runtime/runtime_internal.h"
#include "render/render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct efx_host_state *host_state(JSContext *ctx) {
    return (struct efx_host_state *)JS_GetContextOpaque(ctx);
}

void efx_log(const char *msg) {
    fprintf(stdout, "%s\n", msg ? msg : "");
    fflush(stdout);
}

JSValue efx_js_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    const char *s = NULL;
    if (argc > 0) {
        s = JS_ToCString(ctx, argv[0]);
    }
    fprintf(stdout, "%s\n", s ? s : "");
    if (s) {
        JS_FreeCString(ctx, s);
    }
    fflush(stdout);
    return JS_UNDEFINED;
}

JSValue efx_js_quit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    struct efx_host_state *h = host_state(ctx);
    int32_t code = 0;
    if (argc > 0) {
        JS_ToInt32(ctx, &code, argv[0]);
    }
    h->quit_requested = 1;
    h->quit_code = (int)code;
    return JS_Throw(ctx, JS_DupValue(ctx, h->quit_sentinel));
}

JSValue efx_js_args(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    struct efx_host_state *h = host_state(ctx);
    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < h->arg_count; i++) {
        char idx[16];
        snprintf(idx, sizeof(idx), "%d", i);
        JS_SetPropertyStr(ctx, arr, idx, JS_NewString(ctx, h->args[i]));
    }
    return arr;
}

/* ------------------------------------------------------------ helpers */

static JSValue type_error(JSContext *ctx, const char *msg) {
    return JS_ThrowTypeError(ctx, "%s", msg);
}

static JSValue range_error(JSContext *ctx, const char *msg) {
    return JS_ThrowRangeError(ctx, "%s", msg);
}

static JSValue generic_error(JSContext *ctx, const char *msg) {
    return JS_ThrowInternalError(ctx, "%s", msg);
}

/* --------------------------------------------- F1 lifecycle hooks */

/* unsubscribe closure: magic selects the list (0 = update, 1 = render),
   func_data[0] carries the stable entry index (design D1/D2) */
static JSValue efx_js_unsubscribe(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv, int magic,
                                  JSValue *func_data) {
    (void)this_val;
    (void)argc;
    (void)argv;
    struct efx_host_state *h = host_state(ctx);
    struct efx_hook_list *list = magic ? &h->render_hooks : &h->update_hooks;
    int32_t idx = -1;
    JS_ToInt32(ctx, &idx, func_data[0]);
    if (idx >= 0 && idx < list->count) {
        list->entries[idx].active = 0; /* idempotent: repeated calls are no-ops */
    }
    return JS_UNDEFINED;
}

static JSValue register_hook(JSContext *ctx, JSValueConst fn, int is_render) {
    if (!JS_IsFunction(ctx, fn)) {
        return type_error(ctx, "hook must be a function");
    }
    struct efx_host_state *h = host_state(ctx);
    struct efx_hook_list *list = is_render ? &h->render_hooks : &h->update_hooks;
    int idx = efx_hooks_append(ctx, list, fn);
    if (idx < 0) {
        return generic_error(ctx, "out of memory");
    }
    JSValue data = JS_NewInt32(ctx, idx);
    JSValue unsub = JS_NewCFunctionData(ctx, efx_js_unsubscribe, 0,
                                        is_render ? 1 : 0, 1, &data);
    JS_FreeValue(ctx, data);
    return unsub;
}

JSValue efx_js_registerUpdateHook(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "registerUpdateHook requires a function");
    }
    return register_hook(ctx, argv[0], 0);
}

JSValue efx_js_registerRenderHook(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "registerRenderHook requires a function");
    }
    return register_hook(ctx, argv[0], 1);
}

/* read a flat array (JS array or typed array) of exactly n floats;
   returns 0 ok, -1 wrong type (TypeError thrown), -2 wrong length or
   non-finite/out-of-range element (RangeError thrown) */
static int get_float_array(JSContext *ctx, JSValueConst v, float *out, int n) {
    uint8_t *bytes = NULL;
    size_t blen = 0;

    if (JS_IsArray(v)) {
        /* fall through to element loop */
    } else if ((bytes = JS_GetUint8Array(ctx, &blen, v)) != NULL) {
        if ((int)blen != n) {
            range_error(ctx, "wrong buffer length");
            return -2;
        }
        for (int i = 0; i < n; i++) {
            out[i] = (float)bytes[i];
        }
        return 0;
    } else {
        type_error(ctx, "expected an array");
        return -1;
    }

    JSValue lenv = JS_GetPropertyStr(ctx, v, "length");
    int32_t len = -1;
    JS_ToInt32(ctx, &len, lenv);
    JS_FreeValue(ctx, lenv);
    if (len != n) {
        range_error(ctx, "wrong array length");
        return -2;
    }
    for (int i = 0; i < n; i++) {
        double d;
        JSValue ev = JS_GetPropertyUint32(ctx, v, (uint32_t)i);
        if (JS_ToFloat64(ctx, &d, ev) < 0 || !isfinite(d)) {
            JS_FreeValue(ctx, ev);
            range_error(ctx, "array elements must be finite numbers");
            return -2;
        }
        JS_FreeValue(ctx, ev);
        out[i] = (float)d;
    }
    return 0;
}

/* ------------------------------------------------- resource classes */

typedef struct {
    uint64_t handle;
    int alive;
    int permanent; /* engine-owned (white texture) */
} efxjs_texture;

typedef struct {
    uint8_t *pixels;
    int w, h;
    int alive;
} efxjs_imagedata;

static JSClassID texture_class_id;
static JSClassID imagedata_class_id;

static void texture_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_texture *t = JS_GetOpaque(val, texture_class_id);
    if (t) {
        if (t->alive && !t->permanent) {
            efx_render_texture_destroy(t->handle);
        }
        free(t);
    }
}

static void imagedata_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt;
    efxjs_imagedata *d = JS_GetOpaque(val, imagedata_class_id);
    if (d) {
        free(d->pixels);
        free(d);
    }
}

static JSValue js_destroy_resource(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)argc;
    (void)argv;
    efxjs_texture *t = JS_GetOpaque2(ctx, this_val, texture_class_id);
    if (t) {
        if (!t->alive) {
            return JS_UNDEFINED; /* destroy() is idempotent */
        }
        if (t->permanent) {
            return type_error(ctx, "cannot destroy an engine-owned texture");
        }
        t->alive = 0;
        efx_render_texture_destroy(t->handle);
        return JS_UNDEFINED;
    }
    efxjs_imagedata *d = JS_GetOpaque2(ctx, this_val, imagedata_class_id);
    if (d) {
        d->alive = 0; /* native bytes released by the GC finalizer */
        return JS_UNDEFINED;
    }
    return type_error(ctx, "not a resource object");
}

static JSClassDef texture_class_def = {
    "Texture",
    .finalizer = texture_finalizer,
};
static JSClassDef imagedata_class_def = {
    "ImageData",
    .finalizer = imagedata_finalizer,
};

/* read-only query properties (Texture.width / Texture.height), resolved
 * through the render layer's texture registry at read time */
static JSValue efx_js_texture_getWidth(JSContext *ctx, JSValueConst this_val) {
    efxjs_texture *t = JS_GetOpaque2(ctx, this_val, texture_class_id);
    if (!t) {
        return type_error(ctx, "expected a Texture");
    }
    if (!t->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    int w = 0, h = 0;
    efx_render_texture_size(t->handle, &w, &h);
    return JS_NewInt32(ctx, w);
}

static JSValue efx_js_texture_getHeight(JSContext *ctx, JSValueConst this_val) {
    efxjs_texture *t = JS_GetOpaque2(ctx, this_val, texture_class_id);
    if (!t) {
        return type_error(ctx, "expected a Texture");
    }
    if (!t->alive) {
        return type_error(ctx, "using a destroyed resource");
    }
    int w = 0, h = 0;
    efx_render_texture_size(t->handle, &w, &h);
    return JS_NewInt32(ctx, h);
}

static const JSCFunctionListEntry texture_proto_funcs[] = {
    JS_CGETSET_DEF("width", efx_js_texture_getWidth, NULL),
    JS_CGETSET_DEF("height", efx_js_texture_getHeight, NULL),
};

int efx_api_init(JSContext *ctx) {
    static int registered;
    if (registered) {
        return 0;
    }
    JSRuntime *rt = JS_GetRuntime(ctx);
    if (JS_NewClassID(rt, &texture_class_id) != texture_class_id ||
        JS_NewClassID(rt, &imagedata_class_id) != imagedata_class_id) {
        return -1;
    }
    if (JS_NewClass(rt, texture_class_id, &texture_class_def) < 0 ||
        JS_NewClass(rt, imagedata_class_id, &imagedata_class_def) < 0) {
        return -1;
    }
    JSValue tex_proto = JS_NewObject(ctx);
    JSValue img_proto = JS_NewObject(ctx);
    JSValue m = JS_NewCFunction(ctx, js_destroy_resource, "destroy", 0);
    JS_SetPropertyStr(ctx, tex_proto, "destroy", JS_DupValue(ctx, m));
    JS_SetPropertyStr(ctx, img_proto, "destroy", m);
    JS_SetPropertyFunctionList(ctx, tex_proto, texture_proto_funcs,
                               (int)(sizeof(texture_proto_funcs) /
                                     sizeof(texture_proto_funcs[0])));
    JS_SetClassProto(ctx, texture_class_id, tex_proto);
    JS_SetClassProto(ctx, imagedata_class_id, img_proto);
    registered = 1;
    return 0;
}

/* -------------------------------------------------------- F2 bindings */

JSValue efx_js_whiteTexture(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    struct efx_host_state *h = host_state(ctx);
    if (!h->has_white_texture) {
        uint64_t handle = efx_render_white_texture();
        if (!handle) {
            return generic_error(ctx, "white texture unavailable");
        }
        efxjs_texture *t = calloc(1, sizeof(efxjs_texture));
        t->handle = handle;
        t->alive = 1;
        t->permanent = 1;
        JSValue obj = JS_NewObjectClass(ctx, texture_class_id);
        JS_SetOpaque(obj, t);
        h->white_texture = obj;
        h->has_white_texture = 1;
    }
    return JS_DupValue(ctx, h->white_texture);
}

JSValue efx_js_setClearColor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "setClearColor requires a [r,g,b,a] array");
    }
    float c[4];
    int rc = get_float_array(ctx, argv[0], c, 4);
    if (rc != 0) {
        return JS_EXCEPTION;
    }
    efx_render_set_clear_color(c);
    return JS_UNDEFINED;
}

JSValue efx_js_setCamera2D(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return type_error(ctx, "setCamera2D requires an options object");
    }
    JSValueConst opts = argv[0];
    efx_camera2d cam;
    memset(&cam, 0, sizeof(cam));
    cam.zoom = 1.0f;
    cam.x = NAN; /* NaN = resolve to frame center at record time */
    cam.y = NAN;

    JSValue frame = JS_GetPropertyStr(ctx, opts, "frame");
    if (!JS_IsUndefined(frame)) {
        float f[2];
        if (get_float_array(ctx, frame, f, 2) != 0) {
            JS_FreeValue(ctx, frame);
            return JS_EXCEPTION;
        }
        if (!(f[0] > 0 && f[1] > 0)) {
            JS_FreeValue(ctx, frame);
            return range_error(ctx, "frame must be positive");
        }
        cam.frame_w = f[0];
        cam.frame_h = f[1];
        if (isnan(cam.x)) {
            cam.x = f[0] * 0.5f;
        }
        if (isnan(cam.y)) {
            cam.y = f[1] * 0.5f;
        }
    }
    JS_FreeValue(ctx, frame);

    static const char *keys[] = {"x", "y", "zoom", "rotation"};
    float *targets[] = {&cam.x, &cam.y, &cam.zoom, &cam.rotation};
    for (int i = 0; i < 4; i++) {
        JSValue v = JS_GetPropertyStr(ctx, opts, keys[i]);
        if (!JS_IsUndefined(v)) {
            double d;
            if (JS_ToFloat64(ctx, &d, v) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, v);
                return type_error(ctx, "camera fields must be finite numbers");
            }
            *targets[i] = (float)d;
        }
        JS_FreeValue(ctx, v);
    }
    if (!(cam.zoom > 0)) {
        return range_error(ctx, "zoom must be > 0");
    }
    efx_render_set_camera(&cam);
    return JS_UNDEFINED;
}

JSValue efx_js_createImageData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return type_error(ctx, "createImageData requires an options object");
    }
    JSValueConst opts = argv[0];
    int32_t w = 0, hgt = 0;
    JSValue wv = JS_GetPropertyStr(ctx, opts, "width");
    JSValue hv = JS_GetPropertyStr(ctx, opts, "height");
    int bad = 0;
    if (JS_ToInt32(ctx, &w, wv) < 0 || JS_ToInt32(ctx, &hgt, hv) < 0) {
        bad = 1;
    }
    JS_FreeValue(ctx, wv);
    JS_FreeValue(ctx, hv);
    if (bad || w <= 0 || hgt <= 0) {
        return range_error(ctx, "width and height must be positive");
    }
    double pw = (double)w * (double)hgt * 4.0;
    if (pw > (double)0x7fffffff) {
        return range_error(ctx, "image too large");
    }

    JSValue pixels = JS_GetPropertyStr(ctx, opts, "pixels");
    if (JS_IsUndefined(pixels)) {
        JS_FreeValue(ctx, pixels);
        return type_error(ctx, "createImageData requires pixels");
    }
    size_t n = (size_t)pw;
    uint8_t *buf = malloc(n);
    if (!buf) {
        JS_FreeValue(ctx, pixels);
        return generic_error(ctx, "out of memory");
    }
    int rc;
    if (JS_IsArray(pixels)) {
        rc = 0;
        for (size_t i = 0; i < n && rc == 0; i++) {
            double d;
            JSValue ev = JS_GetPropertyUint32(ctx, pixels, (uint32_t)i);
            if (JS_ToFloat64(ctx, &d, ev) < 0 || d < 0 || d > 255 || d != (int)d) {
                rc = -2;
            } else {
                buf[i] = (uint8_t)d;
            }
            JS_FreeValue(ctx, ev);
        }
        if (rc != 0) {
            range_error(ctx, "pixel bytes must be integers 0..255");
        }
    } else {
        size_t blen = 0;
        uint8_t *ptr = JS_GetUint8Array(ctx, &blen, pixels);
        if (!ptr) {
            type_error(ctx, "pixels must be an array or typed array");
            rc = -1;
        } else if (blen != n) {
            range_error(ctx, "pixels length must be width*height*4");
            rc = -2;
        } else {
            memcpy(buf, ptr, n);
            rc = 0;
        }
    }
    JS_FreeValue(ctx, pixels);
    if (rc != 0) {
        free(buf);
        return JS_EXCEPTION;
    }

    /* format field: only 'rgba8' (the default) exists in F2 */
    JSValue fmt = JS_GetPropertyStr(ctx, opts, "format");
    int fmt_bad = 0;
    if (!JS_IsUndefined(fmt)) {
        const char *fs = JS_ToCString(ctx, fmt);
        if (!fs || strcmp(fs, "rgba8") != 0) {
            fmt_bad = 1;
        }
        if (fs) {
            JS_FreeCString(ctx, fs);
        }
    }
    JS_FreeValue(ctx, fmt);
    if (fmt_bad) {
        free(buf);
        return range_error(ctx, "unsupported image format (only 'rgba8')");
    }

    /* unknown-field check (typo protection) */
    static const char *known[] = {"width", "height", "pixels", "format"};
    JSPropertyEnum *props = NULL;
    uint32_t nprops = 0;
    if (JS_GetOwnPropertyNames(ctx, &props, &nprops, opts,
                               JS_GPN_STRING_MASK) == 0) {
        int unknown = 0;
        for (uint32_t i = 0; i < nprops; i++) {
            const char *k = JS_AtomToCString(ctx, props[i].atom);
            int ok = 0;
            for (int j = 0; j < 4; j++) {
                if (k && strcmp(k, known[j]) == 0) {
                    ok = 1;
                    break;
                }
            }
            if (!ok) {
                JS_ThrowTypeError(ctx, "unknown option '%s'", k ? k : "?");
                unknown = 1;
            }
            if (k) {
                JS_FreeCString(ctx, k);
            }
            JS_FreeAtom(ctx, props[i].atom);
        }
        js_free(ctx, props);
        if (unknown) {
            free(buf);
            return JS_EXCEPTION;
        }
    }

    efxjs_imagedata *d = calloc(1, sizeof(efxjs_imagedata));
    d->pixels = buf;
    d->w = w;
    d->h = hgt;
    d->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, imagedata_class_id);
    JS_SetOpaque(obj, d);
    return obj;
}

static efxjs_imagedata *get_live_imagedata(JSContext *ctx, JSValueConst v) {
    efxjs_imagedata *d = JS_GetOpaque2(ctx, v, imagedata_class_id);
    if (!d) {
        type_error(ctx, "expected an ImageData");
        return NULL;
    }
    if (!d->alive) {
        type_error(ctx, "using a destroyed resource");
        return NULL;
    }
    return d;
}

static efxjs_texture *get_live_texture(JSContext *ctx, JSValueConst v) {
    efxjs_texture *t = JS_GetOpaque2(ctx, v, texture_class_id);
    if (!t) {
        type_error(ctx, "expected a Texture");
        return NULL;
    }
    if (!t->alive) {
        type_error(ctx, "using a destroyed resource");
        return NULL;
    }
    return t;
}

JSValue efx_js_createTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "createTexture requires an ImageData");
    }
    efxjs_imagedata *d = get_live_imagedata(ctx, argv[0]);
    if (!d) {
        return JS_EXCEPTION;
    }
    uint64_t handle = efx_render_texture_create(d->w, d->h, d->pixels);
    if (!handle) {
        return generic_error(ctx, "texture upload failed (no GPU context?)");
    }
    efxjs_texture *t = calloc(1, sizeof(efxjs_texture));
    t->handle = handle;
    t->alive = 1;
    JSValue obj = JS_NewObjectClass(ctx, texture_class_id);
    JS_SetOpaque(obj, t);
    return obj;
}

JSValue efx_js_drawQuad(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 3) {
        return type_error(ctx, "drawQuad requires (x, y, texture, opts?)");
    }
    double x, y;
    if (JS_ToFloat64(ctx, &x, argv[0]) < 0 || JS_ToFloat64(ctx, &y, argv[1]) < 0) {
        return type_error(ctx, "x and y must be numbers");
    }
    if (!isfinite(x) || !isfinite(y)) {
        return range_error(ctx, "x and y must be finite");
    }
    efxjs_texture *tex = get_live_texture(ctx, argv[2]);
    if (!tex) {
        return JS_EXCEPTION;
    }

    float color[4] = {1, 1, 1, 1};
    float rotation = 0, scale = 1;
    float src[4] = {0, 0, 0, 0};
    int has_src = 0;
    float size[2] = {0, 0};
    int has_size = 0;
    float origin[2] = {0, 0};
    int has_origin = 0;

    if (argc >= 4 && !JS_IsUndefined(argv[3])) {
        if (!JS_IsObject(argv[3])) {
            return type_error(ctx, "opts must be an object");
        }
        JSValueConst opts = argv[3];
        static const char *known[] = {"color", "rotation", "scale", "sourceRect", "size", "origin"};
        JSPropertyEnum *props = NULL;
        uint32_t nprops = 0;
        if (JS_GetOwnPropertyNames(ctx, &props, &nprops, opts,
                                   JS_GPN_STRING_MASK) == 0) {
            for (uint32_t i = 0; i < nprops; i++) {
                const char *k = JS_AtomToCString(ctx, props[i].atom);
                int ok = 0;
                for (int j = 0; j < 6; j++) {
                    if (k && strcmp(k, known[j]) == 0) {
                        ok = 1;
                        break;
                    }
                }
                if (k) {
                    JS_FreeCString(ctx, k);
                }
                JS_FreeAtom(ctx, props[i].atom);
                if (!ok) {
                    for (uint32_t j = i + 1; j < nprops; j++) {
                        JS_FreeAtom(ctx, props[j].atom);
                    }
                    js_free(ctx, props);
                    return JS_ThrowTypeError(ctx, "unknown drawQuad option");
                }
            }
            js_free(ctx, props);
        }

        JSValue cv = JS_GetPropertyStr(ctx, opts, "color");
        if (!JS_IsUndefined(cv)) {
            if (get_float_array(ctx, cv, color, 4) != 0) {
                JS_FreeValue(ctx, cv);
                return JS_EXCEPTION;
            }
        }
        JS_FreeValue(ctx, cv);

        JSValue rv = JS_GetPropertyStr(ctx, opts, "rotation");
        if (!JS_IsUndefined(rv)) {
            double d;
            if (JS_ToFloat64(ctx, &d, rv) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, rv);
                return type_error(ctx, "rotation must be a finite number");
            }
            rotation = (float)d;
        }
        JS_FreeValue(ctx, rv);

        JSValue sv = JS_GetPropertyStr(ctx, opts, "scale");
        if (!JS_IsUndefined(sv)) {
            double d;
            if (JS_ToFloat64(ctx, &d, sv) < 0 || !isfinite(d)) {
                JS_FreeValue(ctx, sv);
                return type_error(ctx, "scale must be a finite number");
            }
            if (d <= 0) {
                JS_FreeValue(ctx, sv);
                return range_error(ctx, "scale must be > 0");
            }
            scale = (float)d;
        }
        JS_FreeValue(ctx, sv);

        JSValue zv = JS_GetPropertyStr(ctx, opts, "size");
        if (!JS_IsUndefined(zv)) {
            if (get_float_array(ctx, zv, size, 2) != 0) {
                JS_FreeValue(ctx, zv);
                return JS_EXCEPTION;
            }
            if (size[0] <= 0 || size[1] <= 0) {
                JS_FreeValue(ctx, zv);
                return range_error(ctx, "size entries must be > 0");
            }
            has_size = 1;
        }
        JS_FreeValue(ctx, zv);

        JSValue ov = JS_GetPropertyStr(ctx, opts, "origin");
        if (!JS_IsUndefined(ov)) {
            if (get_float_array(ctx, ov, origin, 2) != 0) {
                JS_FreeValue(ctx, ov);
                return JS_EXCEPTION;
            }
            has_origin = 1;
        }
        JS_FreeValue(ctx, ov);

        JSValue srcv = JS_GetPropertyStr(ctx, opts, "sourceRect");
        if (!JS_IsUndefined(srcv)) {
            if (!JS_IsObject(srcv)) {
                JS_FreeValue(ctx, srcv);
                return type_error(ctx, "sourceRect must be an object");
            }
            static const char *skeys[] = {"x", "y", "w", "h"};
            for (int i = 0; i < 4; i++) {
                JSValue f = JS_GetPropertyStr(ctx, srcv, skeys[i]);
                double d;
                if (JS_ToFloat64(ctx, &d, f) < 0 || !isfinite(d)) {
                    JS_FreeValue(ctx, f);
                    JS_FreeValue(ctx, srcv);
                    return type_error(ctx, "sourceRect fields must be finite numbers");
                }
                JS_FreeValue(ctx, f);
                src[i] = (float)d;
            }
            JS_FreeValue(ctx, srcv);
            if (src[2] <= 0 || src[3] <= 0) {
                return range_error(ctx, "sourceRect extent must be > 0");
            }
            int tw = 0, th = 0;
            efx_render_texture_size(tex->handle, &tw, &th);
            if (src[0] < 0 || src[1] < 0 ||
                src[0] + src[2] > (float)tw || src[1] + src[3] > (float)th) {
                return range_error(ctx, "sourceRect outside texture bounds");
            }
            has_src = 1;
        }
    }

    /* size derivation: explicit size -> sourceRect extent -> texture pixels */
    float w, h;
    if (has_size) {
        w = size[0];
        h = size[1];
    } else if (has_src) {
        w = src[2];
        h = src[3];
    } else {
        int tw = 0, th = 0;
        efx_render_texture_size(tex->handle, &tw, &th);
        w = (float)tw;
        h = (float)th;
    }
    float origin_x = has_origin ? origin[0] : w * 0.5f;
    float origin_y = has_origin ? origin[1] : h * 0.5f;

    int rc = efx_render_quad((float)x, (float)y, w, h,
                             tex->handle, color, rotation, scale, src, has_src,
                             origin_x, origin_y);
    if (rc == EFX_RENDER_ERR_BUDGET) {
        return range_error(ctx, "display list budget exceeded");
    }
    if (rc == EFX_RENDER_ERR_SINK) {
        return generic_error(ctx, "no render surface (draw calls need a window)");
    }
    if (rc != EFX_RENDER_OK) {
        return generic_error(ctx, "drawQuad failed");
    }
    return JS_UNDEFINED;
}

JSValue efx_js_setBlendMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) {
        return type_error(ctx, "setBlendMode requires a mode string");
    }
    const char *s = JS_ToCString(ctx, argv[0]);
    if (!s) {
        return type_error(ctx, "setBlendMode requires a mode string");
    }
    int mode;
    if (strcmp(s, "alpha") == 0) {
        mode = EFX_BLEND_ALPHA;
    } else if (strcmp(s, "additive") == 0) {
        mode = EFX_BLEND_ADDITIVE;
    } else if (strcmp(s, "subtractive") == 0) {
        mode = EFX_BLEND_SUBTRACTIVE;
    } else {
        JS_FreeCString(ctx, s);
        return type_error(ctx, "unknown blend mode");
    }
    JS_FreeCString(ctx, s);
    efx_render_set_blend(mode);
    return JS_UNDEFINED;
}
