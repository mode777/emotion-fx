/*
 * Sokol implementation of the render-module GPU sink: canned 2D quad
 * shader (ADR 0015), three blend pipeline variants, dynamic vertex
 * buffer, batching over the frame's records (design D2/D3/D10).
 */
#include "platform/pipeline.h"
#include "platform/backend.h"
#include "render/render.h"

#include <stdlib.h>
#include <string.h>

#include "sokol_gfx.h"

/* interleaved quad vertex: pos(2f) uv(2f) color(ub4n) = 20 bytes */
typedef struct {
    float x, y, u, v;
    uint8_t r, g, b, a;
} pipe_vertex;

typedef struct {
    sg_image img;
    sg_view view;
} pipe_tex;

typedef struct {
    sg_shader shd;
    sg_pipeline pip[3];
    sg_sampler smp;
    sg_buffer vbuf;
    pipe_vertex *scratch;
    int scratch_cap;
    int installed;
} pipe_state;

static pipe_state P;

#if defined(SOKOL_GLCORE) || defined(SOKOL_GLES3)
static const char *VS_SRC =
    "#version 300 es\n"
    "in vec2 a_pos;\n"
    "in vec2 a_uv;\n"
    "in vec4 a_color;\n"
    "out vec2 efx_uv;\n"
    "out vec4 efx_color;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "    efx_uv = a_uv;\n"
    "    efx_color = a_color;\n"
    "}\n";
static const char *FS_SRC =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D efx_tex0;\n"
    "in vec2 efx_uv;\n"
    "in vec4 efx_color;\n"
    "out vec4 frag_color;\n"
    "void main() { frag_color = texture(efx_tex0, efx_uv) * efx_color; }\n";
#elif defined(SOKOL_D3D11)
static const char *VS_SRC =
    "struct vs_in { float2 a_pos: A_POS; float2 a_uv: A_UV; float4 a_color: A_COL; };\n"
    "struct vs_out { float4 pos: SV_Position; float2 uv: TEXCOORD0; float4 color: TEXCOORD1; };\n"
    "vs_out vs_main(vs_in inp) {\n"
    "    vs_out o; o.pos = float4(inp.a_pos, 0.0, 1.0); o.uv = inp.a_uv; o.color = inp.a_color; return o;\n"
    "}\n";
static const char *FS_SRC =
    "Texture2D efx_tex0 : register(t0); SamplerState efx_smp : register(s0);\n"
    "float4 fs_main(vs_out inp) : SV_Target {\n"
    "    return efx_tex0.Sample(efx_smp, inp.uv) * inp.color;\n"
    "}\n";
