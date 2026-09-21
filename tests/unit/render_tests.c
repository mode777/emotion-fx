/*
 * Headless display-list unit tests (ADR 0019 record/assert gate).
 * Usage: efx_render_tests <case-name> ; exit 0 = pass.
 */
#include "render/render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *what) {
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

static int feq(float a, float b) {
    return fabsf(a - b) < 0.001f;
}

/* mock sink --------------------------------------------------------- */

static int g_created, g_destroyed;

static void *mock_create(void *ud, int w, int h, const uint8_t *rgba) {
    (void)ud; (void)w; (void)h; (void)rgba;
    g_created++;
    return malloc(8);
}

static void mock_destroy(void *ud, void *native) {
    (void)ud;
    g_destroyed++;
    free(native);
}

static void install_mock_sink(void) {
    static const efx_render_sink sink = {
        NULL, mock_create, mock_destroy, NULL, NULL,
    };
    efx_render_install_sink(&sink);
    g_created = 0;
    g_destroyed = 0;
}

/* cases -------------------------------------------------------------- */

static int compose_camera(void) {
    efx_camera2d cam = {640, 480, 320, 240, 1, 0};
    efx_affine m = efx_camera_matrix(&cam, 640, 480);
    /* center maps to center */
    float px = m.a * 320 + m.c * 240 + m.tx;
    float py = m.b * 320 + m.d * 240 + m.ty;
    if (!feq(px, 320) || !feq(py, 240)) return fail("center not fixed");
    /* zoom 2: visible world width halves; world x=480 (the new right edge)
       maps to the frame's right edge (640) */
    cam.zoom = 2;
    m = efx_camera_matrix(&cam, 640, 480);
    px = m.a * 480 + m.c * 240 + m.tx;
    py = m.b * 480 + m.d * 240 + m.ty;
    if (!feq(px, 640) || !feq(py, 240)) return fail("zoom pivot");
    /* rotation 90 clockwise (y-down): right of center -> below center */
    cam.zoom = 1;
    cam.rotation = 90;
    m = efx_camera_matrix(&cam, 640, 480);
    px = m.a * 420 + m.c * 240 + m.tx;
    py = m.b * 420 + m.d * 240 + m.ty;
    if (!feq(px, 320) || !feq(py, 340)) return fail("rotation pivot/direction");
    return 0;
}

static int compose_quad(void) {
    /* corner anchoring, identity transform */
    efx_affine m = efx_quad_matrix(10, 20, 100, 50, 0, 1);
    float px = m.a * 0 + m.c * 0 + m.tx;
    float py = m.b * 0 + m.d * 0 + m.ty;
    if (!feq(px, 10) || !feq(py, 20)) return fail("top-left anchor");
    px = m.a * 100 + m.c * 50 + m.tx;
    py = m.b * 100 + m.d * 50 + m.ty;
    if (!feq(px, 110) || !feq(py, 70)) return fail("bottom-right corner");
    /* rotation pivots on quad center */
    m = efx_quad_matrix(10, 20, 100, 50, 90, 1);
    px = m.a * 0 + m.c * 0 + m.tx;
    py = m.b * 0 + m.d * 0 + m.ty;
    if (!feq(px, 85) || !feq(py, -5)) return fail("rotation pivot center");
    /* scale pivots on quad center: corners at center ± (2*50, 2*25) */
    m = efx_quad_matrix(10, 20, 100, 50, 0, 2);
    px = m.a * 0 + m.c * 0 + m.tx;
    py = m.b * 0 + m.d * 0 + m.ty;
    if (!feq(px, -40) || !feq(py, -5)) return fail("scale pivot center");
    return 0;
}

