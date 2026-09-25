/* TEMPORARY cross-platform rendering probe (f3-3d-core debugging):
 * a verbatim adaptation of floooh/sokol-samples sapp/cube-sapp.c (official
 * sample) against THIS REPO's pinned sokol and shdc-generated shader, with
 * a per-backend frame readback writing a PNG so CI can verify it.
 * Removed once the Windows/Metal mesh-rendering issue is resolved.
 */
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

#if defined(_WIN32)
#include <initguid.h>
#include <d3d11.h>
#include <dxgi.h>
#endif
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#define VECMATH_GENERICS
#include "vecmath.h"
#define SOKOL_SHDC_IMPL
#include "cube-sapp.h"
#include "mesh.h"
#include "../../src/math/efx_math.h"


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>

static void diag_slog(const char *tag, uint32_t level, uint32_t item,
                      const char *message, uint32_t line_nr,
                      const char *filename, void *ud) {
    (void)item; (void)ud;
    fprintf(stderr, "cube_diag[%s] %s:%u: %s\n", tag,
            filename ? filename : "?", line_nr,
            message ? message : "<no message>");
}

static int diag_variant(void) {
    const char *v = getenv("CUBE_DIAG_VARIANT");
    return (v && *v) ? (*v - '0') : 0;
}
static int diag_use_u32(void) {
    int v = diag_variant();
    return (v == 1 || v == 3 || v == 4);
}

static struct {
    float rx, ry;
    sg_pipeline pip;
    sg_bindings bind;
} state;

static int g_frame;
static const char *g_out_path;
static int g_done;

static vs_params_t compute_vsparams(float rx, float ry) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    mat44_t proj = mat44_perspective_fov_rh(vm_radians(60.0f), w/h, 0.01f, 10.0f);
    mat44_t view = mat44_look_at_rh(vec3(0.0f, 1.5f, 4.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat44_t view_proj = vm_mul(view, proj);
    mat44_t rxm = mat44_rotation_x(vm_radians(rx));
    mat44_t rym = mat44_rotation_y(vm_radians(ry));
    mat44_t model = vm_mul(rym, rxm);
    return (vs_params_t){ .mvp = vm_mul(model, view_proj) };
}

static void write_png(const uint8_t *px, int w, int h) {
    if (!g_out_path) {
        g_out_path = "cube_diag.png";
    }
    if (!stbi_write_png(g_out_path, w, h, 4, px, w * 4)) {
        fprintf(stderr, "cube_diag: PNG write failed: %s\n", g_out_path);
        exit(4);
    }
    printf("cube_diag: wrote %s (%dx%d)\n", g_out_path, w, h);
    fflush(stdout);
}

#if defined(SOKOL_METAL)
static id<MTLTexture> diag_color_tex;   /* Managed, readback-able */
static sg_image diag_color_img;
static sg_view diag_color_view;
static id<MTLTexture> diag_depth_tex;
static sg_image diag_depth_img;
static sg_view diag_depth_view;
static sg_attachments diag_atts;
static void diag_attachments_setup(id<MTLDevice> dev) {
    MTLTextureDescriptor *cd = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                     width:640 height:480 mipmapped:NO];
    cd.usage = MTLTextureUsageRenderTarget;
    cd.storageMode = MTLStorageModeManaged;
    id<MTLTexture> ctex = [dev newTextureWithDescriptor:cd];
    diag_color_tex = ctex;
    diag_color_img = sg_make_image(&(sg_image_desc){
        .width = 640, .height = 480,
        .pixel_format = SG_PIXELFORMAT_BGRA8,
        .usage.color_attachment = true,
        .mtl_textures[0] = (__bridge const void *)ctex,
    });
    diag_color_view = sg_make_view(&(sg_view_desc){
        .color_attachment.image = diag_color_img});
    MTLTextureDescriptor *dd = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                     width:640 height:480 mipmapped:NO];
    dd.usage = MTLTextureUsageRenderTarget;
    dd.storageMode = MTLStorageModePrivate;
    id<MTLTexture> dtex = [dev newTextureWithDescriptor:dd];
    diag_depth_tex = dtex;
    diag_depth_img = sg_make_image(&(sg_image_desc){
        .width = 640, .height = 480,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .usage.depth_stencil_attachment = true,
        .mtl_textures[0] = (__bridge const void *)dtex,
    });
    diag_depth_view = sg_make_view(&(sg_view_desc){
        .depth_stencil_attachment.image = diag_depth_img});
    diag_atts.colors[0] = diag_color_view;
    diag_atts.depth_stencil = diag_depth_view;
}
#endif

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = diag_slog,
    });
