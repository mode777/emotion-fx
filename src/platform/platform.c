#define SOKOL_IMPL
#define SOKOL_NO_ENTRY

#if defined(_WIN32)
#define SOKOL_D3D11
#elif defined(__APPLE__)
#define SOKOL_METAL
#elif defined(EMSCRIPTEN)
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif

#include <string.h>

#include "platform/platform.h"

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

static efx_frame_hooks g_hooks;
static sg_pass_action g_pass_action;

static void efx_init_cb(void) {
    sg_setup(&(sg_desc){0});
}

static void efx_frame_cb(void) {
    if (g_hooks.on_frame && g_hooks.on_frame(g_hooks.ud)) {
        sapp_quit();
    }
    sg_begin_pass(&(sg_pass){
        .action = g_pass_action,
        .swapchain = sglue_swapchain(),
    });
    sg_end_pass();
    sg_commit();
}

static void efx_cleanup_cb(void) {
    sg_shutdown();
}

int efx_platform_run(efx_frame_hooks hooks) {
    g_hooks = hooks;
    memset(&g_pass_action, 0, sizeof(g_pass_action));
    g_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    g_pass_action.colors[0].clear_value = (sg_color){0.13f, 0.15f, 0.20f, 1.0f};
    sapp_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.init_cb = efx_init_cb;
    desc.frame_cb = efx_frame_cb;
    desc.cleanup_cb = efx_cleanup_cb;
    desc.width = 1024;
    desc.height = 600;
    desc.window_title = "EmotionFX";
    sapp_run(&desc);
    return 0;
}
