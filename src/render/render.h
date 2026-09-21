#ifndef EFX_RENDER_H
#define EFX_RENDER_H

/*
 * Internal render module: display list (record -> sort -> playback),
 * 2D camera/projection state, texture registry.
 *
 * Pure C, no sokol, no quickjs (ADR 0003 module walls). GPU work goes
 * through a sink vtable installed by the platform side at startup.
 * Semantics per ADR 0019: frame-transient records, value-snapshot state,
 * handle-referenced textures, stable sort by explicit key.
 */

#include <stddef.h>
#include <stdint.h>

/* blend modes */
#define EFX_BLEND_ALPHA 0
#define EFX_BLEND_ADDITIVE 1
#define EFX_BLEND_SUBTRACTIVE 2

/* error codes */
#define EFX_RENDER_OK 0
#define EFX_RENDER_ERR_BUDGET 1
#define EFX_RENDER_ERR_HANDLE 2      /* unknown texture handle */
#define EFX_RENDER_ERR_PERMANENT 3   /* engine-owned texture (white) */
#define EFX_RENDER_ERR_SINK 4        /* no sink installed */
#define EFX_RENDER_ERR_NOMEM 5

/* hard per-frame record budget (design D4) */
#define EFX_RENDER_RECORD_BUDGET_BYTES (16 * 1024 * 1024)

/* row-major 2D affine: x' = a*x + c*y + tx ; y' = b*x + d*y + ty */
typedef struct efx_affine {
    float a, b, c, d, tx, ty;
} efx_affine;

typedef struct efx_camera2d {
    float frame_w, frame_h; /* virtual frame in px; 0 => default (viewport) */
    float x, y;             /* world point displayed at the frame center */
    float zoom;             /* > 0 */
    float rotation;         /* degrees, clockwise in the y-down frame */
} efx_camera2d;

/* one recorded quad (design D1/D3; ~96 bytes) */
typedef struct efx_quad_record {
    efx_affine m;          /* local -> frame, composed at record time */
    float frame_w, frame_h;
    float w, h;            /* local (destination) size in frame px */
    float sx, sy, sw, sh;  /* source rect in texels */
    float tw, th;          /* texture size in texels */
    float color[4];
    uint64_t texture;      /* texture handle; 0 = white texture */
    uint8_t blend;
    uint32_t sort_key;     /* F2: record index (design D3) */
} efx_quad_record;

/* one batched playback run: consecutive records sharing texture + blend */
typedef struct efx_draw_run {
    int start;      /* index of first record */
    int count;      /* number of records in the run */
    uint64_t texture;
    uint8_t blend;
} efx_draw_run;

/* GPU sink, implemented on the platform (sokol) side. */
typedef struct efx_render_sink {
    void *ud;
    void *(*create_texture)(void *ud, int w, int h, const uint8_t *rgba);
    void (*destroy_texture)(void *ud, void *native);
    void (*play)(void *ud, const efx_quad_record *records, int count);
    void (*shutdown)(void *ud);
} efx_render_sink;

/* lifecycle; installing the sink flushes any texture uploads queued
   before a GPU surface existed (top-level createTexture in main.js) */
void efx_render_install_sink(const efx_render_sink *sink);
void efx_render_shutdown(void);
void efx_render_set_viewport(int w, int h);

/* state setters (value-snapshot: recorded draws never observe later changes) */
void efx_render_reset_state(void); /* blend alpha, clear black, default camera */
void efx_render_set_camera(const efx_camera2d *cam);
void efx_render_set_clear_color(const float rgba[4]);
int efx_render_set_blend(int mode);
void efx_render_clear_color(float out_rgba[4]);

/* textures; handles are opaque, 0 = invalid */
uint64_t efx_render_texture_create(int w, int h, const uint8_t *rgba);
int efx_render_texture_destroy(uint64_t h); /* deferred to frame end */
int efx_render_texture_alive(uint64_t h);
void efx_render_texture_size(uint64_t handle, int *out_w, int *out_h);
void *efx_render_texture_native(uint64_t h); /* valid until end of frame */
uint64_t efx_render_white_texture(void);

/* recording */
int efx_render_quad(float x, float y, float w, float h, uint64_t texture,
                    const float color[4], float rotation_deg, float scale,
                    const float src_rect[4], int has_src);
const efx_quad_record *efx_render_records(int *count);
const efx_draw_run *efx_render_runs(int *count); /* batched playback plan */

/* frame boundaries */
void efx_render_begin_frame(void); /* rewind record arena */
void efx_render_end_frame(void);   /* flush deferred texture destroys */

/* affine/math helpers (exposed for unit tests) */
efx_affine efx_affine_mul(efx_affine f, efx_affine g); /* f(g(p)) */
efx_affine efx_camera_matrix(const efx_camera2d *cam, float fw, float fh);
efx_affine efx_quad_matrix(float x, float y, float w, float h,
                           float rotation_deg, float scale);

#endif