#if defined(SOKOL_METAL)
    const void *mdev = sglue_environment().metal.device;
    diag_attachments_setup((__bridge id<MTLDevice>)mdev);
#endif

    // cube vertex buffer (verbatim from cube-sapp.c)
    float vertices[] = {
        -1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
         1.0, -1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
         1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,
        -1.0,  1.0, -1.0,   1.0, 0.0, 0.0, 1.0,

        -1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
         1.0, -1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
         1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,
        -1.0,  1.0,  1.0,   0.0, 1.0, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0,  1.0, -1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0,  1.0,  1.0,   0.0, 0.0, 1.0, 1.0,
        -1.0, -1.0,  1.0,   0.0, 0.0, 1.0, 1.0,

        1.0, -1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
        1.0,  1.0, -1.0,    1.0, 0.5, 0.0, 1.0,
        1.0,  1.0,  1.0,    1.0, 0.5, 0.0, 1.0,
        1.0, -1.0,  1.0,    1.0, 0.5, 0.0, 1.0,

        -1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,
        -1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
         1.0, -1.0,  1.0,   0.0, 0.5, 1.0, 1.0,
         1.0, -1.0, -1.0,   0.0, 0.5, 1.0, 1.0,

        -1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0,
        -1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
         1.0,  1.0,  1.0,   1.0, 0.0, 0.5, 1.0,
         1.0,  1.0, -1.0,   1.0, 0.0, 0.5, 1.0
    };
    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "cube-vertices"
    });

    uint32_t indices32[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .usage.index_buffer = true,
        .data = {
            .ptr = diag_use_u32() ? (const void *)indices32
                                  : (const void *)indices,
            .size = 36 * (diag_use_u32() ? 4 : 2),
        },
        .label = "cube-indices"
    });

    const int use_mesh_shader = (diag_variant() == 3);
    sg_shader shd = sg_make_shader(use_mesh_shader
        ? mesh_shader_desc(sg_query_backend())
        : cube_shader_desc(sg_query_backend()));

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = 28,
            .attrs = {
                [ATTR_cube_position].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_cube_color0].format   = SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = shd,
        .index_type = diag_use_u32() ? SG_INDEXTYPE_UINT32
                                     : SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
        .label = "cube-pipeline"
    });

    state.bind = (sg_bindings) {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
    };
}


static void frame(void) {
    const float t = (float)(sapp_frame_duration() * 60.0);
    state.rx += 1.0f * t; state.ry += 2.0f * t;
    const int variant = diag_variant();
    const int use_mesh_shader = (variant == 3 || variant == 4);
    vs_params_t vs_params;
    if (variant == 4) {
        /* engine camera math: efx_math rows from the same view */
        float proj[16], view[16], vp[16], model[16], mvp[16];
        float eye[3] = {0.0f, 1.5f, 4.0f};
        float center[3] = {0.0f, 0.0f, 0.0f};
        float up[3] = {0.0f, 1.0f, 0.0f};
        float ax[3] = {0, 1, 0};
        float ay[3] = {1, 0, 0};
        efx_math_perspective(proj, 60.0f, 640.0f / 480.0f, 0.01f, 10.0f);
        efx_math_look_at(view, eye, center, up);
        efx_math_mul(vp, proj, view);
        efx_math_identity(model);
        efx_math_rotate(model, model, state.ry, ax);
        efx_math_rotate(model, model, state.rx, ay);
        efx_math_mul(mvp, vp, model);
        for (int r = 0; r < 4; r++) {
            float *dst = (r == 0) ? vs_params.mvp0 : (r == 1) ? vs_params.mvp1
                       : (r == 2) ? vs_params.mvp2 : vs_params.mvp3;
            for (int c = 0; c < 4; c++) {
                dst[c] = mvp[c * 4 + r];
            }
        }
        vs_params.tint[0] = 1; vs_params.tint[1] = 1;
        vs_params.tint[2] = 1; vs_params.tint[3] = 1;
    } else {
        vs_params = compute_vsparams(state.rx, state.ry);
    }

#if defined(SOKOL_METAL)
    sg_begin_pass(&(sg_pass){
        .action.colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.25f, 0.5f, 0.75f, 1.0f }
        },
        .action.depth = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = 1.0f
        },
        .attachments = diag_atts
    });
#else
    sg_begin_pass(&(sg_pass){
        .action.colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.25f, 0.5f, 0.75f, 1.0f }
        },
        .action.depth = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = 1.0f
        },
        .swapchain = sglue_swapchain()
    });