#elif defined(SOKOL_METAL)
static const char *VS_SRC =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct vs_out {\n"
    "    float4 pos [[position]];\n"
    "    float2 uv;\n"
    "    float4 color;\n"
    "};\n"
    "vertex vs_out vs_main(float2 a_pos [[attribute(0)]], float2 a_uv [[attribute(1)]], float4 a_color [[attribute(2)]]) {\n"
    "    vs_out o; o.pos = float4(a_pos, 0.0, 1.0); o.uv = a_uv; o.color = a_color; return o;\n"
    "}\n";
static const char *FS_SRC =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_in {\n"
    "    float4 pos [[position]];\n"
    "    float2 uv;\n"
    "    float4 color;\n"
    "};\n"
    "fragment float4 fs_main(fs_in inp [[stage_in]], texture2d<float> efx_tex0 [[texture(0)]], sampler efx_smp [[sampler(0)]]) {\n"
    "    return efx_tex0.sample(efx_smp, inp.uv) * inp.color;\n"
    "}\n";
#endif

static void *pipe_create_texture(void *ud, int w, int h, const uint8_t *rgba) {
    (void)ud;
    pipe_tex *t = calloc(1, sizeof(pipe_tex));
    t->img = sg_make_image(&(sg_image_desc){
        .width = w,
        .height = h,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data = {.mip_levels[0] = {.ptr = rgba, .size = (size_t)w * h * 4}},
    });
    t->view = sg_make_view(&(sg_view_desc){
        .texture.image = t->img,
    });
    return t;
}

static void pipe_destroy_texture(void *ud, void *native) {
    (void)ud;
    pipe_tex *t = (pipe_tex *)native;
    if (t) {
        sg_destroy_view(t->view);
        sg_destroy_image(t->img);
        free(t);
    }
}

static uint32_t pack_color(const float c[4]) {
    uint32_t r = (uint32_t)(c[0] * 255.0f + 0.5f);
    uint32_t g = (uint32_t)(c[1] * 255.0f + 0.5f);
    uint32_t b = (uint32_t)(c[2] * 255.0f + 0.5f);
    uint32_t a = (uint32_t)(c[3] * 255.0f + 0.5f);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

static pipe_vertex emit_vert(const efx_quad_record *r, int i) {
    /* corner i in strip order: TL, TR, BL, BR; local space is 0..w, 0..h */
    static const float lu[4] = {0, 1, 0, 1};
    static const float lv[4] = {0, 0, 1, 1};
    static const float su[4] = {0, 1, 0, 1};
    static const float sv[4] = {0, 0, 1, 1};
    pipe_vertex v;
    const uint32_t packed = pack_color(r->color);
    {
        float fx = r->m.a * lu[i] * r->w + r->m.c * lv[i] * r->h + r->m.tx;
        float fy = r->m.b * lu[i] * r->w + r->m.d * lv[i] * r->h + r->m.ty;
        v.x = 2.0f * fx / r->frame_w - 1.0f;
        v.y = 1.0f - 2.0f * fy / r->frame_h;
        v.u = (r->sx + su[i] * r->sw) / r->tw;
        v.v = (r->sy + sv[i] * r->sh) / r->th;
        v.r = (uint8_t)(packed & 0xff);
        v.g = (uint8_t)((packed >> 8) & 0xff);
        v.b = (uint8_t)((packed >> 16) & 0xff);
        v.a = (uint8_t)((packed >> 24) & 0xff);
    }
    return v;
}

static pipe_vertex *emit_quad(pipe_vertex *v, const efx_quad_record *r) {
    for (int i = 0; i < 4; i++) {
        *v++ = emit_vert(r, i);
    }
    return v;
}

static pipe_vertex *emit_quad_bridged(pipe_vertex *v, const efx_quad_record *r) {
    /* continue a strip: duplicate last vertex, then first vertex of the new
       quad twice, then the remaining three (two degenerate triangles) */
    v[0] = v[-1];
    v[1] = emit_vert(r, 0);
    v[2] = v[1];
    v += 3;
    for (int i = 1; i < 4; i++) {
        *v++ = emit_vert(r, i);
    }
    return v;
}


void efx_pipeline_install(void) {
    if (P.installed) {
        return;
    }
    memset(&P, 0, sizeof(P));

    P.shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source = VS_SRC,
        .fragment_func.source = FS_SRC,
        .attrs = {
            [0] = {.glsl_name = "a_pos", .hlsl_sem_name = "A_POS"},
            [1] = {.glsl_name = "a_uv", .hlsl_sem_name = "A_UV"},
            [2] = {.glsl_name = "a_color", .hlsl_sem_name = "A_COL"},
        },
        .views[0].texture = {.stage = SG_SHADERSTAGE_FRAGMENT,
                             .hlsl_register_t_n = 0, .msl_texture_n = 0},
        .samplers[0] = {.stage = SG_SHADERSTAGE_FRAGMENT,
                        .hlsl_register_s_n = 0, .msl_sampler_n = 0},
        .texture_sampler_pairs[0] = {.stage = SG_SHADERSTAGE_FRAGMENT,
                                     .view_slot = 0, .sampler_slot = 0,
                                     .glsl_name = "efx_tex0"},
    });

    static const sg_blend_state blends[3] = {
        /* alpha: src*sa + dst*(1-sa) */
        {.enabled = true,
         .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
         .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
         .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
         .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA},
        /* additive: dst + src*sa */
        {.enabled = true,
         .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
         .dst_factor_rgb = SG_BLENDFACTOR_ONE,
         .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
         .dst_factor_alpha = SG_BLENDFACTOR_ONE},
        /* subtractive: dst - src*sa */
        {.enabled = true,
         .op_rgb = SG_BLENDOP_SUBTRACT,
         .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
         .dst_factor_rgb = SG_BLENDFACTOR_ONE,
         .op_alpha = SG_BLENDOP_SUBTRACT,
         .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
         .dst_factor_alpha = SG_BLENDFACTOR_ONE},
    };

    for (int i = 0; i < 3; i++) {
        P.pip[i] = sg_make_pipeline(&(sg_pipeline_desc){
            .shader = P.shd,
            .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
            .layout = {.buffers[0].stride = (int)sizeof(pipe_vertex),
                       .attrs = {[0] = {.format = SG_VERTEXFORMAT_FLOAT2},
                                 [1] = {.format = SG_VERTEXFORMAT_FLOAT2, .offset = 8},
                                 [2] = {.format = SG_VERTEXFORMAT_UBYTE4N, .offset = 16}}},
            .colors[0] = {.pixel_format = SG_PIXELFORMAT_RGBA8,
                          .blend = blends[i]},
            .depth = {.compare = SG_COMPAREFUNC_ALWAYS, .write_enabled = false},
            .cull_mode = SG_CULLMODE_NONE,
            .sample_count = 1,
        });
    }

    P.smp = sg_make_sampler(&(sg_sampler_desc){.min_filter = SG_FILTER_LINEAR,
                                               .mag_filter = SG_FILTER_LINEAR});
    P.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = 256 * 1024,
        .usage = {.vertex_buffer = true, .dynamic_update = true},
    });
    P.installed = 1;

    static const efx_render_sink sink = {
        NULL, pipe_create_texture, pipe_destroy_texture, NULL, NULL,
    };
    efx_render_install_sink(&sink);
    efx_render_white_texture(); /* engine-owned 1x1 white (design D5) */
}

