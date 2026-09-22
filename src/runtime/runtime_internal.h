#ifndef EFX_RUNTIME_INTERNAL_H
#define EFX_RUNTIME_INTERNAL_H

#include "quickjs.h"

/* lifecycle hooks (ADR 0016): ordered registration, unsubscribe marks an
   entry inactive but keeps its JSValue until runtime teardown, so a hook can
   safely unsubscribe itself while it is being called */
struct efx_hook_entry {
    JSValue fn;
    int active;
};

struct efx_hook_list {
    struct efx_hook_entry *entries;
    int count;
    int cap;
};

struct efx_host_state {
    int quit_requested;
    int quit_code;
    JSValue quit_sentinel;
    char **args;
    int arg_count;
    /* F2 resource state */
    JSValue white_texture;
    int has_white_texture;
    /* lifecycle hook lists (F1 contract, ADR 0016) */
    struct efx_hook_list update_hooks;
    struct efx_hook_list render_hooks;
};

/* append a duplicated reference; returns the stable entry index or -1 */
int efx_hooks_append(JSContext *ctx, struct efx_hook_list *list, JSValueConst fn);
int efx_hooks_active(const struct efx_hook_list *list);
void efx_hooks_free_all(JSContext *ctx, struct efx_hook_list *list);

#endif
