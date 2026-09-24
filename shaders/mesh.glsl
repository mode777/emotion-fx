/*
 * EmotionFX 3D mesh shader — single shader source for all platforms
 * (ADR 0021), transpiled by the pinned sokol-shdc into shaders/mesh.h.
 *
 * F3 canned fill: unlit, fragment = vertex color × tint (design D5).
 * Attributes arrive interleaved pos(3f) normal(3f) uv(2f) color(4f);
 * positions arrive in clip space (MVP premultiplied on the CPU, design
 * D3/D7 — the D3D11/Metal depth-range remap is folded into the MVP by
 * the platform layer, not by per-backend shader code).
 */

@vs mesh_vs
/* The MVP is a plain vec4[4] (std140, 16 floats column-major) combined
   with explicit dot products: identical uniform bytes and identical math
   on every backend, with no per-backend matrix-packing conventions. */
layout(binding=0) uniform vs_params {
    vec4 mvp0;
    vec4 mvp1;
    vec4 mvp2;
    vec4 mvp3;
    vec4 tint;
};

/* Attribute slots must be declared consumed-first: the canned F3 shader
   reads only position and color, and sokol requires pipeline attribute
   slots to be contiguous from 0 (F4 re-adds normal/uv as slots 2/3). */
in vec3 a_pos;
in vec4 a_color;
in vec3 a_normal;
in vec2 a_uv;

out vec4 efx_color;

void main() {
    vec4 p = vec4(a_pos, 1.0);
    vec4 clip = vec4(dot(mvp0, p), dot(mvp1, p), dot(mvp2, p), dot(mvp3, p));
    gl_Position = clip;
    efx_color = a_color * tint;
}
@end

@fs mesh_fs
in vec4 efx_color;

out vec4 frag_color;

void main() {
    frag_color = efx_color;
}
@end

@program mesh mesh_vs mesh_fs
