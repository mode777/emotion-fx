#include "render/render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG2RAD (float)(M_PI / 180.0)

/* ---------------------------------------------------------------- state */

static void flush_pending_uploads(void);

typedef struct {
    int used;
    int alive;
    int permanent;
    uint32_t gen;
    int w, h;
    void *native;
    uint8_t *pending; /* RGBA bytes queued before a sink existed */
} tex_slot;

/* pending mesh upload: interleaved surfaces queued before a sink existed */
typedef struct {
    int count;
    efx_mesh_gpu_surface *surfs; /* count entries; pointers into data */
    uint8_t *data;               /* single backing block */
} mesh_pending;

typedef struct {
    int used;
    int alive;
    uint32_t gen;
    int surface_count;
    void *native;
    mesh_pending pending;
} mesh_slot;

static struct {
    const efx_render_sink *sink;
    int viewport_w, viewport_h;
    efx_camera2d camera;      /* frame_w == 0 => default camera */
    efx_camera3d camera3d;
    float clear_color[4];
    int blend;
    tex_slot *slots;
    int slot_count, slot_cap;
    mesh_slot *meshes;
    int mesh_count, mesh_cap;
    uint64_t white_handle;
    efx_record *records;
    int record_count, record_cap;
    /* deferred texture/mesh destroys (slot indexes) */
    int *deferred_tex;
    int deferred_tex_count, deferred_tex_cap;
    int *deferred_mesh;
    int deferred_mesh_count, deferred_mesh_cap;
} R;

void efx_render_install_sink(const efx_render_sink *sink) {
    R.sink = sink;
    flush_pending_uploads();
}

void efx_render_set_viewport(int w, int h) {
    R.viewport_w = w;
    R.viewport_h = h;
}

void efx_render_viewport(int *out_w, int *out_h) {
    if (out_w) *out_w = R.viewport_w;
    if (out_h) *out_h = R.viewport_h;
}

/* documented defaults apply once, lazily — the script configures state
   at eval time, before the GPU sink exists, and installing the sink must
   not wipe that configuration */
static int state_ready;
static void default_camera(efx_camera2d *cam);
static void default_camera3d(efx_camera3d *cam);

static void ensure_state(void) {
    if (state_ready) {
        return;
    }
    default_camera(&R.camera);
    default_camera3d(&R.camera3d);
    R.clear_color[0] = 0.0f;
    R.clear_color[1] = 0.0f;
    R.clear_color[2] = 0.0f;
    R.clear_color[3] = 1.0f;
    R.blend = EFX_BLEND_ALPHA;
    state_ready = 1;
}

static void default_camera(efx_camera2d *cam) {
    cam->frame_w = 0.0f;
    cam->frame_h = 0.0f;
    cam->x = 0.0f;
    cam->y = 0.0f;
    cam->zoom = 1.0f;
    cam->rotation = 0.0f;
}

static void default_camera3d(efx_camera3d *cam) {
    cam->pos[0] = 0.0f;
    cam->pos[1] = 0.0f;
    cam->pos[2] = 1.0f;
    cam->target[0] = 0.0f;
    cam->target[1] = 0.0f;
    cam->target[2] = 0.0f;
    cam->fov = 60.0f;
    cam->near_z = 0.1f;
    cam->far_z = 100.0f;
}

void efx_render_reset_state(void) {
    default_camera(&R.camera);
    default_camera3d(&R.camera3d);
    R.clear_color[0] = 0.0f;
    R.clear_color[1] = 0.0f;
    R.clear_color[2] = 0.0f;
    R.clear_color[3] = 1.0f;
    R.blend = EFX_BLEND_ALPHA;
}

void efx_render_set_camera(const efx_camera2d *cam) {
    ensure_state();
    if (cam) {
        R.camera = *cam;
    }
}

void efx_render_set_camera3d(const efx_camera3d *cam) {
    ensure_state();
    if (cam) {
        R.camera3d = *cam;
    }
}

