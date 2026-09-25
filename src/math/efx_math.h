#ifndef EFX_MATH_H
#define EFX_MATH_H

/*
 * Plain C math surface over GLM (ADR 0005). The implementation TUs are
 * C++-compiled (GLM is C++-only); GLM types never appear here — matrices
 * are float[16] column-major, vectors float[3]/float[4], angles degrees.
 *
 * Conventions (match the script-side efx.mat4 helpers and GLM's storage):
 * column-major float[16], right-handed, OpenGL depth range (-1..+1).
 * Composition: efx_math_mul(out, a, b) computes a·b (b applies to the
 * vector first), efx_math_rotate computes m·R (rotation applied first).
 */

#ifdef __cplusplus
extern "C" {
#endif

/* out = a·b; out may alias a or b */
void efx_math_mul(float out[16], const float a[16], const float b[16]);

void efx_math_identity(float out[16]);

/* vertical fov in degrees */
void efx_math_perspective(float out[16], float fov_y_deg, float aspect,
                          float near_z, float far_z);

/* centered ortho box of the given width/height, depth near..far */
void efx_math_ortho(float out[16], float width, float height,
                    float near_z, float far_z);

/* right-handed look-at, up vector +Y convention passed explicitly */
void efx_math_look_at(float out[16], const float eye[3],
                      const float center[3], const float up[3]);

/* out = m·T(v) (translation applied first); out may alias m */
void efx_math_translate(float out[16], const float m[16], const float v[3]);

/* out = m·R(deg, axis); axis need not be normalized; out may alias m */
void efx_math_rotate(float out[16], const float m[16], float deg,
                     const float axis[3]);

/* out = m·S(v); out may alias m */
void efx_math_scale(float out[16], const float m[16], const float v[3]);

/* vec3 helpers (pure, out may not alias) */
void efx_vec3_add(float out[3], const float a[3], const float b[3]);
void efx_vec3_sub(float out[3], const float a[3], const float b[3]);
void efx_vec3_scale(float out[3], const float v[3], float s);
void efx_vec3_normalize(float out[3], const float v[3]);
void efx_vec3_cross(float out[3], const float a[3], const float b[3]);
float efx_vec3_dot(const float a[3], const float b[3]);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
