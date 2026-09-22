#ifndef EFX_RUNTIME_H
#define EFX_RUNTIME_H

#define EFX_HOOK_OK 0
#define EFX_HOOK_QUIT 1
#define EFX_HOOK_ERROR 2

typedef struct efx_runtime efx_runtime;

efx_runtime *efx_runtime_new(char *const *args, int arg_count);
void efx_runtime_destroy(efx_runtime *rt);

int efx_runtime_eval_file(efx_runtime *rt, const char *path);
int efx_runtime_eval_string(efx_runtime *rt, const char *name, const char *code);
void efx_runtime_pick_hooks(efx_runtime *rt, int *has_update, int *has_render);
int efx_runtime_call_hook(efx_runtime *rt, int update_not_render, double dt);

int efx_runtime_quit_requested(const efx_runtime *rt);
int efx_runtime_quit_code(const efx_runtime *rt);
int efx_runtime_in_error(const efx_runtime *rt);
void efx_runtime_collect(efx_runtime *rt); /* frame-end GC (finalizers) */

#endif
