#define _POSIX_C_SOURCE 200809L

#include "runtime/runtime.h"
#include "runtime/runtime_internal.h"
#include "api/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quickjs.h"

struct efx_runtime {
    struct efx_host_state host;
    JSRuntime *js_rt;
    JSContext *ctx;
    int in_error;
    JSValue hook_update;
    JSValue hook_render;
    int has_update;
    int has_render;
};

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
    rt->hook_update = JS_UNDEFINED;
    rt->hook_render = JS_UNDEFINED;
    rt->host.quit_sentinel = JS_NewObject(rt->ctx);
    rt->host.quit_code = 0;
    if (arg_count > 0 && args) {
        rt->host.args = calloc((size_t)arg_count, sizeof(char *));
        for (int i = 0; i < arg_count; i++) {
            rt->host.args[i] = strdup(args[i]);
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
    };
    JS_SetPropertyFunctionList(rt->ctx, efx, efx_funcs, 3);
    JS_SetPropertyStr(rt->ctx, glob, "efx", efx);
    JS_FreeValue(rt->ctx, glob);
    return rt;
}

void efx_runtime_destroy(efx_runtime *rt) {
    if (!rt) {
        return;
    }
    JS_FreeValue(rt->ctx, rt->hook_update);
    JS_FreeValue(rt->ctx, rt->hook_render);
    JS_FreeValue(rt->ctx, rt->host.quit_sentinel);
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

void efx_runtime_pick_hooks(efx_runtime *rt, int *has_update, int *has_render) {
    JSValue glob = JS_GetGlobalObject(rt->ctx);
    JS_FreeValue(rt->ctx, rt->hook_update);
    JS_FreeValue(rt->ctx, rt->hook_render);
    rt->hook_update = JS_GetPropertyStr(rt->ctx, glob, "update");
    rt->hook_render = JS_GetPropertyStr(rt->ctx, glob, "render");
    JS_FreeValue(rt->ctx, glob);
    rt->has_update = JS_IsFunction(rt->ctx, rt->hook_update) > 0;
    rt->has_render = JS_IsFunction(rt->ctx, rt->hook_render) > 0;
    if (has_update) {
        *has_update = rt->has_update;
    }
    if (has_render) {
        *has_render = rt->has_render;
    }
}

int efx_runtime_call_hook(efx_runtime *rt, int update_not_render) {
    int has = update_not_render ? rt->has_update : rt->has_render;
    if (!has) {
        return EFX_HOOK_OK;
    }
    JSValue hook = update_not_render ? rt->hook_update : rt->hook_render;
    JSValue result = JS_Call(rt->ctx, hook, JS_UNDEFINED, 0, NULL);
    if (JS_IsException(result)) {
        int rc = finish_exception(rt);
        return rc == 0 ? EFX_HOOK_QUIT : EFX_HOOK_ERROR;
    }
    JS_FreeValue(rt->ctx, result);
    return rt->host.quit_requested ? EFX_HOOK_QUIT : EFX_HOOK_OK;
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
