#define _POSIX_C_SOURCE 200809L

#include "runtime/runtime.h"
#include "runtime/runtime_internal.h"
#include "api/api.h"
#include "prelude/prelude.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"

struct efx_runtime {
    struct efx_host_state host;
    JSRuntime *js_rt;
    JSContext *ctx;
    int in_error;
    int hooks_sugar_done; /* global update/render registered once after eval */
};

int efx_hooks_append(JSContext *ctx, struct efx_hook_list *list, JSValueConst fn) {
    if (list->count == list->cap) {
        int cap = list->cap ? list->cap * 2 : 4;
        struct efx_hook_entry *grown =
            realloc(list->entries, (size_t)cap * sizeof(*grown));
        if (!grown) {
            return -1;
        }
        list->entries = grown;
        list->cap = cap;
    }
    int idx = list->count++;
    list->entries[idx].fn = JS_DupValue(ctx, fn);
    list->entries[idx].active = 1;
    return idx;
}

int efx_hooks_active(const struct efx_hook_list *list) {
    int n = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->entries[i].active) {
            n++;
        }
    }
    return n;
}

void efx_hooks_free_all(JSContext *ctx, struct efx_hook_list *list) {
    for (int i = 0; i < list->count; i++) {
        JS_FreeValue(ctx, list->entries[i].fn);
    }
    free(list->entries);
    list->entries = NULL;
    list->count = 0;
    list->cap = 0;
}

static char *dup_string(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) {
        memcpy(p, s, n);
    }
    return p;
}

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    *out_len = rd;
    return buf;
}

static void dump_exception_value(efx_runtime *rt, JSValue exc) {
    if (JS_IsError(exc)) {
        JSValue msg = JS_GetPropertyStr(rt->ctx, exc, "message");
        const char *msg_str = JS_ToCString(rt->ctx, msg);
        fprintf(stderr, "uncaught exception: %s\n", msg_str ? msg_str : "<no message>");
        if (msg_str) {
            JS_FreeCString(rt->ctx, msg_str);
        }
        JS_FreeValue(rt->ctx, msg);
        JSValue stack = JS_GetPropertyStr(rt->ctx, exc, "stack");
        if (!JS_IsUndefined(stack)) {
            const char *stack_str = JS_ToCString(rt->ctx, stack);
            if (stack_str) {
                fprintf(stderr, "%s\n", stack_str);
                JS_FreeCString(rt->ctx, stack_str);
            }
        }
        JS_FreeValue(rt->ctx, stack);
    } else {
        const char *s = JS_ToCString(rt->ctx, exc);
        fprintf(stderr, "uncaught exception: %s\n", s ? s : "<non-error value thrown>");
        if (s) {
            JS_FreeCString(rt->ctx, s);
        }
    }
}

static int finish_exception(efx_runtime *rt) {
    JSValue exc = JS_GetException(rt->ctx);
    if (JS_VALUE_GET_PTR(exc) == JS_VALUE_GET_PTR(rt->host.quit_sentinel)) {
        JS_FreeValue(rt->ctx, exc);
        return 0;
    }
    dump_exception_value(rt, exc);
    JS_FreeValue(rt->ctx, exc);
    rt->in_error = 1;
    return 1;
}

efx_runtime *efx_runtime_new(char *const *args, int arg_count) {
    efx_runtime *rt = calloc(1, sizeof(*rt));
    if (!rt) {
        fprintf(stderr, "player: out of memory\n");
        return NULL;
    }
    rt->js_rt = JS_NewRuntime();
    rt->ctx = JS_NewContext(rt->js_rt);
    rt->host.quit_sentinel = JS_NewObject(rt->ctx);
    rt->host.quit_code = 0;
    if (arg_count > 0 && args) {
        rt->host.args = calloc((size_t)arg_count, sizeof(char *));
        for (int i = 0; i < arg_count; i++) {
            rt->host.args[i] = dup_string(args[i]);
        }
        rt->host.arg_count = arg_count;
    }
    JS_SetContextOpaque(rt->ctx, &rt->host);

    JSValue glob = JS_GetGlobalObject(rt->ctx);
    JSValue efx = JS_NewObject(rt->ctx);
    static const JSCFunctionListEntry efx_funcs[] = {
        JS_CFUNC_DEF("log", 1, efx_js_log),
        JS_CFUNC_DEF("quit", 1, efx_js_quit),
        JS_CFUNC_DEF("args", 0, efx_js_args),
        JS_CFUNC_DEF("registerUpdateHook", 1, efx_js_registerUpdateHook),
        JS_CFUNC_DEF("registerRenderHook", 1, efx_js_registerRenderHook),
        JS_CFUNC_DEF("setClearColor", 1, efx_js_setClearColor),
        JS_CFUNC_DEF("setCamera2D", 1, efx_js_setCamera2D),
        JS_CFUNC_DEF("createImageData", 1, efx_js_createImageData),
        JS_CFUNC_DEF("createTexture", 1, efx_js_createTexture),
        JS_CFUNC_DEF("drawQuad", 4, efx_js_drawQuad),
        JS_CFUNC_DEF("setBlendMode", 1, efx_js_setBlendMode),
        JS_CGETSET_DEF("whiteTexture", efx_js_whiteTexture, NULL),
        JS_CFUNC_DEF("setCamera3D", 1, efx_js_setCamera3D),
        JS_CFUNC_DEF("createMeshData", 1, efx_js_createMeshData),
        JS_CFUNC_DEF("createMesh", 1, efx_js_createMesh),
        JS_CFUNC_DEF("drawMesh", 1, efx_js_drawMesh),
    };
    JS_SetPropertyFunctionList(rt->ctx, efx, efx_funcs,
                               (int)(sizeof(efx_funcs) / sizeof(efx_funcs[0])));
    JS_SetPropertyStr(rt->ctx, glob, "efx", efx);
    JS_FreeValue(rt->ctx, glob);
    if (efx_api_init(rt->ctx) < 0) {
        fprintf(stderr, "player: api init failed\n");
        efx_runtime_destroy(rt);
        return NULL;
    }
    /* engine-bundled pure-JS layer (F3 math + primitives); evaluated
       against the efx namespace so both bindings share one source */
    static const char wrapper[] =
        "(function(efx){\n";
    size_t wrap_len = sizeof(wrapper) - 1;
    size_t total = wrap_len + (size_t)EFX_JS_PRELUDE_LEN + 16;
    char *code = malloc(total);
    if (!code) {
        fprintf(stderr, "player: out of memory\n");
        efx_runtime_destroy(rt);
        return NULL;
    }
    memcpy(code, wrapper, wrap_len);
    memcpy(code + wrap_len, EFX_JS_PRELUDE, (size_t)EFX_JS_PRELUDE_LEN);
    memcpy(code + wrap_len + (size_t)EFX_JS_PRELUDE_LEN, "\n})(efx);\n", 11);
    if (efx_runtime_eval_string(rt, "<prelude>", code) != 0) {
        fprintf(stderr, "player: prelude evaluation failed\n");
        free(code);
        efx_runtime_destroy(rt);
        return NULL;
    }
    free(code);
    return rt;
}

