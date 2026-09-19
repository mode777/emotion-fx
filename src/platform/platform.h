#ifndef EFX_PLATFORM_H
#define EFX_PLATFORM_H

typedef struct efx_frame_hooks {
    void *ud;
    int (*on_frame)(void *ud);
} efx_frame_hooks;

int efx_platform_run(efx_frame_hooks hooks);

#endif
