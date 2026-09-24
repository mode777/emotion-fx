#define SOKOL_IMPL
#define SOKOL_NO_ENTRY

#if defined(_WIN32)
#define SOKOL_D3D11
#elif defined(__APPLE__)
#define SOKOL_METAL
#elif defined(__EMSCRIPTEN__)
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif

#include <stdio.h>
#include <string.h>

#include "platform/platform.h"
#include "platform/pipeline.h"
#include "platform/capture.h"
#include "render/render.h"

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

/* fixed virtual frame for golden captures (design D7/D9) */
#define EFX_CAP_W 640
#define EFX_CAP_H 480

static efx_frame_hooks g_hooks;
static efx_platform_capture g_capture;
static int g_frame;

#ifdef SOKOL_METAL
/* capture pass renders into an injected Managed texture instead of the
   framebuffer-only swapchain drawable */
static sg_image g_cap_img;
static sg_view g_cap_view;
static sg_image g_cap_depth_img;
static sg_view g_cap_depth_view;
static sg_attachments g_cap_atts;
static void *g_cap_mtl;
static void *g_cap_depth_mtl;
static int g_cap_active;
#endif

/* surface sokol validation/creation errors on stderr (they are silent
   otherwise and render as inexplicable missing geometry) */
static void efx_sokol_log(const char *tag, uint32_t level,
                          uint32_t item_id, const char *message,
                          uint32_t line_nr, const char *filename,
                          void *ud) {
    (void)item_id;
    (void)ud;
    if (level <= 2) { /* panic + error + warning */
        fprintf(stderr, "sokol[%s] %s:%u: %s\n", tag,
                filename ? filename : "?", line_nr,
                message ? message : "<no message>");
        fflush(stderr);
    }
}

static sg_pass_action efx_pass_action(void) {
    sg_pass_action pa;
    memset(&pa, 0, sizeof(pa));
    float c[4];
    efx_render_clear_color(c);
    pa.colors[0].load_action = SG_LOADACTION_CLEAR;
    pa.colors[0].clear_value = (sg_color){c[0], c[1], c[2], c[3]};
    /* F3 depth: cleared to far (1.0) each pass (design D4) */
    pa.depth.load_action = SG_LOADACTION_CLEAR;
    pa.depth.clear_value = 1.0f;
    return pa;
}

#ifdef SOKOL_METAL
static void efx_capture_setup(void) {
    const void *dev = sapp_get_environment().metal.device;
    if (!dev) {
        return;
    }
    id<MTLDevice> mtl = (__bridge id<MTLDevice>)dev;
    /* BGRA8: the sapp swapchain format — matches the environment default
       the pipelines are built with */
    MTLTextureDescriptor *td = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                     width:EFX_CAP_W height:EFX_CAP_H
                                 mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget;
    td.storageMode = MTLStorageModeManaged;
    id<MTLTexture> tex = [mtl newTextureWithDescriptor:td];
    if (!tex) {
        return;
    }
    g_cap_mtl = (__bridge_retained void *)tex;
    efx_capture_metal_set_texture(g_cap_mtl);
    g_cap_img = sg_make_image(&(sg_image_desc){
        .width = EFX_CAP_W,
        .height = EFX_CAP_H,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage.color_attachment = true,
        .mtl_textures[0] = g_cap_mtl,
    });
    g_cap_view = sg_make_view(&(sg_view_desc){
        .color_attachment.image = g_cap_img,
    });
    /* depth attachment so golden captures depth-test like the window pass
       (design D4) */
    MTLTextureDescriptor *dd = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                     width:EFX_CAP_W height:EFX_CAP_H
                                 mipmapped:NO];
    dd.usage = MTLTextureUsageRenderTarget;
    dd.storageMode = MTLStorageModePrivate;
    id<MTLTexture> dtex = [mtl newTextureWithDescriptor:dd];
    if (!dtex) {
        return;
    }
    g_cap_depth_mtl = (__bridge_retained void *)dtex;
    g_cap_depth_img = sg_make_image(&(sg_image_desc){
        .width = EFX_CAP_W,
        .height = EFX_CAP_H,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .usage.depth_stencil_attachment = true,
        .mtl_textures[0] = g_cap_depth_mtl,
    });
    g_cap_depth_view = sg_make_view(&(sg_view_desc){
        .depth_stencil_attachment.image = g_cap_depth_img,
    });
    memset(&g_cap_atts, 0, sizeof(g_cap_atts));
    g_cap_atts.colors[0] = g_cap_view;
    g_cap_atts.depth_stencil = g_cap_depth_view;
    g_cap_active = 1;
}
#endif

static void efx_init_cb(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger = {.func = efx_sokol_log},
    });
    efx_pipeline_install();
#ifdef SOKOL_METAL
    if (g_capture.frame > 0) {
        efx_capture_setup();
    }