void efx_runtime_destroy(efx_runtime *rt) {
    if (!rt) {
        return;
    }
    efx_hooks_free_all(rt->ctx, &rt->host.update_hooks);
    efx_hooks_free_all(rt->ctx, &rt->host.render_hooks);
    JS_FreeValue(rt->ctx, rt->host.quit_sentinel);
    if (rt->host.has_white_texture) {
        JS_FreeValue(rt->ctx, rt->host.white_texture);
    }
    for (int i = 0; i < rt->host.arg_count; i++) {
        free(rt->host.args[i]);
    }
    free(rt->host.args);
    JS_FreeContext(rt->ctx);
    JS_FreeRuntime(rt->js_rt);
    free(rt);
}

int efx_runtime_eval_file(efx_runtime *rt, const char *path) {
    size_t len = 0;
    char *code = read_file(path, &len);
    if (!code) {
        fprintf(stderr, "player: cannot read script file: %s\n", path);
        return -1;
    }
    JSValue result = JS_Eval(rt->ctx, code, len, path, JS_EVAL_TYPE_GLOBAL);
    free(code);
    if (JS_IsException(result)) {
        return finish_exception(rt);
    }
    return 0;
}

int efx_runtime_eval_string(efx_runtime *rt, const char *name, const char *code) {
    size_t len = strlen(code);
    JSValue result = JS_Eval(rt->ctx, code, len, name, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        return finish_exception(rt);
    }
    return 0;
}

void efx_runtime_pick_hooks(efx_runtime *rt, int *has_update, int *has_render) {
    if (!rt->hooks_sugar_done) {
        rt->hooks_sugar_done = 1;
        JSValue glob = JS_GetGlobalObject(rt->ctx);
        JSValue u = JS_GetPropertyStr(rt->ctx, glob, "update");
        JSValue r = JS_GetPropertyStr(rt->ctx, glob, "render");
        if (JS_IsFunction(rt->ctx, u)) {
            efx_hooks_append(rt->ctx, &rt->host.update_hooks, u);
        }
        if (JS_IsFunction(rt->ctx, r)) {
            efx_hooks_append(rt->ctx, &rt->host.render_hooks, r);
        }
        JS_FreeValue(rt->ctx, u);
        JS_FreeValue(rt->ctx, r);
        JS_FreeValue(rt->ctx, glob);
    }
    if (has_update) {
        *has_update = efx_hooks_active(&rt->host.update_hooks) > 0;
    }
    if (has_render) {
        *has_render = efx_hooks_active(&rt->host.render_hooks) > 0;
    }
}

int efx_runtime_call_hook(efx_runtime *rt, int update_not_render, double dt) {
    struct efx_hook_list *list =
        update_not_render ? &rt->host.update_hooks : &rt->host.render_hooks;
    for (int i = 0; i < list->count; i++) {
        if (!list->entries[i].active) {
            continue;
        }
        JSValue args[1];
        int nargs = 0;
        if (update_not_render) {
            args[0] = JS_NewFloat64(rt->ctx, dt);
            nargs = 1;
        }
        JSValue result = JS_Call(rt->ctx, list->entries[i].fn, JS_UNDEFINED,
                                 nargs, args);
        if (nargs) {
            JS_FreeValue(rt->ctx, args[0]);
        }
        if (JS_IsException(result)) {
            int rc = finish_exception(rt);
            return rc == 0 ? EFX_HOOK_QUIT : EFX_HOOK_ERROR;
        }
        JS_FreeValue(rt->ctx, result);
        if (rt->host.quit_requested) {
            return EFX_HOOK_QUIT;
        }
    }
    return EFX_HOOK_OK;
}

int efx_runtime_quit_requested(const efx_runtime *rt) {
    return rt->host.quit_requested;
}

int efx_runtime_quit_code(const efx_runtime *rt) {
    return rt->host.quit_code;
}

int efx_runtime_in_error(const efx_runtime *rt) {
    return rt->in_error;
}

void efx_runtime_collect(efx_runtime *rt) {
    if (rt) {
        JS_RunGC(rt->js_rt);
    }
}
