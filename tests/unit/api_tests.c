/*
 * Headless JS-API tests for the F2 2D layer: installs a mock GPU sink,
 * runs the real quickjs runtime + api bindings, and asserts semantics by
 * driving JS snippets and inspecting the display list from C.
 * Usage: efx_api_tests <case> ; exit 0 = pass.
 */
#include "render/render.h"
#include "runtime/runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *what) {
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

static void *mock_create(void *ud, int w, int h, const uint8_t *rgba) {
    (void)ud; (void)rgba;
    return malloc((size_t)(w * h * 4 > 0 ? w * h * 4 : 1));
}

static void mock_destroy(void *ud, void *native) {
    (void)ud;
    free(native);
}

static const efx_render_sink g_sink = {
    NULL, mock_create, mock_destroy, NULL, NULL,
};

static efx_runtime *g_rt;

static int run_js(const char *code) {
    efx_render_install_sink(&g_sink);
    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    g_rt = efx_runtime_new(NULL, 0);
    if (!g_rt) {
        return 1;
    }
    int rc = efx_runtime_eval_string(g_rt, "test", code);
    if (rc == 0) {
        efx_runtime_collect(g_rt);
    }
    return rc;
}

static void end_js(void) {
    efx_runtime_destroy(g_rt);
    g_rt = NULL;
    efx_render_end_frame();
    efx_render_shutdown();
}

static int ok_js(const char *code) {
    int rc = run_js(code);
    if (rc != 0) {
        fprintf(stderr, "snippet raised unexpectedly: %.120s\n", code);
        return 1;
    }
    return 0;
}

static int err_js(const char *code, const char *what) {
    if (run_js(code) == 0) {
        fprintf(stderr, "snippet did not raise: %s\n", what);
        return 1;
    }
    return 0;
}

static int rec_count(void) {
    int n = 0;
    efx_render_records(&n);
    return n;
}

static int feq(float a, float b) {
    return (a - b) < 0.001f && (b - a) < 0.001f;
}

/* white texture: exists, stable identity, destroy() throws */
static int white(void) {
    if (ok_js("const a = efx.whiteTexture; const b = efx.whiteTexture; if (a !== b) throw new Error('identity');"
              "try { a.destroy(); throw new Error('no'); } catch (e) { if (!(e instanceof TypeError)) throw e; }")) {
        end_js();
        return fail("white texture identity/destroy");
    }
    end_js();
    return 0;
}

/* end-to-end: JS camera + quad options land in the composed record */
static int quad_record(void) {
    const char *code =
        "efx.setCamera2D({ frame: [640, 480], x: 320, y: 240, zoom: 2, rotation: 0 });"
        "efx.drawQuad(0, 0, efx.whiteTexture,"
        "  { rotation: 90, scale: 1.5, color: [1, 0, 0, 1], size: [64, 32],"
        "    sourceRect: { x: 0, y: 0, w: 1, h: 1 } });";
    if (ok_js(code)) {
        end_js();
        return fail("snippet");
    }
    if (rec_count() != 1) {
        end_js();
        return fail("record count");
    }
    efx_camera2d cam = {640, 480, 320, 240, 2, 0};
    efx_affine expect = efx_affine_mul(efx_camera_matrix(&cam, 640, 480),
                                       efx_quad_matrix(0, 0, 32, 16, 90, 1.5f));
    const efx_quad_record *r = efx_render_records(NULL);
    if (!feq(r[0].m.a, expect.a) || !feq(r[0].m.tx, expect.tx) ||
        !feq(r[0].m.ty, expect.ty)) {
        end_js();
        return fail("composed transform mismatch");
    }
    if (!feq(r[0].tw, 1) || !feq(r[0].sw, 1)) {
        end_js();
        return fail("source rect/texture size");
    }
    if (!feq(r[0].w, 64) || !feq(r[0].h, 32)) {
        end_js();
        return fail("explicit size overrides derivation");
    }
    if (!feq(r[0].color[0], 1) || !feq(r[0].color[1], 0) || !feq(r[0].color[3], 1)) {
        end_js();
        return fail("tint");
    }
    if (r[0].blend != EFX_BLEND_ALPHA) {
        end_js();
        return fail("blend");
    }
    end_js();
    return 0;
}

/* size derivation: explicit size -> sourceRect extent -> texture pixels;
 * scale applies after the size is determined */
