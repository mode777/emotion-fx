#ifndef EFX_API_H
#define EFX_API_H

#include "quickjs.h"

JSValue efx_js_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_quit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_args(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F1 — lifecycle hooks (ADR 0016) */
JSValue efx_js_registerUpdateHook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_registerRenderHook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* F2 — 2D drawing */
JSValue efx_js_setClearColor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_setCamera2D(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_createImageData(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_createTexture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_drawQuad(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_setBlendMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_whiteTexture(JSContext *ctx, JSValueConst this_val);

/* per-context setup: registers resource classes, prototypes and state */
int efx_api_init(JSContext *ctx);

void efx_log(const char *msg);

#endif
