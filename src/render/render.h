#ifndef EFX_RENDER_H
#define EFX_RENDER_H

/*
 * Internal render module: display list (record -> sort -> playback),
 * 2D/3D camera state, texture and mesh registries, CPU mesh data.
 *
 * Pure C, no sokol, no quickjs (ADR 0003 module walls). GPU work goes
 * through a sink vtable installed by the platform side at startup.
 * Semantics per ADR 0019: frame-transient records, value-snapshot state,
 * handle-referenced resources, stable sort by explicit key.
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
#define EFX_RENDER_ERR_HANDLE 2      /* unknown texture/mesh handle */
#define EFX_RENDER_ERR_PERMANENT 3   /* engine-owned resource */
#define EFX_RENDER_ERR_SINK 4        /* no sink installed */
#define EFX_RENDER_ERR_NOMEM 5

/* mesh data validation failures (range-level; type-level errors are
 * reported by the binding while extracting JS values) */
#define EFX_MESHERR_OK 0
#define EFX_MESHERR_COUNT 1    /* surface count outside 1..EFX_MESH_MAX_SURFACES */
#define EFX_MESHERR_LEN 2      /* array lengths: multiples / attribute mismatch */
#define EFX_MESHERR_INDEX 3    /* index out of vertex range */
#define EFX_MESHERR_NOMEM 4

/* fixed limit: surfaces per mesh (vision.md fixed limits, F3) */
#define EFX_MESH_MAX_SURFACES 16

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

typedef struct efx_camera3d {
    float pos[3];
    float target[3];
    float fov;              /* vertical, degrees */
    float near_z, far_z;
} efx_camera3d;

/* one quad (design D1/D3; ~96 bytes) */
typedef struct efx_quad_record {
    efx_affine m;          /* local -> frame, composed at record time */
    float frame_w, frame_h;
    float w, h;            /* local (destination) size in frame px */
    float sx, sy, sw, sh;  /* source rect in texels */
    float tw, th;          /* texture size in texels */
    float color[4];
    uint64_t texture;      /* texture handle; 0 = white texture */
    uint8_t blend;
} efx_quad_record;

/* one whole-mesh draw (design D7): every surface plays back in surface
 * order, depth-tested; the camera is value-snapshotted at record time */
typedef struct efx_mesh_record {
    uint64_t mesh;
    float transform[16];   /* column-major model matrix */
    float color[4];        /* tint */
    efx_camera3d camera;
    uint8_t blend;
} efx_mesh_record;

#define EFX_RECORD_QUAD 0
#define EFX_RECORD_MESH 1

/* one display-list record; sort key = record index (F2: playback order
 * equals record order, design D3) */
typedef struct efx_record {
    uint8_t type;
    uint32_t sort_key;
    union {
        efx_quad_record quad;
        efx_mesh_record mesh;
    } u;
} efx_record;

/* one batched quad playback run: consecutive quad records sharing
 * texture + blend (mesh records break runs; design D10) */
typedef struct efx_draw_run {
    int start;      /* index of first record */
    int count;      /* number of records in the run */
    uint64_t texture;
    uint8_t blend;
} efx_draw_run;

/* CPU mesh data: 1..EFX_MESH_MAX_SURFACES surfaces, each with its own
 * attribute arrays + optional indices (Godot surface / glTF primitive).
 * Storage is deep-copied and engine-owned (design D8). */
typedef struct efx_surface_src {
    int positions_len;   /* floats, %3 == 0, > 0 */
    int normals_len;     /* floats, %3 == 0, 0 = absent */
    int uvs_len;         /* floats, %2 == 0, 0 = absent */
    int colors_len;      /* floats, %4 == 0, 0 = absent */
    int indices_len;     /* uint32 count, %3 == 0, 0 = non-indexed */
    const float *positions, *normals, *uvs, *colors;
    const uint32_t *indices;
} efx_surface_src;

typedef struct efx_surface {
    int vertex_count, index_count;
    float *positions, *normals, *uvs, *colors; /* NULL when absent */
    uint32_t *indices;                          /* NULL when non-indexed */
} efx_surface;

