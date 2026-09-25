/*
 * GLM-backed implementation of the plain C math surface (efx_math.h).
 * C++ translation unit — the only place GLM types exist (ADR 0005).
 * Conventions: column-major float[16], right-handed, GL depth range.
 *
 * float[16] <-> glm::mat4 conversion goes through glm::make_mat4 /
 * glm::value_ptr (the supported layout bridge) — no reinterpret casts.
 */
#include "math/efx_math.h"

#include <cstring>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

static glm::mat4 load(const float m[16]) {
    return glm::make_mat4(m);
}

static void store(float out[16], const glm::mat4 &m) {
    const float *src = glm::value_ptr(m);
    std::memcpy(out, src, 16 * sizeof(float));
}

extern "C" {

void efx_math_mul(float out[16], const float a[16], const float b[16]) {
    store(out, load(a) * load(b));
}

void efx_math_identity(float out[16]) {
    store(out, glm::mat4(1.0f));
}

void efx_math_perspective(float out[16], float fov_y_deg, float aspect,
                          float near_z, float far_z) {
    store(out, glm::perspectiveRH_NO(glm::radians(fov_y_deg), aspect,
                                     near_z, far_z));
}

void efx_math_ortho(float out[16], float width, float height,
                    float near_z, float far_z) {
    store(out, glm::orthoRH_NO(-width * 0.5f, width * 0.5f,
                               -height * 0.5f, height * 0.5f,
                               near_z, far_z));
}

void efx_math_look_at(float out[16], const float eye[3],
                      const float center[3], const float up[3]) {
    store(out, glm::lookAtRH(glm::make_vec3(eye), glm::make_vec3(center),
                             glm::make_vec3(up)));
}

void efx_math_translate(float out[16], const float m[16], const float v[3]) {
    store(out, glm::translate(load(m), glm::make_vec3(v)));
}

void efx_math_rotate(float out[16], const float m[16], float deg,
                     const float axis[3]) {
    store(out, glm::rotate(load(m), glm::radians(deg), glm::make_vec3(axis)));
}

void efx_math_scale(float out[16], const float m[16], const float v[3]) {
    store(out, glm::scale(load(m), glm::make_vec3(v)));
}

void efx_vec3_add(float out[3], const float a[3], const float b[3]) {
    const glm::vec3 r = glm::make_vec3(a) + glm::make_vec3(b);
    out[0] = r.x;
    out[1] = r.y;
    out[2] = r.z;
}

void efx_vec3_sub(float out[3], const float a[3], const float b[3]) {
    const glm::vec3 r = glm::make_vec3(a) - glm::make_vec3(b);
    out[0] = r.x;
    out[1] = r.y;
    out[2] = r.z;
}

void efx_vec3_scale(float out[3], const float v[3], float s) {
    const glm::vec3 r = glm::make_vec3(v) * s;
    out[0] = r.x;
    out[1] = r.y;
    out[2] = r.z;
}

void efx_vec3_normalize(float out[3], const float v[3]) {
    const glm::vec3 r = glm::normalize(glm::make_vec3(v));
    out[0] = r.x;
    out[1] = r.y;
    out[2] = r.z;
}

void efx_vec3_cross(float out[3], const float a[3], const float b[3]) {
    const glm::vec3 r = glm::cross(glm::make_vec3(a), glm::make_vec3(b));
    out[0] = r.x;
    out[1] = r.y;
    out[2] = r.z;
}

float efx_vec3_dot(const float a[3], const float b[3]) {
    return glm::dot(glm::make_vec3(a), glm::make_vec3(b));
}

} /* extern "C" */