static int size_derivation(void) {
    const char *code =
        "const img = efx.createImageData({ width: 64, height: 32, pixels: new Uint8Array(64 * 32 * 4) });"
        "const tex = efx.createTexture(img);"
        "efx.drawQuad(0, 0, tex);"                                                    /* texture pixels */
        "efx.drawQuad(0, 0, tex, { sourceRect: { x: 0, y: 0, w: 8, h: 4 } });"        /* src extent */
        "efx.drawQuad(0, 0, tex, { sourceRect: { x: 0, y: 0, w: 8, h: 4 }, size: [50, 20] });"
        "efx.drawQuad(0, 0, tex, { size: [32, 16], scale: 2 });";                     /* scale after size */
    if (ok_js(code)) {
        end_js();
        return fail("snippet");
    }
    const efx_quad_record *r = efx_render_records(NULL);
    if (rec_count() != 4) {
        end_js();
        return fail("record count");
    }
    if (!feq(r[0].w, 64) || !feq(r[0].h, 32)) {
        end_js();
        return fail("derive from texture pixels");
    }
    if (!feq(r[1].w, 8) || !feq(r[1].h, 4)) {
        end_js();
        return fail("derive from sourceRect");
    }
    if (!feq(r[2].w, 50) || !feq(r[2].h, 20)) {
        end_js();
        return fail("explicit size overrides sourceRect");
    }
    /* scale 2 around the (default center) pivot: matrix a-component = 2 */
    if (!feq(r[3].w, 32) || !feq(r[3].h, 16) || !feq(r[3].m.a, 2)) {
        end_js();
        return fail("scale applies after size");
    }
    end_js();
    return 0;
}

/* origin: pivot point in quad-local pixels; placement unchanged without
 * rotation/scale; rotation around origin [0,0] fixes the top-left corner */
static int origin_pivot(void) {
    const char *code =
        "const img = efx.createImageData({ width: 64, height: 32, pixels: new Uint8Array(64 * 32 * 4) });"
        "const tex = efx.createTexture(img);"
        "efx.drawQuad(10, 20, tex);"
        "efx.drawQuad(10, 20, tex, { origin: [50, 100] });"              /* no transform: same */
        "efx.drawQuad(10, 20, tex, { origin: [0, 0], rotation: 90 });";  /* pivot at top-left */
    if (ok_js(code)) {
        end_js();
        return fail("snippet");
    }
    const efx_quad_record *r = efx_render_records(NULL);
    if (rec_count() != 3) {
        end_js();
        return fail("record count");
    }
    /* untransformed: origin must not move the quad */
    if (!feq(r[0].m.tx, r[1].m.tx) || !feq(r[0].m.ty, r[1].m.ty) ||
        !feq(r[0].m.a, r[1].m.a)) {
        end_js();
        return fail("origin must not move an untransformed quad");
    }
    /* origin [0,0] + rotation 90 (y-down, clockwise): local (0,0) maps to
     * (10, 20) and local (64, 0) maps to (10, 20 + 64) */
    if (!feq(r[2].m.a + r[2].m.c * 0 + r[2].m.tx, 10) ||
        !feq(r[2].m.b * 0 + r[2].m.d * 0 + r[2].m.ty, 20)) {
        end_js();
        return fail("origin pivot corner position");
    }
    if (!feq(r[2].m.a * 64 + r[2].m.tx, 10) || !feq(r[2].m.b * 64 + r[2].m.ty, 84)) {
        end_js();
        return fail("origin pivot rotation direction");
    }
    end_js();
    return 0;
}

/* validation matrix for size/origin/zero-extent sourceRect */
static int quad_validation(void) {
    const char *code =
        "function t(fn, kind) {"
        "  try { fn(); throw new Error('did not throw'); }"
        "  catch (e) {"
        "    if (e instanceof Error && !(e instanceof TypeError) && !(e instanceof RangeError)) throw e;"
        "    if (!(e instanceof kind)) throw new Error('wrong kind: ' + e);"
        "  }"
        "}"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { size: [0, 10] }), RangeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { size: [10] }), RangeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { size: 'big' }), TypeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { origin: [NaN, 0] }), RangeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { origin: 'center' }), TypeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture,"
        "  { sourceRect: { x: 0, y: 0, w: 0, h: 1 } }), RangeError);"
        "t(() => efx.drawQuad(0, 0, efx.whiteTexture, { size: [4, 4], frobnicate: 1 }), TypeError);";
    if (ok_js(code)) {
        end_js();
        return fail("quad validation matrix");
    }
    if (rec_count() != 0) {
        end_js();
        return fail("failed calls must record nothing");
    }
    end_js();
    return 0;
}

