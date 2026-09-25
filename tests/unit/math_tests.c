/*
 * Headless unit tests for the GLM wrapper (efx_math, ADR 0005) and the
 * F3 camera/MVP composition contract (design D3/D7).
 * Usage: efx_math_tests <case-name> ; exit 0 = pass.
 */
#include "math/efx_math.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fail(const char *what) {
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

static int feq(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static int m16eq(const float *a, const float *b) {
    for (int i = 0; i < 16; i++) {
        if (!feq(a[i], b[i])) {
            fprintf(stderr, "  mismatch at %d: %f vs %f\n", i, a[i], b[i]);
            return 0;
        }
    }
    return 1;
}

/* transform a point (w=1) by a column-major matrix */
static void xform(float out[3], const float m[16], const float p[3]) {
    out[0] = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
    out[1] = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
    out[2] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
}

static int identity(void) {
    float m[16];
    efx_math_identity(m);
    static const float expect[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                     0, 0, 1, 0, 0, 0, 0, 1};
    if (!m16eq(m, expect)) return fail("identity values");
    return 0;
}

static int perspective_values(void) {
    /* GL-style RH perspective: m[0] = f/aspect, m[5] = f,
       f = 1/tan(fov/2); m[10] = -(far+near)/(far-near), m[11] = -1,
       m[14] = -2*far*near/(far-near) */
    float m[16];
    efx_math_perspective(m, 60.0f, 4.0f / 3.0f, 0.1f, 100.0f);
    float f = 1.0f / tanf((float)(3.14159265358979323846) / 6.0f);
    if (!feq(m[0], f / (4.0f / 3.0f))) return fail("perspective m[0]");
    if (!feq(m[5], f)) return fail("perspective m[5]");
    if (!feq(m[10], -(100.0f + 0.1f) / (100.0f - 0.1f)))
        return fail("perspective m[10]");
    if (!feq(m[11], -1.0f)) return fail("perspective m[11] (RH, GL range)");
    if (!feq(m[14], -2.0f * 100.0f * 0.1f / (100.0f - 0.1f)))
        return fail("perspective m[14]");
    if (!feq(m[1], 0) || !feq(m[4], 0)) return fail("perspective zeros");
    return 0;
}

static int ortho_values(void) {
    float m[16];
    efx_math_ortho(m, 4.0f, 2.0f, 1.0f, 11.0f);
    if (!feq(m[0], 0.5f)) return fail("ortho m[0] = 2/width");
    if (!feq(m[5], 1.0f)) return fail("ortho m[5] = 2/height");
    if (!feq(m[10], -2.0f / 10.0f)) return fail("ortho m[10]");
    if (!feq(m[14], -12.0f / 10.0f)) return fail("ortho m[14] center");
    return 0;
}

static int look_at_values(void) {
    /* eye (0,2,5) looking at origin: forward = -(0,2,5)/|(0,2,5)| */
    float eye[3] = {0, 2, 5};
    float center[3] = {0, 0, 0};
    float up[3] = {0, 1, 0};
    float m[16];
    efx_math_look_at(m, eye, center, up);
    /* the eye maps to the origin */
    float e[3];
    xform(e, m, eye);
    if (!feq(e[0], 0) || !feq(e[1], 0) || !feq(e[2], 0))
        return fail("lookAt eye at origin");
    /* a point in front of the eye (toward target) maps to negative view z
       (right-handed GL convention: camera looks down -z) */
    float front[3] = {0, 1, 4}; /* 1 unit toward target along the axis */
    float q[3];
    xform(q, m, front);
    if (!(q[2] < -0.9f)) return fail("lookAt forward is -z");
    return 0;
}

static int mul_order(void) {
    /* efx_math_mul(out, a, b) = a·b: b applies to the vector first */
    float t[16], r[16], tr[16], rt[16];
    float v[3] = {0, 0, 0};
    float tv[3] = {5, 0, 0};
    efx_math_identity(t);
    efx_math_translate(t, t, tv);
    float axis[3] = {0, 1, 0};
    efx_math_identity(r);
    efx_math_rotate(r, r, 90, axis);
    efx_math_mul(tr, t, r); /* T·R: rotation first */
    efx_math_mul(rt, r, t); /* R·T: translation first */
    float a[3], b[3];
    xform(a, tr, v);
    xform(b, rt, v);
    /* T·R rotates the origin to the origin, then translates: (5,0,0) */
    if (!feq(a[0], 5) || !feq(a[1], 0) || !feq(a[2], 0))
        return fail("mul(T,R) applies R first");
    /* R·T translates first, then rotates: (0,0,-5) */
    if (!feq(b[0], 0) || !feq(b[1], 0) || !feq(b[2], -5))
        return fail("mul(R,T) applies T first");
    return 0;
}

static float *tv_dummy(void) {
    static float v[3] = {1, 2, 3};
    return v;
}

static int translate_rotate_scale(void) {
    float m[16], p[3], q[3];
    /* translate */
    efx_math_identity(m);
    float v[3] = {1, 2, 3};
    efx_math_translate(m, m, v);
    float p0[3] = {0, 0, 0};
    xform(p, m, p0);
    if (!feq(p[0], 1) || !feq(p[1], 2) || !feq(p[2], 3))
        return fail("translate");
    /* rotate 90 about Y: x axis -> -z (right-handed, CCW looking down -Y
       toward the origin) */
    efx_math_identity(m);
    float axis[3] = {0, 1, 0};
    efx_math_rotate(m, m, 90, axis);
    float px[3] = {1, 0, 0};
    xform(q, m, px);
    if (!feq(q[0], 0) || !feq(q[1], 0) || !feq(q[2], -1))
        return fail("rotate 90 about Y");
    /* rotate composes onto m (m·R): translation unaffected by later rotate */
    efx_math_identity(m);
    efx_math_translate(m, m, tv_dummy());
    efx_math_rotate(m, m, 90, axis);
    xform(q, m, p0);
    if (!feq(q[0], 1) || !feq(q[1], 2) || !feq(q[2], 3))
        return fail("rotate composes m·R");
    /* scale */
    efx_math_identity(m);
    float s[3] = {2, 3, 4};
    efx_math_scale(m, m, s);
    xform(q, m, px);
    if (!feq(q[0], 2) || !feq(q[1], 0) || !feq(q[2], 0))
        return fail("scale");
    return 0;
}

static int vec3_ops(void) {
    float a[3] = {1, 0, 0}, b[3] = {0, 1, 0}, out[3];
    efx_vec3_add(out, a, b);
    if (!feq(out[0], 1) || !feq(out[1], 1)) return fail("add");
    efx_vec3_sub(out, a, b);
    if (!feq(out[0], 1) || !feq(out[1], -1)) return fail("sub");
    efx_vec3_scale(out, a, 4);
    if (!feq(out[0], 4)) return fail("scale");
    efx_vec3_cross(out, a, b);
    if (!feq(out[2], 1)) return fail("cross");
    if (!feq(efx_vec3_dot(a, b), 0)) return fail("dot");
    float c[3] = {3, 0, 4};
    efx_vec3_normalize(out, c);
    if (!feq(out[0], 0.6f) || !feq(out[2], 0.8f)) return fail("normalize");
    /* alias-free: normalize of unit vector stays unit */
    efx_vec3_normalize(out, a);
    if (!feq(out[0], 1)) return fail("normalize unit");
    return 0;
}

/* MVP composition (design D3/D7): VP(from camera) · model */
static int mvp_compose(void) {
    float view[16], proj[16], vp[16], model[16], mvp[16];
    float eye[3] = {0, 0, 5}, center[3] = {0, 0, 0}, up[3] = {0, 1, 0};
    efx_math_look_at(view, eye, center, up);
    efx_math_perspective(proj, 90.0f, 640.0f / 480.0f, 0.1f, 100.0f);
    efx_math_mul(vp, proj, view);
    /* a point 1 unit in front of the eye lands on the view axis in front
       of the camera: clip z between -w and w; y sign preserved in GL NDC */
    float model_axis[3] = {0, 0, 1};
    efx_math_identity(model);
    efx_math_translate(model, model, model_axis); /* z = +1, toward camera */
    efx_math_mul(mvp, vp, model);
    float p[3] = {0, 0, 0};
    float clip[4];
    clip[0] = mvp[0] * p[0] + mvp[4] * p[1] + mvp[8] * p[2] + mvp[12];
    clip[1] = mvp[1] * p[0] + mvp[5] * p[1] + mvp[9] * p[2] + mvp[13];
    clip[2] = mvp[2] * p[0] + mvp[6] * p[1] + mvp[10] * p[2] + mvp[14];
    clip[3] = mvp[3] * p[0] + mvp[7] * p[1] + mvp[11] * p[2] + mvp[15];
    if (!(clip[3] > 0)) return fail("w positive");
    float ndc_z = clip[2] / clip[3];
    /* point 4 units from the eye at z=+1 -> view z = -4; NDC z = ((f+n)
       * 4 - 2fn) / (4 (f-n)) with sign flipped: between -1 and 1, nearer
       than the far plane */
    if (!(ndc_z > -1.0f && ndc_z < 1.0f)) return fail("ndc in range");
    if (!(ndc_z > 0.9f)) return fail("near camera, high ndc z");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: efx_math_tests <case>\n");
        return 2;
    }
    const char *c = argv[1];
    if (!strcmp(c, "identity")) return identity();
    if (!strcmp(c, "perspective_values")) return perspective_values();
    if (!strcmp(c, "ortho_values")) return ortho_values();
    if (!strcmp(c, "look_at_values")) return look_at_values();
    if (!strcmp(c, "mul_order")) return mul_order();
    if (!strcmp(c, "translate_rotate_scale")) return translate_rotate_scale();
    if (!strcmp(c, "vec3_ops")) return vec3_ops();
    if (!strcmp(c, "mvp_compose")) return mvp_compose();
    fprintf(stderr, "unknown case: %s\n", c);
    return 2;
}