typedef struct efx_meshdata {
    efx_surface *surfaces;
    int surface_count;
} efx_meshdata;

/* validates + deep-copies; NULL + one of the EFX_MESHERR_* codes */
efx_meshdata *efx_meshdata_create(const efx_surface_src *src, int count,
                                  int *err);
void efx_meshdata_destroy(efx_meshdata *md); /* idempotent, NULL safe */

/* one GPU surface as handed to the sink: vertices interleaved
 * pos(3f) normal(3f) uv(2f) color(4f) = 12 floats/vertex (design D1;
 * absent attributes are filled with deterministic defaults) */
typedef struct efx_mesh_gpu_surface {
    int vertex_count, index_count;
    const float *interleaved;
    const uint32_t *indices; /* NULL when non-indexed */
} efx_mesh_gpu_surface;

/* GPU sink, implemented on the platform (sokol) side. create_mesh may
 * return NULL on failure; native handles are owned by the sink side. */
typedef struct efx_render_sink {
    void *ud;
    void *(*create_texture)(void *ud, int w, int h, const uint8_t *rgba);
    void (*destroy_texture)(void *ud, void *native);
    void *(*create_mesh)(void *ud, const efx_mesh_gpu_surface *surfaces,
                         int count);
    void (*destroy_mesh)(void *ud, void *native);
    void (*play)(void *ud, const efx_quad_record *records, int count);
    void (*shutdown)(void *ud);
} efx_render_sink;

/* lifecycle; installing the sink flushes any uploads queued before a GPU
   surface existed (top-level createTexture/createMesh in main.js) */
void efx_render_install_sink(const efx_render_sink *sink);
void efx_render_shutdown(void);
void efx_render_set_viewport(int w, int h);
void efx_render_viewport(int *out_w, int *out_h);

/* state setters (value-snapshot: recorded draws never observe later changes) */
void efx_render_reset_state(void); /* blend alpha, clear black, default cameras */
void efx_render_set_camera(const efx_camera2d *cam);
void efx_render_set_camera3d(const efx_camera3d *cam);
void efx_render_set_clear_color(const float rgba[4]);
int efx_render_set_blend(int mode);
void efx_render_clear_color(float out_rgba[4]);
void efx_render_camera3d(float out_pos[3], float out_target[3], float *out_fov,
                         float *out_near, float *out_far);

/* textures; handles are opaque, 0 = invalid */
uint64_t efx_render_texture_create(int w, int h, const uint8_t *rgba);
int efx_render_texture_destroy(uint64_t h); /* deferred to frame end */
int efx_render_texture_alive(uint64_t h);
void efx_render_texture_size(uint64_t handle, int *out_w, int *out_h);
void *efx_render_texture_native(uint64_t h); /* valid until end of frame */
uint64_t efx_render_white_texture(void);

/* meshes; handles are opaque, 0 = invalid; destroy is deferred to frame
 * end (records may reference the mesh until playback finishes) */
uint64_t efx_render_mesh_create(const efx_meshdata *md);
int efx_render_mesh_destroy(uint64_t h);
int efx_render_mesh_alive(uint64_t h);
int efx_render_mesh_surface_count(uint64_t h);
void *efx_render_mesh_native(uint64_t h);

/* recording */
int efx_render_quad(float x, float y, float w, float h, uint64_t texture,
                    const float color[4], float rotation_deg, float scale,
                    const float src_rect[4], int has_src,
                    float origin_x, float origin_y);
int efx_render_mesh(uint64_t mesh, const float transform[16],
                    const float color[4]);
const efx_record *efx_render_records(int *count);
const efx_draw_run *efx_render_runs(int *count); /* batched quad plan */

/* frame boundaries */
void efx_render_begin_frame(void); /* rewind record arena */
void efx_render_end_frame(void);   /* flush deferred destroys */

/* affine/math helpers (exposed for unit tests) */
efx_affine efx_affine_mul(efx_affine f, efx_affine g); /* f(g(p)) */
efx_affine efx_camera_matrix(const efx_camera2d *cam, float fw, float fh);
efx_affine efx_quad_matrix(float x, float y,
                           float origin_x, float origin_y,
                           float rotation_deg, float scale);

#endif
