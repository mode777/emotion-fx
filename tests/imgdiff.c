/*
 * Golden-image comparator (design D8): compares two PNGs under the
 * settled tolerance policy — a pixel passes when every channel differs by
 * at most 2 of 255; a frame passes when at least 99.5% of pixels pass.
 * Writes a visual diff image highlighting failing pixels.
 *
 * usage: efx_imgdiff <actual.png> <golden.png> <diff.png>
 * exit:  0 = pass, 1 = fail, 2 = usage/IO error
 */
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>

#define EFX_CHANNEL_TOLERANCE 2
#define EFX_MIN_PASS_FRACTION 0.995

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: efx_imgdiff <actual.png> <golden.png> <diff.png>\n");
        return 2;
    }
    int aw, ah, an, gw, gh, gn;
    unsigned char *a = stbi_load(argv[1], &aw, &ah, &an, 4);
    unsigned char *g = stbi_load(argv[2], &gw, &gh, &gn, 4);
    if (!a || !g) {
        fprintf(stderr, "efx_imgdiff: cannot read input image(s)\n");
        free(a);
        free(g);
        return 2;
    }
    if (aw != gw || ah != gh) {
        fprintf(stderr, "efx_imgdiff: size mismatch: %dx%d vs %dx%d\n",
                aw, ah, gw, gh);
        free(a);
        free(g);
        return 1;
    }
    size_t n = (size_t)aw * ah;
    unsigned char *diff = malloc(n * 4);
    if (!diff) {
        free(a);
        free(g);
        return 2;
    }
    size_t pass = 0;
    for (size_t i = 0; i < n; i++) {
        const unsigned char *pa = a + i * 4;
        const unsigned char *pg = g + i * 4;
        int ok = 1;
        unsigned char d[4] = {0, 0, 0, 0};
        for (int c = 0; c < 3; c++) {
            int delta = pa[c] - pg[c];
            if (delta < 0) delta = -delta;
            if (delta > EFX_CHANNEL_TOLERANCE) {
                ok = 0;
                d[0] = 255; /* failing pixel: red */
                d[3] = 255;
            }
        }
        diff[i * 4 + 0] = d[0];
        diff[i * 4 + 1] = ok ? pg[0] / 3 : 0;
        diff[i * 4 + 2] = ok ? pg[2] / 3 : 0;
        diff[i * 4 + 3] = 255;
        pass += ok;
    }
    double fraction = n ? (double)pass / (double)n : 0.0;
    int result = fraction >= EFX_MIN_PASS_FRACTION ? 0 : 1;
    printf("efx_imgdiff: %zu/%zu pixels pass (%.4f%%, need %.1f%%) -> %s\n",
           pass, n, fraction * 100.0, EFX_MIN_PASS_FRACTION * 100.0,
           result == 0 ? "PASS" : "FAIL");
    stbi_write_png(argv[3], aw, ah, 4, diff, aw * 4);
    free(diff);
    free(a);
    free(g);
    return result;
}
