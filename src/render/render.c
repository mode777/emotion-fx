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

static struct {
    const efx_render_sink *sink;
    int viewport_w, viewport_h;
    efx_camera2d camera;      /* frame_w == 0 => default camera */
    float clear_color[4];
    int blend;
    tex_slot *slots;
    int slot_count, slot_cap;
    uint64_t white_handle;
    efx_quad_record *records;
    int record_count, record_cap;
    size_t record_bytes;
    /* deferred texture destroys (slot indexes) */
    int *deferred;
    int deferred_count, deferred_cap;
} R;

void efx_render_install_sink(const efx_render_sink *sink) {
    R.sink = sink;
    flush_pending_uploads();
}

void efx_render_set_viewport(int w, int h) {
    R.viewport_w = w;
    R.viewport_h = h;
}

/* documented defaults apply once, lazily — the script configures state
   at eval time, before the GPU sink exists, and installing the sink must
   not wipe that configuration */
static int state_ready;
static void default_camera(efx_camera2d *cam);

static void ensure_state(void) {
    if (state_ready) {
        return;
    }
    default_camera(&R.camera);
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

void efx_render_reset_state(void) {
    default_camera(&R.camera);
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
        if (R.deferred_count >= R.deferred_cap) {
            int cap = R.deferred_cap ? R.deferred_cap * 2 : 16;
            int *grown = realloc(R.deferred, (size_t)cap * sizeof(int));
            if (!grown) {
                return EFX_RENDER_ERR_NOMEM;
            }
            R.deferred = grown;
            R.deferred_cap = cap;
        }
        R.deferred[R.deferred_count++] = (int)(s - R.slots);
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

static int record_push(efx_quad_record rec) {
    if ((size_t)(R.record_count + 1) * sizeof(efx_quad_record) >
        EFX_RENDER_RECORD_BUDGET_BYTES) {
        return EFX_RENDER_ERR_BUDGET;
    }
    if (R.record_count >= R.record_cap) {
        int cap = R.record_cap ? R.record_cap * 2 : 256;
        efx_quad_record *grown =
            realloc(R.records, (size_t)cap * sizeof(efx_quad_record));
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

    efx_quad_record rec;
    rec.m = efx_affine_mul(view, model);
    rec.frame_w = fw;
    rec.frame_h = fh;
    rec.w = w;
    rec.h = h;

    if (!texture) {
        texture = efx_render_white_texture();
        if (!texture) {
            return EFX_RENDER_ERR_SINK;
        }
    }
    rec.texture = texture;
    {
        int tw = 0, th = 0;
        efx_render_texture_size(texture, &tw, &th);
        rec.tw = (float)tw;
        rec.th = (float)th;
    }

    if (has_src) {
        rec.sx = src_rect[0];
        rec.sy = src_rect[1];
        rec.sw = src_rect[2];
        rec.sh = src_rect[3];
    } else {
        rec.sx = 0.0f;
        rec.sy = 0.0f;
        rec.sw = (float)rec.tw;
        rec.sh = (float)rec.th;
    }
    for (int i = 0; i < 4; i++) {
        rec.color[i] = color ? color[i] : 1.0f;
    }
    rec.blend = (uint8_t)R.blend;
    /* sort key: record index — playback order equals record order in F2
       (design D3); the stable sort below generalizes when keys change */
    rec.sort_key = (uint32_t)R.record_count;

    return record_push(rec);
}

const efx_quad_record *efx_render_records(int *count) {
    if (count) {
        *count = R.record_count;
    }
    return R.records;
}

const efx_draw_run *efx_render_runs(int *count) {
    /* batch consecutive same-texture, same-blend records (design D3/D10) */
    static efx_draw_run *runs = NULL;
    static int run_cap = 0;
    int n = 0;
    for (int i = 0; i < R.record_count; i++) {
        efx_quad_record *r = &R.records[i];
        if (n > 0 && runs[n - 1].texture == r->texture &&
            runs[n - 1].blend == r->blend) {
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
            runs[n].texture = r->texture;
            runs[n].blend = r->blend;
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
    R.record_bytes = 0;
}

void efx_render_end_frame(void) {
    if (R.sink && R.sink->destroy_texture) {
        for (int i = 0; i < R.deferred_count; i++) {
            int idx = R.deferred[i];
            R.sink->destroy_texture(R.sink->ud, R.slots[idx].native);
            R.slots[idx].native = NULL;
            R.slots[idx].used = 0;
        }
    }
    R.deferred_count = 0;
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
        if (R.sink->shutdown) {
            R.sink->shutdown(R.sink->ud);
        }
    }
    free(R.slots);
    free(R.records);
    free(R.deferred);
    memset(&R, 0, sizeof(R));
    state_ready = 0;
}