#endif
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();

    g_frame++;
    if (g_frame < 30 || g_done) {
        return;
    }
    g_done = 1;

#if defined(_WIN32)
    /* D3D11: staging copy of the swapchain back buffer */
    sapp_environment env = sapp_get_environment();
    ID3D11Device *dev = (ID3D11Device *)env.d3d11.device;
    ID3D11DeviceContext *ctx = (ID3D11DeviceContext *)env.d3d11.device_context;
    IDXGISwapChain *sc = (IDXGISwapChain *)sapp_d3d11_get_swap_chain();
    ID3D11Texture2D *back = NULL;
    if (FAILED(sc->lpVtbl->GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void **)&back))) {
        fprintf(stderr, "cube_diag: GetBuffer failed\n");
        exit(3);
    }
    D3D11_TEXTURE2D_DESC bd;
    back->lpVtbl->GetDesc(back, &bd);
    D3D11_TEXTURE2D_DESC sd = bd;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.MipLevels = 1;
    sd.ArraySize = 1;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    ID3D11Texture2D *staging = NULL;
    if (FAILED(dev->lpVtbl->CreateTexture2D(dev, &sd, NULL, &staging))) {
        fprintf(stderr, "cube_diag: staging create failed\n");
        exit(3);
    }
    ctx->lpVtbl->CopyResource(ctx, (ID3D11Resource *)staging, (ID3D11Resource *)back);
    D3D11_MAPPED_SUBRESOURCE map;
    if (FAILED(ctx->lpVtbl->Map(ctx, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &map))) {
        fprintf(stderr, "cube_diag: Map failed\n");
        exit(3);
    }
    uint8_t *px = malloc((size_t)bd.Width * bd.Height * 4);
    for (unsigned int y = 0; y < bd.Height; y++) {
        const uint8_t *src = (const uint8_t *)map.pData + (size_t)y * map.RowPitch;
        uint8_t *dst = px + (size_t)y * bd.Width * 4;
        for (unsigned int x = 0; x < bd.Width; x++) {
            dst[x * 4 + 0] = src[x * 4 + 2]; /* BGRA -> RGBA */
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = src[x * 4 + 3];
        }
    }
    ctx->lpVtbl->Unmap(ctx, (ID3D11Resource *)staging, 0);
    staging->lpVtbl->Release(staging);
    back->lpVtbl->Release(back);
    write_png(px, (int)bd.Width, (int)bd.Height);
    free(px);
#elif defined(SOKOL_METAL)
    /* Metal: synchronize + read the Managed offscreen color texture */
    int w = (int)diag_color_tex.width;
    int h = (int)diag_color_tex.height;
    id<MTLDevice> dev = diag_color_tex.device;
    id<MTLCommandQueue> q = [dev newCommandQueue];
    id<MTLCommandBuffer> cb = [q commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    [blit synchronizeTexture:diag_color_tex slice:0 level:0];
    [blit endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    uint8_t *px = malloc((size_t)w * h * 4);
    if (!px) {
        exit(3);
    }
    [diag_color_tex getBytes:px bytesPerRow:(NSUInteger)w * 4
                  fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
    /* BGRA -> RGBA */
    for (int y = 0; y < h; y++) {
        uint8_t *row = px + (size_t)y * w * 4;
        for (int x = 0; x < w; x++) {
            uint8_t b = row[x * 4 + 0];
            row[x * 4 + 0] = row[x * 4 + 2];
            row[x * 4 + 2] = b;
        }
    }
    write_png(px, w, h);
    free(px);
#elif defined(SOKOL_GLCORE)
    /* GL: bottom-up read + flip */
    int w = sapp_width();
    int h = sapp_height();
    uint8_t *px = malloc((size_t)w * h * 4);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (int y = 0; y < h / 2; y++) {
        uint8_t *a = px + (size_t)y * w * 4;
        uint8_t *b = px + (size_t)(h - 1 - y) * w * 4;
        for (int x = 0; x < w * 4; x++) {
            uint8_t tmp = a[x];
            a[x] = b[x];
            b[x] = tmp;
        }
    }
    write_png(px, w, h);
    free(px);
#endif
    sapp_quit();
}

static void cleanup(void) {
    sg_shutdown();
}

int main(int argc, char *argv[]) {
    (void)argc;
    g_out_path = (argc > 1) ? argv[1] : "cube_diag.png";
    sapp_run(&(sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 640,
        .height = 480,
        .sample_count = (diag_variant() >= 2) ? 1 : 4,
        .window_title = "cube_diag (official cube-sapp adaptation)",
        .logger.func = diag_slog,
    });
    return 0;
}