/* Texture width/height getters: values, whiteTexture, destroyed throws */
static int texture_size_getters(void) {
    const char *code =
        "const img = efx.createImageData({ width: 64, height: 32, pixels: new Uint8Array(64 * 32 * 4) });"
        "const tex = efx.createTexture(img);"
        "if (tex.width !== 64 || tex.height !== 32) throw new Error('texture size');"
        "if (efx.whiteTexture.width !== 1 || efx.whiteTexture.height !== 1)"
        "  throw new Error('white texture size');"
        "tex.destroy();"
        "try { tex.width; throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }"
        "try { efx.drawQuad(0, 0, tex); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }";
    if (ok_js(code)) {
        end_js();
        return fail("texture size getters");
    }
    end_js();
    return 0;
}

/* camera is snapshotted per record */
static int camera_snapshot(void) {
    const char *code =
        "efx.setCamera2D({ frame: [640, 480] });"
        "efx.drawQuad(100, 0, efx.whiteTexture, { size: [8, 8] });"
        "efx.setCamera2D({ frame: [640, 480], x: 370, y: 0 });"
        "efx.drawQuad(100, 0, efx.whiteTexture, { size: [8, 8] });";
    if (ok_js(code)) {
        end_js();
        return fail("snippet");
    }
    const efx_quad_record *r = efx_render_records(NULL);
    if (r[0].m.tx == r[1].m.tx) {
        end_js();
        return fail("camera not snapshotted");
    }
    /* second view looks 50 world px right of the first: at zoom 1 the
       recorded quad shifts 50 frame px left (world moves right on screen) */
    if (!feq(r[1].m.tx - r[0].m.tx, -50.0f)) {
        end_js();
        return fail("camera delta");
    }
    end_js();
    return 0;
}

/* out-of-bounds sourceRect throws RangeError */
static int src_oob(void) {
    if (err_js("efx.drawQuad(0, 0, efx.whiteTexture,"
               "  { sourceRect: { x: 0, y: 0, w: 5, h: 5 } });", "oob sourceRect")) {
        end_js();
        return fail("oob sourceRect must throw");
    }
    end_js();
    return 0;
}

/* record budget surfaces as a thrown error from JS */
static int budget(void) {
    const char *code =
        "try {"
        "  for (let i = 0; i < 500000; i++) efx.drawQuad(0, 0, efx.whiteTexture);"
        "  throw new Error('budget not enforced');"
        "} catch (e) { if (!(e instanceof RangeError)) throw e; }";
    if (ok_js(code)) {
        end_js();
        return fail("budget RangeError");
    }
    end_js();
    return 0;
}

/* texture resource lifecycle at the JS level */
static int texture_lifecycle(void) {
    const char *code =
        "const img = efx.createImageData({ width: 2, height: 2, pixels: new Uint8Array(16) });"
        "const tex = efx.createTexture(img);"
        "tex.destroy();"
        "tex.destroy();" /* idempotent */
        "try { efx.drawQuad(0, 0, tex); throw new Error('no'); }"
        "catch (e) { if (!(e instanceof TypeError)) throw e; }";
    if (ok_js(code)) {
        end_js();
        return fail("texture lifecycle");
    }
    end_js();
    return 0;
}

/* blend snapshot at the JS level */
static int blend_snapshot(void) {
    const char *code =
        "efx.drawQuad(0, 0, efx.whiteTexture, { size: [4, 4] });"
        "efx.setBlendMode('subtractive');"
        "efx.drawQuad(0, 0, efx.whiteTexture, { size: [4, 4] });";
    if (ok_js(code)) {
        end_js();
        return fail("snippet");
    }
    const efx_quad_record *r = efx_render_records(NULL);
    if (r[0].blend != EFX_BLEND_ALPHA || r[1].blend != EFX_BLEND_SUBTRACTIVE) {
        end_js();
        return fail("blend snapshot");
    }
    end_js();
    return 0;
}

