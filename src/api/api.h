#ifndef EFX_API_H
#define EFX_API_H

#include "quickjs.h"

JSValue efx_js_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_quit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue efx_js_args(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

void efx_log(const char *msg);

#endif