#endif
}

static void efx_frame_cb(void) {
    g_frame++;
    double dt = (g_frame == 1) ? 0.0 : sapp_frame_duration();
    efx_render_begin_frame();
    efx_render_set_viewport(sapp_width(), sapp_height());
    if (g_hooks.on_frame && g_hooks.on_frame(g_hooks.ud, dt)) {
        sapp_quit();
        return;
    }

#ifdef SOKOL_METAL
    int use_capture_pass = g_cap_active && g_frame == g_capture.frame;
#endif

#ifdef SOKOL_METAL
    if (use_capture_pass) {
        sg_begin_pass(&(sg_pass){
            .action = efx_pass_action(),
            .attachments = g_cap_atts,
        });
        efx_pipeline_play();
        sg_end_pass();
    } else {
        sg_begin_pass(&(sg_pass){
            .action = efx_pass_action(),
            .swapchain = sglue_swapchain(),
        });
        efx_pipeline_play();
        sg_end_pass();
    }
#else
    sg_begin_pass(&(sg_pass){
        .action = efx_pass_action(),
        .swapchain = sglue_swapchain(),
    });
    efx_pipeline_play();
    sg_end_pass();
#endif
    sg_commit();
    efx_render_end_frame();

#if defined(__EMSCRIPTEN__)
    if (g_capture.frame > 0 && g_frame >= g_capture.frame) {
        /* web: read back via canvas.toDataURL (native glReadPixels from the
           default framebuffer crashes headless shells with SwiftShader) */
        EM_ASM({
            try {
                const url = document.getElementById('canvas').toDataURL('image/png');
                Module['webGoldenCapture'] = url.substring(url.indexOf(',') + 1);
            } catch (e) {
                console.error('golden capture export failed:', e);
            }
        });
        sapp_quit();
    }
#else
    if (g_capture.frame > 0 && g_frame >= g_capture.frame) {
        uint8_t *px = NULL;
        int w = 0, h = 0;
        if (efx_capture_read_rgba(&px, &w, &h) == 0) {
            if (efx_capture_write_png(g_capture.output, w, h, px) != 0) {
                fprintf(stderr, "player: capture PNG write failed: %s\n", g_capture.output);
                fflush(stderr);
            }
            free(px);
        } else {
            fprintf(stderr, "player: capture readback failed\n");
            fflush(stderr);
        }
        sapp_quit();
    }
#endif
}

static void efx_cleanup_cb(void) {
#ifdef SOKOL_METAL
    if (g_cap_mtl) {
        id<MTLTexture> tex = (__bridge id<MTLTexture>)g_cap_mtl;
        [tex release];
        g_cap_mtl = NULL;
    }
#endif
#if defined(__EMSCRIPTEN__)
    /* the web loop has no C caller after sapp_run returns (ADR 0022): the
       full render-stack teardown happens here, when the sokol loop ends */
    efx_render_end_frame();
    efx_render_shutdown();
    efx_pipeline_shutdown();
    sg_shutdown();
#else
    /* sg_shutdown is deferred to efx_platform_shutdown() so callers can
       release their GPU resources first */
#endif
}

int efx_platform_run(const efx_platform_desc *desc, efx_frame_hooks hooks) {
    g_hooks = hooks;
    memset(&g_capture, 0, sizeof(g_capture));
    if (desc) {
        g_capture = desc->capture;
    }
    g_frame = 0;
    sapp_desc d;
    memset(&d, 0, sizeof(d));
    d.init_cb = efx_init_cb;
    d.frame_cb = efx_frame_cb;
    d.cleanup_cb = efx_cleanup_cb;
    d.width = (desc && desc->width > 0) ? desc->width : 1024;
    d.height = (desc && desc->height > 0) ? desc->height : 600;
    if (g_capture.frame > 0) {
        d.width = EFX_CAP_W;
        d.height = EFX_CAP_H;
    }
    d.window_title = "EmotionFX";
#if defined(__EMSCRIPTEN__)
    if (g_capture.frame > 0) {
        d.html5.preserve_drawing_buffer = true; /* canvas readback after commit */
    }
#endif
    sapp_run(&d);
    return 0;
}

void efx_platform_shutdown(void) {
    efx_pipeline_shutdown();
#ifdef SOKOL_METAL
    if (g_cap_active) {
        sg_destroy_view(g_cap_view);
        sg_destroy_image(g_cap_img);
        sg_destroy_view(g_cap_depth_view);
        sg_destroy_image(g_cap_depth_img);
        g_cap_active = 0;
    }
    if (g_cap_depth_mtl) {
        id<MTLTexture> dtex = (__bridge id<MTLTexture>)g_cap_depth_mtl;
        [dtex release];
        g_cap_depth_mtl = NULL;
    }
#endif
    sg_shutdown();
}