void efx_render_camera3d(float out_pos[3], float out_target[3], float *out_fov,
                         float *out_near, float *out_far) {
    ensure_state();
    if (out_pos) {
        out_pos[0] = R.camera3d.pos[0];
        out_pos[1] = R.camera3d.pos[1];
        out_pos[2] = R.camera3d.pos[2];
    }
    if (out_target) {
        out_target[0] = R.camera3d.target[0];
        out_target[1] = R.camera3d.target[1];
        out_target[2] = R.camera3d.target[2];
    }
    if (out_fov) *out_fov = R.camera3d.fov;
    if (out_near) *out_near = R.camera3d.near_z;
    if (out_far) *out_far = R.camera3d.far_z;
}

void efx_render_set_clear_color(const float rgba[4]) {
    ensure_state();
    if (rgba) {
        for (int i = 0; i < 4; i++) {
            R.clear_color[i] = rgba[i];
        }
    }
}

void efx_render_clear_color(float out_rgba[4]) {
    ensure_state();
    for (int i = 0; i < 4; i++) {
        out_rgba[i] = R.clear_color[i];
    }
}

int efx_render_set_blend(int mode) {
    ensure_state();
    if (mode < EFX_BLEND_ALPHA || mode > EFX_BLEND_SUBTRACTIVE) {
        return -1;
    }
    R.blend = mode;
    return 0;
}

/* ------------------------------------------------------------ textures */

static tex_slot *slot_get(uint64_t h) {
    uint32_t idx = (uint32_t)(h & 0xffffffffu);
    uint32_t gen = (uint32_t)(h >> 32);
    if (idx == 0 || (size_t)idx > (size_t)R.slot_count) {
        return NULL;
    }
    tex_slot *s = &R.slots[idx - 1];
    if (!s->used || s->gen != gen) {
        return NULL;
    }
    return s;
}

static mesh_slot *mesh_get(uint64_t h) {
    uint32_t idx = (uint32_t)(h & 0xffffffffu);
    uint32_t gen = (uint32_t)(h >> 32);
    if (idx == 0 || (size_t)idx > (size_t)R.mesh_count) {
        return NULL;
    }
    mesh_slot *m = &R.meshes[idx - 1];
    if (!m->used || m->gen != gen) {
        return NULL;
    }
    return m;
}

static void flush_pending_uploads(void) {
    if (!R.sink || !R.sink->create_texture) {
        return;
    }
    for (int i = 0; i < R.slot_count; i++) {
        tex_slot *s = &R.slots[i];
        if (s->used && s->alive && !s->native && s->pending) {
            s->native = R.sink->create_texture(R.sink->ud, s->w, s->h, s->pending);
            free(s->pending);
            s->pending = NULL;
        }
    }
    if (!R.sink->create_mesh) {
        return;
    }
    for (int i = 0; i < R.mesh_count; i++) {
        mesh_slot *m = &R.meshes[i];
        if (m->used && m->alive && !m->native && m->pending.data) {
            m->native = R.sink->create_mesh(R.sink->ud, m->pending.surfs,
                                            m->pending.count);
            free(m->pending.data);
            free(m->pending.surfs);
            m->pending.data = NULL;
            m->pending.surfs = NULL;
            m->pending.count = 0;
        }
    }
}

