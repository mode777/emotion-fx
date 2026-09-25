#include <emscripten.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/platform.h"
#include "render/render.h"
#include "prelude/prelude.h"
#include "web/web.h"

#define EFX_WEB_ROOT_MAX 512

static struct {
    int quit_requested;
    int quit_code;
    int in_error;
    char **args;
    int arg_count;
    char root[EFX_WEB_ROOT_MAX];
    int dom;
    int golden_mode;
    efx_platform_capture capture;
    double frame_last_now;
    int frame_have_now;
} W;

EM_JS(int, efx_web_has_dom_js, (void), {
    return (typeof document !== 'undefined') ? 1 : 0;
});

EM_JS(int, efx_web_call_hook_js, (int which, double dt), {
    return globalThis.__efxDispatchHook(which, dt);
});

EM_JS(void, efx_web_publish_exit, (int code), {
    Module['efxExitCode'] = code;
    Module['efxRunEnded'] = true;
});

static char *dup_string(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) {
        memcpy(p, s, n);
    }
    return p;
}

static void set_args(char *const *args, int count) {
    if (count > 0 && args) {
        W.args = calloc((size_t)count, sizeof(char *));
        if (!W.args) {
            return;
        }
        for (int i = 0; i < count; i++) {
            W.args[i] = dup_string(args[i]);
        }
        W.arg_count = count;
    }
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_quit(int code) {
    W.quit_requested = 1;
    W.quit_code = code;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_error(void) {
    W.in_error = 1;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_fail(void) {
    W.in_error = 1;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_exit_code(void) {
    if (W.in_error) {
        return 1;
    }
    if (W.quit_requested) {
        return W.quit_code;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_arg_count(void) {
    return W.arg_count;
}

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_arg(int i) {
    if (i < 0 || i >= W.arg_count || !W.args) {
        return "";
    }
    return W.args[i];
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_clear_color(float r, float g, float b, float a) {
    float c[4] = {r, g, b, a};
    efx_render_set_clear_color(c);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_camera(float fw, float fh, float x, float y,
                                                float zoom, float rotation) {
    efx_camera2d cam;
    memset(&cam, 0, sizeof(cam));
    cam.frame_w = fw;
    cam.frame_h = fh;
    cam.x = x;
    cam.y = y;
    cam.zoom = zoom;
    cam.rotation = rotation;
    efx_render_set_camera(&cam);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_blend(int mode) {
    efx_render_set_blend(mode);
}

typedef struct {
    uint8_t *pixels;
    int w, h;
    int alive;
} img_slot;

static struct {
    img_slot *slots;
    int count, cap;
} IMG;

static img_slot *img_get(int id) {
    if (id <= 0 || id > IMG.count) {
        return NULL;
    }
    return &IMG.slots[id - 1];
}

EMSCRIPTEN_KEEPALIVE uint8_t *efx_bridge_imagedata_alloc(int bytes) {
    if (bytes <= 0) {
        return NULL;
    }
    return (uint8_t *)malloc((size_t)bytes);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_mem_free(void *p) {
    free(p);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_imagedata_commit(int w, int h, uint8_t *pixels) {
    if (IMG.count >= IMG.cap) {
        int cap = IMG.cap ? IMG.cap * 2 : 64;
        img_slot *grown = realloc(IMG.slots, (size_t)cap * sizeof(img_slot));
        if (!grown) {
            return 0;
        }
        IMG.slots = grown;
        IMG.cap = cap;
    }
    img_slot *s = &IMG.slots[IMG.count];
    memset(s, 0, sizeof(*s));
    s->pixels = pixels;
    s->w = w;
    s->h = h;
    s->alive = 1;
    IMG.count++;
    return IMG.count; /* 1-based id; never reused (wrapper identity stays valid) */
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_imagedata_destroy(int id) {
    img_slot *s = img_get(id);
    if (!s || !s->alive) {
        return;
    }
    s->alive = 0;
    free(s->pixels);
    s->pixels = NULL;
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_texture_create(int id) {
    img_slot *s = img_get(id);
    if (!s || !s->alive) {
        return 0;
    }
    return (double)efx_render_texture_create(s->w, s->h, s->pixels);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_texture_destroy(double handle) {
    efx_render_texture_destroy((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_texture_alive(double handle) {
    return efx_render_texture_alive((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_texture_width(double handle) {
    int w = 0, h = 0;
    efx_render_texture_size((uint64_t)handle, &w, &h);
    return w;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_texture_height(double handle) {
    int w = 0, h = 0;
    efx_render_texture_size((uint64_t)handle, &w, &h);
    return h;
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_white_texture(void) {
    return (double)efx_render_white_texture();
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_draw_quad(double handle, float x, float y, float w, float h,
                                              float cr, float cg, float cb, float ca,
                                              float rotation, float scale,
                                              float sx, float sy, float sw, float sh,
                                              int has_src, float origin_x, float origin_y) {
    float color[4] = {cr, cg, cb, ca};
    float src[4] = {sx, sy, sw, sh};
    return efx_render_quad(x, y, w, h, (uint64_t)handle, color, rotation, scale,
                           src, has_src, origin_x, origin_y);
}

/* ------------------------------------------------- F3 (3D core) */

EMSCRIPTEN_KEEPALIVE const char *efx_bridge_js_prelude(void) {
    return EFX_JS_PRELUDE; /* NUL-terminated; EFX_JS_PRELUDE_LEN bytes */
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_set_camera3d(float px, float py, float pz,
                                                  float tx, float ty, float tz,
                                                  float fov, float near_z,
                                                  float far_z) {
    efx_camera3d cam;
    memset(&cam, 0, sizeof(cam));
    cam.pos[0] = px;
    cam.pos[1] = py;
    cam.pos[2] = pz;
    cam.target[0] = tx;
    cam.target[1] = ty;
    cam.target[2] = tz;
    cam.fov = fov;
    cam.near_z = near_z;
    cam.far_z = far_z;
    efx_render_set_camera3d(&cam);
}

/* MeshData slot table (mirrors the desktop api.c wrapper ownership).
   Surfaces arrive one at a time from JS: they are validated as a probe
   MeshData, then their storage moves into the staging array; commit
   assembles the final efx_meshdata. */
typedef struct {
    efx_surface *staged; /* surface_count entries; storage NULL until filled */
    int filled;
    efx_meshdata *md;    /* set by commit */
    int alive;
    int surface_count;
} wmd_slot;

static struct {
    wmd_slot *slots;
    int count, cap;
} WMD;

static wmd_slot *wmd_get(int id) {
    if (id <= 0 || id > WMD.count) {
        return NULL;
    }
    return &WMD.slots[id - 1];
}

static void wmd_release(wmd_slot *s) {
    if (s->staged) {
        for (int i = 0; i < s->surface_count; i++) {
            efx_surface *sf = &s->staged[i];
            free(sf->positions);
            free(sf->normals);
            free(sf->uvs);
            free(sf->colors);
            free(sf->indices);
        }
        free(s->staged);
        s->staged = NULL;
    }
    efx_meshdata_destroy(s->md);
    s->md = NULL;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_create(int surface_count) {
    if (surface_count < 1 || surface_count > EFX_MESH_MAX_SURFACES) {
        return 0;
    }
    if (WMD.count >= WMD.cap) {
        int cap = WMD.cap ? WMD.cap * 2 : 16;
        wmd_slot *grown = realloc(WMD.slots, (size_t)cap * sizeof(wmd_slot));
        if (!grown) {
            return 0;
        }
        WMD.slots = grown;
        WMD.cap = cap;
    }
    wmd_slot *s = &WMD.slots[WMD.count];
    memset(s, 0, sizeof(*s));
    s->staged = calloc((size_t)surface_count, sizeof(efx_surface));
    if (!s->staged) {
        return 0;
    }
    s->surface_count = surface_count;
    s->alive = 1;
    WMD.count++;
    return WMD.count;
}

/* fill one staged surface; returns 0 ok, EFX_MESHERR_* on validation
   failure (the staged MeshData is released; the JS layer throws) */
EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_surface(int id, int index,
                                                     const float *positions,
                                                     int positions_len,
                                                     const float *normals,
                                                     int normals_len,
                                                     const float *uvs,
                                                     int uvs_len,
                                                     const float *colors,
                                                     int colors_len,
                                                     const uint32_t *indices,
                                                     int indices_len) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || index < 0 || index >= s->surface_count) {
        return EFX_MESHERR_COUNT;
    }
    efx_surface_src src;
    memset(&src, 0, sizeof(src));
    src.positions = positions;
    src.positions_len = positions_len;
    src.normals = normals;
    src.normals_len = normals_len;
    src.uvs = uvs;
    src.uvs_len = uvs_len;
    src.colors = colors;
    src.colors_len = colors_len;
    src.indices = indices;
    src.indices_len = indices_len;
    int err = 0;
    efx_meshdata *probe = efx_meshdata_create(&src, 1, &err);
    if (!probe) {
        wmd_release(s);
        s->alive = 0;
        return err;
    }
    /* transfer the validated surface storage into the staging slot */
    s->staged[index] = probe->surfaces[0];
    free(probe->surfaces);
    free(probe);
    s->filled++;
    return EFX_MESHERR_OK;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_surface_count(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || !s->md) {
        return -1;
    }
    return s->md->surface_count;
}

/* assemble the final MeshData; returns 0 ok, EFX_MESHERR_* on failure */
EMSCRIPTEN_KEEPALIVE int efx_bridge_meshdata_commit(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || !s->staged) {
        return EFX_MESHERR_COUNT;
    }
    if (s->filled != s->surface_count) {
        wmd_release(s);
        s->alive = 0;
        return EFX_MESHERR_LEN;
    }
    s->md = malloc(sizeof(efx_meshdata));
    if (!s->md) {
        wmd_release(s);
        s->alive = 0;
        return EFX_MESHERR_NOMEM;
    }
    s->md->surface_count = s->surface_count;
    s->md->surfaces = s->staged;
    s->staged = NULL; /* ownership moved into the meshdata */
    return EFX_MESHERR_OK;
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_meshdata_destroy(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive) {
        return;
    }
    s->alive = 0;
    wmd_release(s);
}

EMSCRIPTEN_KEEPALIVE double efx_bridge_mesh_create(int id) {
    wmd_slot *s = wmd_get(id);
    if (!s || !s->alive || !s->md) {
        return 0;
    }
    return (double)efx_render_mesh_create(s->md);
}

EMSCRIPTEN_KEEPALIVE void efx_bridge_mesh_destroy(double handle) {
    efx_render_mesh_destroy((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_mesh_alive(double handle) {
    return efx_render_mesh_alive((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_mesh_surface_count(double handle) {
    return efx_render_mesh_surface_count((uint64_t)handle);
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_draw_mesh(double handle,
                                              const float *transform,
                                              const float *color) {
    return efx_render_mesh((uint64_t)handle, transform, color);
}

static int web_frame(void *ud, double dt) {
    (void)ud;
    int stop = 0;
    if (W.quit_requested || W.in_error) {
        stop = 1;
    } else if (efx_web_call_hook_js(1, dt) != 0) {
        stop = 1;
    } else if (W.quit_requested || W.in_error) {
        stop = 1;
    } else if (efx_web_call_hook_js(0, dt) != 0) {
        stop = 1;
    } else if (W.quit_requested || W.in_error) {
        stop = 1;
    }
    if (stop) {
        efx_web_publish_exit(efx_bridge_exit_code());
    }
    return stop;
}

EMSCRIPTEN_KEEPALIVE int efx_bridge_frame(void) {
    /* direct (Node harness) path bypasses the sokol frame loop, so derive dt
       here; the first frame reports 0 (desktop parity, design D3) */
    double now = emscripten_get_now();
    double dt = W.frame_have_now ? (now - W.frame_last_now) / 1000.0 : 0.0;
    W.frame_have_now = 1;
    W.frame_last_now = now;
    return web_frame(NULL, dt);
}

EMSCRIPTEN_KEEPALIVE const char *efx_web_root(void) {
    return W.root;
}

EMSCRIPTEN_KEEPALIVE void efx_web_set_golden_mode(void) {
    W.golden_mode = 1;
}

int efx_web_main(int argc, char *const *argv) {
    int golden = W.golden_mode;
    memset(&W, 0, sizeof(W));
    W.golden_mode = golden;
    if (golden) {
        static char outbuf[160];
        char scene[64] = "clear";
        const char *s = emscripten_run_script_string(
            "(function(){ try { return new URLSearchParams(location.search).get('scene') || 'clear'; } catch (e) { return 'clear'; } })()");
        if (s && !strchr(s, '/') && !strstr(s, "..")) {
            snprintf(scene, sizeof(scene), "%s", s);
        }
        const char *r = emscripten_run_script_string(
            "(function(){ try { return new URLSearchParams(location.search).get('root') || ''; } catch (e) { return ''; } })()");
        if (r && r[0] && !strstr(r, "..")) {
            snprintf(W.root, sizeof(W.root), "%s", r);
        } else {
            snprintf(W.root, sizeof(W.root), "/goldens/%s", scene);
        }
        snprintf(outbuf, sizeof(outbuf), "/captures/%s.png", scene);
        W.capture.frame = 2;
        W.capture.output = outbuf;
    } else {
        if (argc >= 2) {
            if (argv[1][0] == '/') {
                snprintf(W.root, sizeof(W.root), "%s", argv[1]);
            } else {
                char cwd[EFX_WEB_ROOT_MAX];
                if (getcwd(cwd, sizeof(cwd))) {
                    snprintf(W.root, sizeof(W.root), "%s/%s", cwd, argv[1]);
                } else {
                    snprintf(W.root, sizeof(W.root), "%s", argv[1]);
                }
            }
        } else {
            snprintf(W.root, sizeof(W.root), "%s", "/examples/browser");
        }
        if (argc > 2) {
            set_args(argv + 2, argc - 2);
        }
    }
    W.dom = efx_web_has_dom_js();
    return 0;
}

EMSCRIPTEN_KEEPALIVE void efx_web_start_loop(void) {
    if (!W.dom) {
        return;
    }
    efx_platform_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.capture = W.capture;
    efx_frame_hooks hooks;
    hooks.ud = NULL;
    hooks.on_frame = web_frame;
    efx_platform_run(&desc, hooks);
}