static int value_snapshot(void) {
    install_mock_sink();
    efx_render_set_viewport(1024, 768);
    efx_camera2d cam = {640, 480, 320, 240, 1, 0};
    efx_render_set_camera(&cam);
    efx_render_quad(0, 0, 32, 32, 0, NULL, 0, 1, NULL, 0);
    efx_camera2d cam2 = {640, 480, 100, 100, 1, 0};
    efx_render_set_camera(&cam2);
    efx_render_quad(0, 0, 32, 32, 0, NULL, 0, 1, NULL, 0);

    /* camera at record time applies; second quad sees new camera */
    efx_affine expect = efx_camera_matrix(&cam, 640, 480);
    expect = efx_affine_mul(expect, efx_quad_matrix(0, 0, 32, 32, 0, 1));
    int count = 0;
    const efx_quad_record *recs = efx_render_records(&count);
    if (count != 2) return fail("record count");
    if (!feq(recs[0].m.a, expect.a) || !feq(recs[0].m.tx, expect.tx))
        return fail("first quad uses first camera");
    if (feq(recs[1].m.tx, expect.tx)) return fail("second quad used old camera");
    /* keys: stable sort key = record index */
    if (recs[0].sort_key != 0 || recs[1].sort_key != 1) return fail("sort keys");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int default_camera_viewport(void) {
    install_mock_sink();
    efx_render_set_viewport(1024, 600);
    efx_render_quad(0, 0, 8, 8, 0, NULL, 0, 1, NULL, 0);
    int count = 0;
    const efx_quad_record *recs = efx_render_records(&count);
    if (recs[0].frame_w != 1024 || recs[0].frame_h != 600)
        return fail("default frame = viewport");
    /* default view looks at frame center */
    if (!feq(recs[0].m.a, 1) || !feq(recs[0].m.tx, 0) || !feq(recs[0].m.ty, 0))
        return fail("default view identity");
    /* frame set through camera wins over viewport */
    efx_camera2d cam = {640, 480, 320, 240, 1, 0};
    efx_render_set_camera(&cam);
    efx_render_quad(0, 0, 8, 8, 0, NULL, 0, 1, NULL, 0);
    recs = efx_render_records(&count);
    if (recs[1].frame_w != 640 || recs[1].frame_h != 480)
        return fail("explicit frame");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int blend_snapshot(void) {
    install_mock_sink();
    efx_render_quad(0, 0, 8, 8, 0, NULL, 0, 1, NULL, 0);
    efx_render_set_blend(EFX_BLEND_ADDITIVE);
    efx_render_quad(0, 0, 8, 8, 0, NULL, 0, 1, NULL, 0);
    if (efx_render_set_blend(99) == 0) return fail("invalid blend accepted");
    int count = 0;
    const efx_quad_record *recs = efx_render_records(&count);
    if (recs[0].blend != EFX_BLEND_ALPHA || recs[1].blend != EFX_BLEND_ADDITIVE)
        return fail("blend per record");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int record_budget(void) {
    static const efx_render_sink sink = {NULL, mock_create, mock_destroy, NULL, NULL};
    efx_render_install_sink(&sink);
    int pushed = 0;
    for (;;) {
        int rc = efx_render_quad(0, 0, 1, 1, 0, NULL, 0, 1, NULL, 0);
        if (rc == EFX_RENDER_ERR_BUDGET) break;
        if (rc != EFX_RENDER_OK) return fail("unexpected error in budget loop");
        pushed++;
        if (pushed > 1000000) return fail("budget never reached");
    }
    if (pushed < 100000) return fail("budget suspiciously small");
    efx_render_begin_frame();
    int count = 0;
    efx_render_records(&count);
    if (count != 0) return fail("rewind after budget stop");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int texture_lifecycle(void) {
    install_mock_sink();
    uint8_t px[4] = {255, 0, 0, 255};
    uint64_t t1 = efx_render_texture_create(2, 2, px);
    if (!t1 || !efx_render_texture_alive(t1)) return fail("texture create");
    int w = 0, h = 0;
    efx_render_texture_size(t1, &w, &h);
    if (w != 2 || h != 2) return fail("texture size");
    /* deferred destroy: native alive until end_frame */
    if (efx_render_texture_destroy(t1) != EFX_RENDER_OK) return fail("destroy");
    if (efx_render_texture_alive(t1)) return fail("alive after destroy");
    if (g_destroyed != 0) return fail("destroy not deferred");
    efx_render_end_frame();
    if (g_destroyed != 1) return fail("deferred destroy not flushed");
    /* after the frame-end release the C handle is dead (JS wrapper layer
       supplies the documented idempotency via its own flag) */
    if (efx_render_texture_destroy(t1) == EFX_RENDER_OK) return fail("dead handle destroy");
    /* stale handle (generation bump) */
    uint64_t stale = t1;
    uint64_t t2 = efx_render_texture_create(1, 1, px);
    if (!t2 || t2 == stale) return fail("handle reuse");
    if (efx_render_texture_alive(stale)) return fail("stale handle alive");
    if (efx_render_texture_destroy(stale) == EFX_RENDER_OK) return fail("stale destroy");
    /* white texture: permanent, usable, not destroyable */
    uint64_t white = efx_render_white_texture();
    if (!white || !efx_render_texture_alive(white)) return fail("white texture");
    if (efx_render_texture_destroy(white) != EFX_RENDER_ERR_PERMANENT)
        return fail("white destroy must be refused");
    efx_render_end_frame();
    efx_render_shutdown();
    efx_render_install_sink(NULL);
    if (efx_render_white_texture() != 0) return fail("white after shutdown");
    return 0;
}

static int record_fields(void) {
    install_mock_sink();
    uint8_t px[4] = {0, 0, 255, 255};
    uint64_t tex = efx_render_texture_create(64, 32, px);
    float color[4] = {1, 0.5, 0.25, 0.125};
    float src[4] = {8, 4, 16, 8};
    efx_render_quad(1, 2, 30, 40, tex, color, 45, 2, src, 1);
    int count = 0;
    const efx_quad_record *r = efx_render_records(&count);
    if (count != 1) return fail("count");
    if (r[0].tw != 64 || r[0].th != 32) return fail("texel size");
    if (!feq(r[0].sx, 8) || !feq(r[0].sw, 16) || !feq(r[0].sh, 8))
        return fail("source rect");
    if (!feq(r[0].color[1], 0.5f) || !feq(r[0].color[3], 0.125f))
        return fail("tint");
    /* default source rect = full texture */
    efx_render_quad(0, 0, 4, 4, tex, NULL, 0, 1, NULL, 0);
    r = efx_render_records(&count);
    if (!feq(r[1].sw, 64) || !feq(r[1].sh, 32)) return fail("default src");
    /* default tint = opaque white */
    if (!feq(r[1].color[0], 1) || !feq(r[1].color[3], 1)) return fail("default tint");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

static int batching(void) {
    install_mock_sink();
    uint8_t px[4] = {255, 255, 255, 255};
    uint64_t t1 = efx_render_texture_create(4, 4, px);
    uint64_t t2 = efx_render_texture_create(4, 4, px);
    /* sequence: A A B A  -> three runs (t1x2, t2, t1) */
    efx_render_quad(0, 0, 4, 4, t1, NULL, 0, 1, NULL, 0);
    efx_render_quad(5, 0, 4, 4, t1, NULL, 0, 1, NULL, 0);
    efx_render_set_blend(EFX_BLEND_ADDITIVE);
    efx_render_quad(9, 0, 4, 4, t1, NULL, 0, 1, NULL, 0);
    efx_render_quad(12, 0, 4, 4, t2, NULL, 0, 1, NULL, 0);
    efx_render_set_blend(EFX_BLEND_ALPHA);
    efx_render_quad(15, 0, 4, 4, t1, NULL, 0, 1, NULL, 0);
    int run_count = 0;
    const efx_draw_run *runs = efx_render_runs(&run_count);
    if (run_count != 4) return fail("expected 4 runs");
    if (runs[0].texture != t1 || runs[0].count != 2) return fail("run 0");
    if (runs[1].blend != EFX_BLEND_ADDITIVE) return fail("run 1 blend");
    if (runs[2].texture != t2) return fail("run 2 texture");
    if (runs[3].texture != t1 || runs[3].start != 4) return fail("run 3");
    efx_render_end_frame();
    efx_render_shutdown();
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: efx_render_tests <case>\n");
        return 2;
    }
    const char *c = argv[1];
    if (!strcmp(c, "compose_camera")) return compose_camera();
    if (!strcmp(c, "compose_quad")) return compose_quad();
    if (!strcmp(c, "value_snapshot")) return value_snapshot();
    if (!strcmp(c, "default_camera_viewport")) return default_camera_viewport();
    if (!strcmp(c, "blend_snapshot")) return blend_snapshot();
    if (!strcmp(c, "record_budget")) return record_budget();
    if (!strcmp(c, "texture_lifecycle")) return texture_lifecycle();
    if (!strcmp(c, "record_fields")) return record_fields();
    if (!strcmp(c, "batching")) return batching();
    fprintf(stderr, "unknown case: %s\n", c);
    return 2;
}