void efx_pipeline_play(void) {
    if (!P.installed) {
        return;
    }
    int count = 0;
    const efx_quad_record *records = efx_render_records(&count);
    if (count <= 0) {
        return;
    }
    int run_count = 0;
    const efx_draw_run *runs = efx_render_runs(&run_count);
    if (!runs || run_count <= 0) {
        return;
    }

    int cap = count * 6 + 4;
    if (cap > P.scratch_cap) {
        pipe_vertex *grown = realloc(P.scratch, (size_t)cap * sizeof(pipe_vertex));
        if (!grown) {
            return;
        }
        P.scratch = grown;
        P.scratch_cap = cap;
    }

    /* emit all runs; strip degenerates bridge between quads */
    pipe_vertex *v = P.scratch;
    int *run_first = malloc((size_t)run_count * sizeof(int));
    int *run_verts = malloc((size_t)run_count * sizeof(int));
    if (!run_first || !run_verts) {
        free(run_first);
        free(run_verts);
        return;
    }
    for (int ri = 0; ri < run_count; ri++) {
        run_first[ri] = (int)(v - P.scratch);
        int start = (int)(v - P.scratch);
        for (int q = 0; q < runs[ri].count; q++) {
            if (q > 0) {
                v = emit_quad_bridged(v, &records[runs[ri].start + q]);
            } else {
                v = emit_quad(v, &records[runs[ri].start + q]);
            }
        }
        run_verts[ri] = (int)(v - P.scratch) - start;
    }
    int vcount = (int)(v - P.scratch);

    sg_update_buffer(P.vbuf, &(sg_range){.ptr = P.scratch,
                                         .size = (size_t)vcount * sizeof(pipe_vertex)});

    for (int ri = 0; ri < run_count; ri++) {
        sg_apply_pipeline(P.pip[runs[ri].blend]);
        sg_bindings bnd = {0};
        bnd.vertex_buffers[0] = P.vbuf;
        pipe_tex *t = (pipe_tex *)efx_render_texture_native(runs[ri].texture);
        if (!t) {
            continue;
        }
        bnd.views[0] = t->view;
        bnd.samplers[0] = P.smp;
        sg_apply_bindings(&bnd);
        sg_draw(run_first[ri], run_verts[ri], 1);
    }
    free(run_first);
    free(run_verts);
}

void efx_pipeline_shutdown(void) {
    if (!P.installed) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        sg_destroy_pipeline(P.pip[i]);
    }
    sg_destroy_sampler(P.smp);
    sg_destroy_buffer(P.vbuf);
    sg_destroy_shader(P.shd);
    free(P.scratch);
    P.scratch = NULL;
    P.scratch_cap = 0;
    P.installed = 0;
}