/* setClearColor stores through the JS binding */
static int clear_color_js(void) {
    if (ok_js("efx.setClearColor([0.1, 0.7, 0.3, 1]);")) {
        end_js();
        return fail("snippet");
    }
    float c[4];
    efx_render_clear_color(c);
    if (!feq(c[0], 0.1f) || !feq(c[1], 0.7f) || !feq(c[2], 0.3f) || !feq(c[3], 1.0f)) {
        end_js();
        return fail("clear color not stored");
    }
    end_js();
    return 0;
}

/* default camera: frame == viewport, identity view */
static int default_camera(void) {
    if (ok_js("efx.drawQuad(0, 0, efx.whiteTexture, { size: [4, 4] });")) {
        end_js();
        return fail("snippet");
    }
    const efx_quad_record *r = efx_render_records(NULL);
    if (r[0].frame_w != 1024 || r[0].frame_h != 600 || !feq(r[0].m.a, 1) ||
        !feq(r[0].m.tx, 0) || !feq(r[0].m.ty, 0)) {
        end_js();
        return fail("default camera");
    }
    end_js();
    return 0;
}

/* explicit lifecycle hooks: registration order, dt, unsubscribe, sugar */
static int hooks_registration(void) {
    const char *code =
        "globalThis.__hooksLog = [];"
        "try { efx.registerUpdateHook(123); __hooksLog.push('NO-THROW'); }"
        "catch (e) { __hooksLog.push('typeerror:' + (e instanceof TypeError)); }"
        "globalThis.__off = efx.registerUpdateHook(function (dt) {"
        "  __hooksLog.push('uA:' + (typeof dt === 'number' && isFinite(dt))); });"
        "efx.registerUpdateHook(function () { __hooksLog.push('uB'); });"
        "efx.registerRenderHook(function () { __hooksLog.push('r'); });"
        "globalThis.update = function (dt) {"
        "  __hooksLog.push('gU:' + (typeof dt === 'number' && isFinite(dt))); };"
        "globalThis.render = function () { __hooksLog.push('gR'); };";
    if (ok_js(code)) {
        end_js();
        return fail("hooks snippet");
    }
    int has_update = 0;
    int has_render = 0;
    efx_runtime_pick_hooks(g_rt, &has_update, &has_render);
    if (!has_update || !has_render) {
        end_js();
        return fail("sugar hooks not picked up");
    }
    if (efx_runtime_call_hook(g_rt, 1, 0.5) != EFX_HOOK_OK ||
        efx_runtime_call_hook(g_rt, 0, 0.5) != EFX_HOOK_OK) {
        end_js();
        return fail("hook dispatch returned an error");
    }
    /* unsubscribe is idempotent and removes the first hook */
    if (efx_runtime_eval_string(g_rt, "unsub", "__off(); __off();") != 0) {
        end_js();
        return fail("unsubscribe snippet");
    }
    if (efx_runtime_call_hook(g_rt, 1, 0.25) != EFX_HOOK_OK) {
        end_js();
        return fail("post-unsubscribe dispatch");
    }
    const char *want =
        "typeerror:true|uA:true|uB|gU:true|r|gR|uB|gU:true";
    char verify[512];
    snprintf(verify, sizeof(verify),
             "if (__hooksLog.join('|') !== '%s')"
             "  throw new Error('hook order: ' + __hooksLog.join('|'));",
             want);
    if (efx_runtime_eval_string(g_rt, "verify", verify) != 0) {
        end_js();
        return fail("hook order/dt mismatch");
    }
    end_js();
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: efx_api_tests <case>\n");
        return 2;
    }
    const char *c = argv[1];
    if (!strcmp(c, "white")) return white();
    if (!strcmp(c, "quad_record")) return quad_record();
    if (!strcmp(c, "size_derivation")) return size_derivation();
    if (!strcmp(c, "origin_pivot")) return origin_pivot();
    if (!strcmp(c, "quad_validation")) return quad_validation();
    if (!strcmp(c, "texture_size_getters")) return texture_size_getters();
    if (!strcmp(c, "camera_snapshot")) return camera_snapshot();
    if (!strcmp(c, "src_oob")) return src_oob();
    if (!strcmp(c, "budget")) return budget();
    if (!strcmp(c, "texture_lifecycle")) return texture_lifecycle();
    if (!strcmp(c, "blend_snapshot")) return blend_snapshot();
    if (!strcmp(c, "default_camera")) return default_camera();
    if (!strcmp(c, "clear_color_js")) return clear_color_js();
    if (!strcmp(c, "hooks_registration")) return hooks_registration();
    fprintf(stderr, "unknown case: %s\n", c);
    return 2;
}
