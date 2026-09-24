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
layout(binding=0) uniform vs_params {
    mat4 mvp;
};

in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
in vec4 a_color;

out vec4 efx_color;

void main() {
    gl_Position = mvp * vec4(a_pos, 1.0);
    efx_color = a_color;
}
@end

@fs mesh_fs
layout(binding=1) uniform fs_params {
    vec4 tint;
};

in vec4 efx_color;

out vec4 frag_color;

void main() {
    frag_color = efx_color * tint;
}
@end

@program mesh mesh_vs mesh_fs
