/* Build-time helper: scale/crop icon_source.png to 256x256 PNG + JPEG for elf2nro. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image.h"
#include "../third_party/stb_image_write.h"

#define OUT_SIZE 256

static float sample_channel(const uint8_t* src, int sw, int sh, int n, int c, float fx, float fy) {
    int x0 = (int)fx;
    int y0 = (int)fy;
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= sw) x1 = sw - 1;
    if (y1 >= sh) y1 = sh - 1;
    float tx = fx - x0;
    float ty = fy - y0;
    float v00 = src[(y0 * sw + x0) * n + c];
    float v10 = src[(y0 * sw + x1) * n + c];
    float v01 = src[(y1 * sw + x0) * n + c];
    float v11 = src[(y1 * sw + x1) * n + c];
    float a = v00 + (v10 - v00) * tx;
    float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * ty;
}

static uint8_t* resize_bilinear(const uint8_t* src, int sw, int sh, int n, int dw, int dh) {
    uint8_t* dst = (uint8_t*)malloc((size_t)dw * dh * n);
    if (!dst) return NULL;
    float sx = (float)sw / dw;
    float sy = (float)sh / dh;
    for (int y = 0; y < dh; y++) {
        float fy = (y + 0.5f) * sy - 0.5f;
        for (int x = 0; x < dw; x++) {
            float fx = (x + 0.5f) * sx - 0.5f;
            for (int c = 0; c < n; c++) {
                float v = sample_channel(src, sw, sh, n, c, fx, fy);
                if (v < 0.f) v = 0.f;
                if (v > 255.f) v = 255.f;
                dst[(y * dw + x) * n + c] = (uint8_t)v;
            }
        }
    }
    return dst;
}

static uint8_t* center_crop(const uint8_t* src, int sw, int sh, int n, int cw, int ch, int* ow, int* oh) {
    int x0 = (sw - cw) / 2;
    int y0 = (sh - ch) / 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    uint8_t* dst = (uint8_t*)malloc((size_t)cw * ch * n);
    if (!dst) return NULL;
    for (int y = 0; y < ch; y++) {
        memcpy(dst + y * cw * n, src + ((y0 + y) * sw + x0) * n, (size_t)cw * n);
    }
    *ow = cw;
    *oh = ch;
    return dst;
}

static uint8_t* to_rgb(const uint8_t* src, int w, int h, int n) {
    uint8_t* rgb = (uint8_t*)malloc((size_t)w * h * 3);
    if (!rgb) return NULL;
    for (int i = 0; i < w * h; i++) {
        rgb[i * 3 + 0] = src[i * n + 0];
        rgb[i * 3 + 1] = (n >= 2) ? src[i * n + 1] : src[i * n + 0];
        rgb[i * 3 + 2] = (n >= 3) ? src[i * n + 2] : src[i * n + 0];
    }
    return rgb;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: icon_convert <source.png> <out.png> <out.jpg>\n");
        return 1;
    }

    int sw, sh, n;
    uint8_t* loaded = stbi_load(argv[1], &sw, &sh, &n, 0);
    if (!loaded) {
        fprintf(stderr, "icon_convert: failed to load %s\n", argv[1]);
        return 1;
    }

    float scale = (float)OUT_SIZE / sw;
    if ((float)OUT_SIZE / sh > scale) scale = (float)OUT_SIZE / sh;
    int rw = (int)(sw * scale + 0.5f);
    int rh = (int)(sh * scale + 0.5f);
    if (rw < OUT_SIZE) rw = OUT_SIZE;
    if (rh < OUT_SIZE) rh = OUT_SIZE;

    uint8_t* resized = resize_bilinear(loaded, sw, sh, n, rw, rh);
    stbi_image_free(loaded);
    if (!resized) {
        fprintf(stderr, "icon_convert: resize failed\n");
        return 1;
    }

    int cw, ch;
    uint8_t* cropped = center_crop(resized, rw, rh, n, OUT_SIZE, OUT_SIZE, &cw, &ch);
    free(resized);
    if (!cropped) {
        fprintf(stderr, "icon_convert: crop failed\n");
        return 1;
    }

    uint8_t* rgb = to_rgb(cropped, OUT_SIZE, OUT_SIZE, n);
    free(cropped);
    if (!rgb) {
        fprintf(stderr, "icon_convert: rgb convert failed\n");
        return 1;
    }

    if (!stbi_write_png(argv[2], OUT_SIZE, OUT_SIZE, 3, rgb, OUT_SIZE * 3)) {
        fprintf(stderr, "icon_convert: failed to write %s\n", argv[2]);
        free(rgb);
        return 1;
    }
    if (!stbi_write_jpg(argv[3], OUT_SIZE, OUT_SIZE, 3, rgb, 90)) {
        fprintf(stderr, "icon_convert: failed to write %s\n", argv[3]);
        free(rgb);
        return 1;
    }

    free(rgb);
    return 0;
}