uint64_t efx_render_texture_create(int w, int h, const uint8_t *rgba) {
    if (!R.sink || !R.sink->create_texture) {
        /* no GPU surface yet: queue the upload (top-level main.js code) */
        int idx = -1;
        if (R.slot_count >= R.slot_cap) {
            int cap = R.slot_cap ? R.slot_cap * 2 : 64;
            tex_slot *grown = realloc(R.slots, (size_t)cap * sizeof(tex_slot));
            if (!grown) {
                return 0;
            }
            R.slots = grown;
            R.slot_cap = cap;
        }
        tex_slot *s = &R.slots[R.slot_count];
        s->pending = malloc((size_t)w * h * 4);
        if (!s->pending) {
            return 0;
        }
        memcpy(s->pending, rgba, (size_t)w * h * 4);
        s->used = 1;
        s->alive = 1;
        s->permanent = 0;
        s->gen++;
        s->w = w;
        s->h = h;
        s->native = NULL;
        idx = R.slot_count++;
        return ((uint64_t)s->gen << 32) | (uint64_t)(idx + 1);
    }
    void *native = R.sink->create_texture(R.sink->ud, w, h, rgba);
    if (!native) {
        return 0;
    }
    /* find a free slot or grow */
    tex_slot *s = NULL;
    for (int i = 0; i < R.slot_count; i++) {
        if (!R.slots[i].used) {
            s = &R.slots[i];
            break;
        }
    }
    if (!s) {
        if (R.slot_count >= R.slot_cap) {
            int cap = R.slot_cap ? R.slot_cap * 2 : 64;
            tex_slot *grown = realloc(R.slots, (size_t)cap * sizeof(tex_slot));
            if (!grown) {
                R.sink->destroy_texture(R.sink->ud, native);
                return 0;
            }
            R.slots = grown;
            R.slot_cap = cap;
        }
        s = &R.slots[R.slot_count++];
        s->gen = 0;
    }
    s->used = 1;
    s->alive = 1;
    s->permanent = 0;
    s->gen++;
    s->w = w;
    s->h = h;
    s->native = native;
    s->pending = NULL;
    uint32_t idx = (uint32_t)(s - R.slots) + 1;
    return ((uint64_t)s->gen << 32) | (uint64_t)idx;
}

static int texture_release(uint64_t h, tex_slot **out_slot) {
    tex_slot *s = slot_get(h);
    if (!s) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (s->permanent) {
        return EFX_RENDER_ERR_PERMANENT;
    }
    if (!s->alive) {
        return EFX_RENDER_OK; /* destroy() is idempotent */
    }
    s->alive = 0;
    if (s->pending) {
        /* upload never happened; nothing to defer */
        free(s->pending);
        s->pending = NULL;
        return EFX_RENDER_OK;
    }
    /* deferred texture destroys: entries are slot indexes; native release
       happens at end of frame (records may reference the texture until
       playback finishes — js-api resource lifecycle rules) */
    if (R.sink && R.sink->destroy_texture) {
        if (R.deferred_tex_count >= R.deferred_tex_cap) {
            int cap = R.deferred_tex_cap ? R.deferred_tex_cap * 2 : 16;
            int *grown = realloc(R.deferred_tex, (size_t)cap * sizeof(int));
            if (!grown) {
                return EFX_RENDER_ERR_NOMEM;
            }
            R.deferred_tex = grown;
            R.deferred_tex_cap = cap;
        }
        R.deferred_tex[R.deferred_tex_count++] = (int)(s - R.slots);
    }
    if (out_slot) {
        *out_slot = s;
    }
    return EFX_RENDER_OK;
}

int efx_render_texture_destroy(uint64_t h) {
    return texture_release(h, NULL);
}

int efx_render_texture_alive(uint64_t h) {
    tex_slot *s = slot_get(h);
    return s && s->alive;
}

void efx_render_texture_size(uint64_t handle, int *out_w, int *out_h) {
    tex_slot *s = slot_get(handle);
    if (s) {
        if (out_w) *out_w = s->w;
        if (out_h) *out_h = s->h;
    }
}

void *efx_render_texture_native(uint64_t h) {
    tex_slot *s = slot_get(h);
    return s ? s->native : NULL;
}

uint64_t efx_render_white_texture(void) {
    if (R.white_handle) {
        return R.white_handle;
    }
    if (!R.sink || !R.sink->create_texture) {
        return 0;
    }
    static const uint8_t white[4] = {255, 255, 255, 255};
    uint64_t h = efx_render_texture_create(1, 1, white);
    if (h) {
        tex_slot *s = slot_get(h);
        s->permanent = 1;
        R.white_handle = h;
    }
    return h;
}

/* ------------------------------------------------------ CPU mesh data */

