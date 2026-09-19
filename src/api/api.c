#include "api/api.h"
#include "runtime/runtime_internal.h"

#include <stdio.h>
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
