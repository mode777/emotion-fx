#ifndef EFX_PLATFORM_H
#define EFX_PLATFORM_H

typedef struct efx_frame_hooks {
    void *ud;
    int (*on_frame)(void *ud);
} efx_frame_hooks;

typedef struct efx_platform_capture {
    int frame;         /* capture after this 1-based frame; 0 = off */
    const char *output; /* PNG output path */
} efx_platform_capture;

typedef struct efx_platform_desc {
    int width, height;      /* window size; 0 = default */
    efx_platform_capture capture;
} efx_platform_desc;

int efx_platform_run(const efx_platform_desc *desc, efx_frame_hooks hooks);
void efx_platform_shutdown(void); /* after callers released GPU resources */

#endif
