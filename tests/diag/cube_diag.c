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

#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_glue.h"
#define VECMATH_GENERICS
#include "vecmath.h"
#define SOKOL_SHDC_IMPL
#include "cube-sapp.h"

#if defined(_WIN32)
#include <initguid.h>
#include <d3d11.h>
#include <dxgi.h>
#endif

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

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = diag_slog,
    });

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
        .data = SG_RANGE(indices),
        .label = "cube-indices"
    });

    sg_shader shd = sg_make_shader(cube_shader_desc(sg_query_backend()));

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].stride = 28,
            .attrs = {
                [ATTR_cube_position].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_cube_color0].format   = SG_VERTEXFORMAT_FLOAT4
            }
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
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
    const vs_params_t vs_params = compute_vsparams(state.rx, state.ry);

    sg_begin_pass(&(sg_pass){
        .action.colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.25f, 0.5f, 0.75f, 1.0f }
        },
        .swapchain = sglue_swapchain()
    });
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
    /* Metal: synchronize + read the current drawable */
    id<MTLDrawable> drawable = (__bridge id<MTLDrawable>)sapp_metal_get_drawable();
    id<MTLTexture> tex = (id<MTLTexture>)drawable;
    id<MTLDevice> dev = tex.device;
    int w = (int)tex.width;
    int h = (int)tex.height;
    id<MTLCommandQueue> q = [dev newCommandQueue];
    id<MTLCommandBuffer> cb = [q commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    [blit synchronizeTexture:tex slice:0 level:0];
    [blit endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    uint8_t *px = malloc((size_t)w * h * 4);
    if (!px) {
        exit(3);
    }
    [tex getBytes:px bytesPerRow:(NSUInteger)w * 4
       fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
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
        .sample_count = 4,
        .window_title = "cube_diag (official cube-sapp adaptation)",
        .logger.func = diag_slog,
    });
    return 0;
}