efx_meshdata *efx_meshdata_create(const efx_surface_src *src, int count,
                                  int *err) {
    if (err) *err = EFX_MESHERR_OK;
    if (count < 1 || count > EFX_MESH_MAX_SURFACES) {
        if (err) *err = EFX_MESHERR_COUNT;
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        const efx_surface_src *s = &src[i];
        if (s->positions_len <= 0 || s->positions_len % 3 != 0) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        if (s->normals_len % 3 != 0 || s->uvs_len % 2 != 0 ||
            s->colors_len % 4 != 0) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        int vcount = s->positions_len / 3;
        if ((s->normals_len && s->normals_len / 3 != vcount) ||
            (s->uvs_len && s->uvs_len / 2 != vcount) ||
            (s->colors_len && s->colors_len / 4 != vcount)) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        if (s->indices_len % 3 != 0) {
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        if (s->indices_len == 0 && vcount % 3 != 0) {
            /* non-indexed surfaces draw as a triangle list */
            if (err) *err = EFX_MESHERR_LEN;
            return NULL;
        }
        for (int j = 0; j < s->indices_len; j++) {
            if ((size_t)s->indices[j] >= (size_t)vcount) {
                if (err) *err = EFX_MESHERR_INDEX;
                return NULL;
            }
        }
    }

    efx_meshdata *md = calloc(1, sizeof(efx_meshdata));
    if (!md) {
        if (err) *err = EFX_MESHERR_NOMEM;
        return NULL;
    }
    md->surface_count = count;
    md->surfaces = calloc((size_t)count, sizeof(efx_surface));
    if (!md->surfaces) {
        free(md);
        if (err) *err = EFX_MESHERR_NOMEM;
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        const efx_surface_src *s = &src[i];
        efx_surface *d = &md->surfaces[i];
        d->vertex_count = s->positions_len / 3;
        d->index_count = s->indices_len;
        size_t fb = sizeof(float);
        d->positions = malloc((size_t)s->positions_len * fb);
        if (d->positions) memcpy(d->positions, s->positions,
                                 (size_t)s->positions_len * fb);
        if (s->normals_len) {
            d->normals = malloc((size_t)s->normals_len * fb);
            if (d->normals) memcpy(d->normals, s->normals,
                                   (size_t)s->normals_len * fb);
        }
        if (s->uvs_len) {
            d->uvs = malloc((size_t)s->uvs_len * fb);
            if (d->uvs) memcpy(d->uvs, s->uvs, (size_t)s->uvs_len * fb);
        }
        if (s->colors_len) {
            d->colors = malloc((size_t)s->colors_len * fb);
            if (d->colors) memcpy(d->colors, s->colors,
                                  (size_t)s->colors_len * fb);
        }
        if (s->indices_len) {
            d->indices = malloc((size_t)s->indices_len * sizeof(uint32_t));
            if (d->indices) memcpy(d->indices, s->indices,
                                   (size_t)s->indices_len * sizeof(uint32_t));
        }
        int bad = (s->positions_len && !d->positions) ||
                  (s->normals_len && !d->normals) ||
                  (s->uvs_len && !d->uvs) ||
                  (s->colors_len && !d->colors) ||
                  (s->indices_len && !d->indices);
        if (bad) {
            efx_meshdata_destroy(md);
            if (err) *err = EFX_MESHERR_NOMEM;
            return NULL;
        }
    }
    return md;
}

void efx_meshdata_destroy(efx_meshdata *md) {
    if (!md) {
        return;
    }
    if (md->surfaces) {
        for (int i = 0; i < md->surface_count; i++) {
            efx_surface *s = &md->surfaces[i];
            free(s->positions);
            free(s->normals);
            free(s->uvs);
            free(s->colors);
            free(s->indices);
        }
        free(md->surfaces);
    }
    free(md);
}

/* ------------------------------------------------------------- meshes */

/* build the interleaved GPU layout (design D1) into a pending block; the
 * block layout is count · [vcount, icount, interleaved..., indices...] */
static int pending_build(mesh_pending *p, const efx_meshdata *md) {
    if (!md || md->surface_count < 1 ||
        md->surface_count > EFX_MESH_MAX_SURFACES) {
        return EFX_RENDER_ERR_NOMEM;
    }
    size_t floats = 0, indices = 0;
    for (int i = 0; i < md->surface_count; i++) {
        const efx_surface *s = &md->surfaces[i];
        floats += (size_t)s->vertex_count * 12;
        indices += (size_t)s->index_count;
    }
    size_t header = (size_t)md->surface_count * 2;
    size_t total_words = header + floats + indices;
    p->data = malloc(total_words * sizeof(uint32_t));
    p->surfs = calloc((size_t)md->surface_count, sizeof(efx_mesh_gpu_surface));
    if (!p->data || !p->surfs) {
        free(p->data);
        free(p->surfs);
        p->data = NULL;
        p->surfs = NULL;
        return EFX_RENDER_ERR_NOMEM;
    }
    uint32_t *w = (uint32_t *)p->data;
    for (int i = 0; i < md->surface_count; i++) {
        const efx_surface *s = &md->surfaces[i];
        *w++ = (uint32_t)s->vertex_count;
        *w++ = (uint32_t)s->index_count;
        efx_mesh_gpu_surface *g = &p->surfs[i];
        g->vertex_count = s->vertex_count;
        g->index_count = s->index_count;
        g->interleaved = (const float *)w;
        /* defaults: normal +z, uv 0, color white (design D1) */
        for (int v = 0; v < s->vertex_count; v++) {
            float *dst = (float *)w + v * 12;
            dst[0] = s->positions[v * 3];
            dst[1] = s->positions[v * 3 + 1];
            dst[2] = s->positions[v * 3 + 2];
            if (s->normals) {
                dst[3] = s->normals[v * 3];
                dst[4] = s->normals[v * 3 + 1];
                dst[5] = s->normals[v * 3 + 2];
            } else {
                dst[3] = 0.0f;
                dst[4] = 0.0f;
                dst[5] = 1.0f;
            }
            if (s->uvs) {
                dst[6] = s->uvs[v * 2];
                dst[7] = s->uvs[v * 2 + 1];
            } else {
                dst[6] = 0.0f;
                dst[7] = 0.0f;
            }
            if (s->colors) {
                dst[8] = s->colors[v * 4];
                dst[9] = s->colors[v * 4 + 1];
                dst[10] = s->colors[v * 4 + 2];
                dst[11] = s->colors[v * 4 + 3];
            } else {
                dst[8] = 1.0f;
                dst[9] = 1.0f;
                dst[10] = 1.0f;
                dst[11] = 1.0f;
            }
        }
        w += (size_t)s->vertex_count * 12;
        if (s->indices) {
            g->indices = (const uint32_t *)w;
            memcpy(w, s->indices, (size_t)s->index_count * sizeof(uint32_t));
            w += s->index_count;
        } else {
            g->indices = NULL;
        }
    }
    p->count = md->surface_count;
    return EFX_RENDER_OK;
}

static void pending_free(mesh_pending *p) {
    free(p->data);
    free(p->surfs);
    p->data = NULL;
    p->surfs = NULL;
    p->count = 0;
}

uint64_t efx_render_mesh_create(const efx_meshdata *md) {
    if (!md || md->surface_count < 1 ||
        md->surface_count > EFX_MESH_MAX_SURFACES) {
        return 0;
    }
    if (R.mesh_count >= R.mesh_cap) {
        int cap = R.mesh_cap ? R.mesh_cap * 2 : 16;
        mesh_slot *grown = realloc(R.meshes, (size_t)cap * sizeof(mesh_slot));
        if (!grown) {
            return 0;
        }
        R.meshes = grown;
        R.mesh_cap = cap;
    }
    mesh_slot *m = &R.meshes[R.mesh_count];
    memset(m, 0, sizeof(*m));
    m->used = 1;
    m->alive = 1;
    m->gen++;
    m->surface_count = md->surface_count;
    if (R.sink && R.sink->create_mesh) {
        mesh_pending tmp = {0, NULL, NULL};
        if (pending_build(&tmp, md) != EFX_RENDER_OK) {
            return 0;
        }
        m->native = R.sink->create_mesh(R.sink->ud, tmp.surfs, tmp.count);
        pending_free(&tmp);
        if (!m->native) {
            return 0;
        }
    } else {
        /* queue the upload (top-level main.js code, headless scripts) */
        if (pending_build(&m->pending, md) != EFX_RENDER_OK) {
            m->pending.data = NULL;
            return 0;
        }
    }
    uint32_t idx = (uint32_t)R.mesh_count + 1;
    R.mesh_count++;
    return ((uint64_t)m->gen << 32) | (uint64_t)idx;
}

static int mesh_release(uint64_t h, mesh_slot **out) {
    mesh_slot *m = mesh_get(h);
    if (!m) {
        return EFX_RENDER_ERR_HANDLE;
    }
    if (!m->alive) {
        return EFX_RENDER_OK; /* idempotent */
    }
    m->alive = 0;
    if (m->pending.data) {
        pending_free(&m->pending);
        return EFX_RENDER_OK;
    }
    /* deferred native release at frame end (resource lifecycle rules) */
    if (R.sink && R.sink->destroy_mesh) {
        if (R.deferred_mesh_count >= R.deferred_mesh_cap) {
            int cap = R.deferred_mesh_cap ? R.deferred_mesh_cap * 2 : 16;
            int *grown = realloc(R.deferred_mesh, (size_t)cap * sizeof(int));
            if (!grown) {
                return EFX_RENDER_ERR_NOMEM;
            }
            R.deferred_mesh = grown;
            R.deferred_mesh_cap = cap;
        }
        R.deferred_mesh[R.deferred_mesh_count++] = (int)(m - R.meshes);
    }
    if (out) {
        *out = m;
    }
    return EFX_RENDER_OK;
}

int efx_render_mesh_destroy(uint64_t h) {
    return mesh_release(h, NULL);
}

int efx_render_mesh_alive(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return m && m->alive;
}

int efx_render_mesh_surface_count(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return m && m->alive ? m->surface_count : -1;
}

void *efx_render_mesh_native(uint64_t h) {
    mesh_slot *m = mesh_get(h);
    return m ? m->native : NULL;
}

/* -------------------------------------------------------------- affine */

efx_affine efx_affine_mul(efx_affine f, efx_affine g) {
    efx_affine r;
    r.a = f.a * g.a + f.c * g.b;
    r.b = f.b * g.a + f.d * g.b;
    r.c = f.a * g.c + f.c * g.d;
    r.d = f.b * g.c + f.d * g.d;
    r.tx = f.a * g.tx + f.c * g.ty + f.tx;
    r.ty = f.b * g.tx + f.d * g.ty + f.ty;
    return r;
}

/* world -> frame; zoom and rotation pivot on the frame center (x,y is the
 * world point shown at the frame center). Standard rotation matrices read
 * as clockwise in the y-down frame. */
efx_affine efx_camera_matrix(const efx_camera2d *cam, float fw, float fh) {
    float t = cam->rotation * DEG2RAD;
    float cs = cosf(t) * cam->zoom;
    float sn = sinf(t) * cam->zoom;
    efx_affine v;
    v.a = cs;
    v.b = sn;
    v.c = -sn;
    v.d = cs;
    float cx = fw * 0.5f;
    float cy = fh * 0.5f;
    v.tx = cx - (v.a * cam->x + v.c * cam->y);
    v.ty = cy - (v.b * cam->x + v.d * cam->y);
    return v;
}

/* local -> world; corner-anchored placement, rotation/scale pivot on the
 * caller-provided point (quad-local, relative to the quad top-left; the
 * legacy center pivot is origin = size/2) */
efx_affine efx_quad_matrix(float x, float y,
                           float origin_x, float origin_y,
                           float rotation_deg, float scale) {
    float t = rotation_deg * DEG2RAD;
    float cs = cosf(t) * scale;
    float sn = sinf(t) * scale;
    efx_affine m;
    m.a = cs;
    m.b = sn;
    m.c = -sn;
    m.d = cs;
    float px = x + origin_x;
    float py = y + origin_y;
    m.tx = px - (m.a * origin_x + m.c * origin_y);
    m.ty = py - (m.b * origin_x + m.d * origin_y);
    return m;
}

/* ------------------------------------------------------------- records */

static int record_push(efx_record rec, size_t bytes) {
    if ((size_t)(R.record_count + 1) * bytes > EFX_RENDER_RECORD_BUDGET_BYTES) {
        return EFX_RENDER_ERR_BUDGET;
    }
    if (R.record_count >= R.record_cap) {
        int cap = R.record_cap ? R.record_cap * 2 : 256;
        efx_record *grown = realloc(R.records, (size_t)cap * sizeof(efx_record));
        if (!grown) {
            return EFX_RENDER_ERR_NOMEM;
        }
        R.records = grown;
        R.record_cap = cap;
    }
    R.records[R.record_count++] = rec;
    return EFX_RENDER_OK;
}

int efx_render_quad(float x, float y, float w, float h, uint64_t texture,
                    const float color[4], float rotation_deg, float scale,
                    const float src_rect[4], int has_src,
                    float origin_x, float origin_y) {
    ensure_state();
    float fw = R.camera.frame_w > 0.0f ? R.camera.frame_w
                                       : (float)(R.viewport_w ? R.viewport_w : 640);
    float fh = R.camera.frame_h > 0.0f ? R.camera.frame_h
                                       : (float)(R.viewport_h ? R.viewport_h : 480);
    float cx = R.camera.frame_w > 0.0f ? R.camera.x : fw * 0.5f;
    float cy = R.camera.frame_h > 0.0f ? R.camera.y : fh * 0.5f;

    efx_camera2d cam = R.camera;
    cam.x = cx;
    cam.y = cy;
    efx_affine view = efx_camera_matrix(&cam, fw, fh);
    efx_affine model = efx_quad_matrix(x, y, origin_x, origin_y,
                                       rotation_deg, scale);

    efx_record rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = EFX_RECORD_QUAD;
    efx_quad_record *q = &rec.u.quad;
    q->m = efx_affine_mul(view, model);
    q->frame_w = fw;
    q->frame_h = fh;
    q->w = w;
    q->h = h;

    if (!texture) {
        texture = efx_render_white_texture();
        if (!texture) {
            return EFX_RENDER_ERR_SINK;
        }
    }
    q->texture = texture;
    {
        int tw = 0, th = 0;
        efx_render_texture_size(texture, &tw, &th);
        q->tw = (float)tw;
        q->th = (float)th;
    }

    if (has_src) {
        q->sx = src_rect[0];
        q->sy = src_rect[1];
        q->sw = src_rect[2];
        q->sh = src_rect[3];
    } else {
        q->sx = 0.0f;
        q->sy = 0.0f;
        q->sw = (float)q->tw;
        q->sh = (float)q->th;
    }
    for (int i = 0; i < 4; i++) {
        q->color[i] = color ? color[i] : 1.0f;
    }
    q->blend = (uint8_t)R.blend;
    /* sort key: record index — playback order equals record order in F2
       (design D3); the stable sort below generalizes when keys change */
    rec.sort_key = (uint32_t)R.record_count;

    return record_push(rec, sizeof(efx_record));
}

int efx_render_mesh(uint64_t mesh, const float transform[16],
                    const float color[4]) {
    ensure_state();
    if (!mesh || !efx_render_mesh_alive(mesh)) {
        return EFX_RENDER_ERR_HANDLE;
    }
    efx_record rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = EFX_RECORD_MESH;
    efx_mesh_record *mr = &rec.u.mesh;
    mr->mesh = mesh;
    for (int i = 0; i < 16; i++) {
        mr->transform[i] = transform ? transform[i] : 0.0f;
    }
    if (!transform) {
        mr->transform[0] = 1.0f;
        mr->transform[5] = 1.0f;
        mr->transform[10] = 1.0f;
        mr->transform[15] = 1.0f;
    }
    for (int i = 0; i < 4; i++) {
        mr->color[i] = color ? color[i] : 1.0f;
    }
    mr->camera = R.camera3d;
    mr->blend = (uint8_t)R.blend;
    rec.sort_key = (uint32_t)R.record_count;
    return record_push(rec, sizeof(efx_record));
}

const efx_record *efx_render_records(int *count) {
    if (count) {
        *count = R.record_count;
    }
    return R.records;
}

const efx_draw_run *efx_render_runs(int *count) {
    /* batch consecutive same-texture, same-blend quad records (design
       D3/D10); mesh records break runs */
    static efx_draw_run *runs = NULL;
    static int run_cap = 0;
    int n = 0;
    for (int i = 0; i < R.record_count; i++) {
        efx_record *r = &R.records[i];
        if (r->type != EFX_RECORD_QUAD) {
            continue;
        }
        if (n > 0 && runs[n - 1].texture == r->u.quad.texture &&
            runs[n - 1].blend == r->u.quad.blend &&
            runs[n - 1].start + runs[n - 1].count == i) {
            runs[n - 1].count++;
        } else {
            if (n >= run_cap) {
                int cap = run_cap ? run_cap * 2 : 64;
                efx_draw_run *grown = realloc(runs, (size_t)cap * sizeof(efx_draw_run));
                if (!grown) {
                    if (count) *count = 0;
                    return NULL;
                }
                runs = grown;
                run_cap = cap;
            }
            runs[n].start = i;
            runs[n].count = 1;
            runs[n].texture = r->u.quad.texture;
            runs[n].blend = r->u.quad.blend;
            n++;
        }
    }
    if (count) {
        *count = n;
    }
    return runs;
}

void efx_render_begin_frame(void) {
    ensure_state();
    R.record_count = 0;
}

void efx_render_end_frame(void) {
    if (R.sink && R.sink->destroy_texture) {
        for (int i = 0; i < R.deferred_tex_count; i++) {
            int idx = R.deferred_tex[i];
            R.sink->destroy_texture(R.sink->ud, R.slots[idx].native);
            R.slots[idx].native = NULL;
            R.slots[idx].used = 0;
        }
    }
    R.deferred_tex_count = 0;
    if (R.sink && R.sink->destroy_mesh) {
        for (int i = 0; i < R.deferred_mesh_count; i++) {
            int idx = R.deferred_mesh[i];
            R.sink->destroy_mesh(R.sink->ud, R.meshes[idx].native);
            R.meshes[idx].native = NULL;
            R.meshes[idx].used = 0;
        }
    }
    R.deferred_mesh_count = 0;
}

/* ------------------------------------------------------------- shutdown */

void efx_render_shutdown(void) {
    if (R.sink) {
        for (int i = 0; i < R.slot_count; i++) {
            if (R.slots[i].used && R.slots[i].native) {
                R.sink->destroy_texture(R.sink->ud, R.slots[i].native);
            }
            free(R.slots[i].pending);
        }
        for (int i = 0; i < R.mesh_count; i++) {
            if (R.meshes[i].used && R.meshes[i].native &&
                R.sink->destroy_mesh) {
                R.sink->destroy_mesh(R.sink->ud, R.meshes[i].native);
            }
            pending_free(&R.meshes[i].pending);
        }
        if (R.sink->shutdown) {
            R.sink->shutdown(R.sink->ud);
        }
    }
    free(R.slots);
    free(R.meshes);
    free(R.records);
    free(R.deferred_tex);
    free(R.deferred_mesh);
    memset(&R, 0, sizeof(R));
    state_ready = 0;
}
