#ifndef EFX_CAPTURE_H
#define EFX_CAPTURE_H

#include <stdint.h>

/* Frame capture for golden-image tests (design D7): read back the default
 * framebuffer and write it as a PNG. Test infrastructure only. */

/* reads the current default-framebuffer contents (call after sg_commit);
 * returns 0 and a malloc'd RGBA8 buffer on success */
int efx_capture_read_rgba(uint8_t **out_pixels, int *out_w, int *out_h);

/* writes w*h RGBA8 pixels as a PNG file; returns 0 on success */
int efx_capture_write_png(const char *path, int w, int h, const uint8_t *rgba);

/* Metal only: hand over the injected capture texture (platform.c) */
void efx_capture_metal_set_texture(void *tex);

#endif
