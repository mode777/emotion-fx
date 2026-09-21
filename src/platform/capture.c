/*
 * Frame capture: per-backend framebuffer readback + PNG encode (D7/D8).
 * GL backends read the default framebuffer; D3D11 copies the swapchain
 * back buffer into a staging texture; Metal renders nothing special here
 * (the capture pass targets an injected texture — see platform.c) and this
 * file only reads back that texture via the blit path.
 */
#include "platform/capture.h"
#include "platform/backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#if defined(SOKOL_GLCORE) || defined(SOKOL_GLES3)

static void flip_rows(uint8_t *px, int w, int h) {
    const int stride = w * 4;
    uint8_t *row = malloc((size_t)stride);
    for (int y = 0; y < h / 2; y++) {
        uint8_t *a = px + (size_t)y * stride;
        uint8_t *b = px + (size_t)(h - 1 - y) * stride;
        memcpy(row, a, (size_t)stride);
        memcpy(a, b, (size_t)stride);
        memcpy(b, row, (size_t)stride);
    }
    free(row);
}

#define GL_GLEXT_PROTOTYPES
#if defined(SOKOL_GLCORE)
#include <GL/gl.h>
#elif defined(SOKOL_GLES3)
#include <GLES3/gl3.h>
#endif
#include "sokol_app.h" /* sapp_width/height */

int efx_capture_read_rgba(uint8_t **out_pixels, int *out_w, int *out_h) {
    int w = sapp_width();
    int h = sapp_height();
    uint8_t *px = malloc((size_t)w * h * 4);
    if (!px) {
        return -1;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFinish();
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    flip_rows(px, w, h);
    *out_pixels = px;
    *out_w = w;
    *out_h = h;
    return 0;
}

#elif defined(SOKOL_D3D11)

#include <d3d11.h>
#include <dxgi.h>
#include "sokol_app.h"

int efx_capture_read_rgba(uint8_t **out_pixels, int *out_w, int *out_h) {
    int w = sapp_width();
    int h = sapp_height();
    sapp_environment env = sapp_get_environment();
    ID3D11Device *dev = (ID3D11Device *)env.d3d11.device;
    ID3D11DeviceContext *ctx = (ID3D11DeviceContext *)env.d3d11.device_context;
    IDXGISwapChain *sc = (IDXGISwapChain *)sapp_d3d11_get_swap_chain();
    if (!dev || !ctx || !sc) {
        return -1;
    }
    ID3D11Texture2D *back = NULL;
    if (FAILED(sc->lpVtbl->GetBuffer(sc, 0, __uuidof(ID3D11Texture2D), (void **)&back))) {
        return -1;
    }
    D3D11_TEXTURE2D_DESC bd;
    back->lpVtbl->GetDesc(back, &bd);
    D3D11_TEXTURE2D_DESC sd = bd;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.MipLevels = 1;
    sd.ArraySize = 1;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    ID3D11Texture2D *staging = NULL;
    if (FAILED(dev->lpVtbl->CreateTexture2D(dev, &sd, NULL, &staging))) {
        back->lpVtbl->Release(back);
        return -1;
    }
    ctx->lpVtbl->CopyResource(ctx, (ID3D11Resource *)staging, (ID3D11Resource *)back);
    D3D11_MAPPED_SUBRESOURCE map;
    if (FAILED(ctx->lpVtbl->Map(ctx, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &map))) {
        staging->lpVtbl->Release(staging);
        back->lpVtbl->Release(back);
        return -1;
    }
    uint8_t *px = malloc((size_t)w * h * 4);
    for (int y = 0; y < h && px; y++) {
        const uint8_t *src = (const uint8_t *)map.pData + (size_t)y * map.RowPitch;
        uint8_t *dst = px + (size_t)y * w * 4;
        for (int x = 0; x < w; x++) {
            dst[x * 4 + 0] = src[x * 4 + 2]; /* BGRA -> RGBA */
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = src[x * 4 + 3];
        }
    }
    ctx->lpVtbl->Unmap(ctx, (ID3D11Resource *)staging, 0);
    staging->lpVtbl->Release(staging);
    back->lpVtbl->Release(back);
    if (!px) {
        return -1;
    }
    *out_pixels = px;
    *out_w = w;
    *out_h = h;
    return 0;
}

#elif defined(SOKOL_METAL)

#include <Metal/Metal.h>
#include "sokol_app.h"

/* set by platform.c when it creates the injected capture texture; the
   capture pass renders into this texture instead of the swapchain */
static id<MTLTexture> g_capture_tex;

void efx_capture_metal_set_texture(void *tex) {
    g_capture_tex = (__bridge id<MTLTexture>)tex;
}

/* blit the injected capture texture so the CPU can read it */
int efx_capture_read_rgba(uint8_t **out_pixels, int *out_w, int *out_h) {
    if (!g_capture_tex) {
        return -1;
    }
    id<MTLDevice> dev = (__bridge id<MTLDevice>)sapp_get_environment().metal.device;
    if (!dev) {
        return -1;
    }
    int w = (int)g_capture_tex.width;
    int h = (int)g_capture_tex.height;
    id<MTLCommandQueue> q = [dev newCommandQueue];
    id<MTLCommandBuffer> cb = [q commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    [blit synchronizeResource:g_capture_tex];
    [blit endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    uint8_t *px = malloc((size_t)w * h * 4);
    if (!px) {
        return -1;
    }
    [g_capture_tex getBytes:px bytesPerRow:(NSUInteger)w * 4
                 fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
    /* BGRA -> RGBA (texture rows are already top-down) */
    for (int i = 0; i < w * h; i++) {
        uint8_t t = px[i * 4 + 0];
        px[i * 4 + 0] = px[i * 4 + 2];
        px[i * 4 + 2] = t;
    }
    *out_pixels = px;
    *out_w = w;
    *out_h = h;
    return 0;
}

#else
int efx_capture_read_rgba(uint8_t **out_pixels, int *out_w, int *out_h) {
    (void)out_pixels; (void)out_w; (void)out_h;
    return -1;
}
#endif

int efx_capture_write_png(const char *path, int w, int h, const uint8_t *rgba) {
    return stbi_write_png(path, w, h, 4, rgba, w * 4) ? 0 : -1;
}
