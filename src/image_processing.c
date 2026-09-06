#include "image_processing.h"

#include <stdlib.h>
#include <string.h>

static unsigned char clamp_to_byte(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

void apply_grayscale(Image *img)
{
    size_t count = (size_t)img->width * img->height;
    for (size_t i = 0; i < count; ++i) {
        Pixel *p = &img->data[i];
        /* Standard luminance-weighted grayscale formula from the spec. */
        int gray = (int)(0.299 * p->r + 0.587 * p->g + 0.114 * p->b + 0.5);
        p->r = p->g = p->b = clamp_to_byte(gray);
    }
}

void apply_brightness(Image *img, int delta)
{
    size_t count = (size_t)img->width * img->height;
    for (size_t i = 0; i < count; ++i) {
        Pixel *p = &img->data[i];
        p->r = clamp_to_byte(p->r + delta);
        p->g = clamp_to_byte(p->g + delta);
        p->b = clamp_to_byte(p->b + delta);
    }
}

void apply_invert(Image *img)
{
    size_t count = (size_t)img->width * img->height;
    for (size_t i = 0; i < count; ++i) {
        img->data[i].r = 255 - img->data[i].r;
        img->data[i].g = 255 - img->data[i].g;
        img->data[i].b = 255 - img->data[i].b;
    }
}

void flip_horizontal(Image *img)
{
    for (int y = 0; y < img->height; ++y) {
        for (int x = 0; x < img->width / 2; ++x) {
            size_t left  = (size_t)y * img->width + x;
            size_t right = (size_t)y * img->width + (img->width - 1 - x);
            Pixel tmp = img->data[left];
            img->data[left] = img->data[right];
            img->data[right] = tmp;
        }
    }
}

void flip_vertical(Image *img)
{
    for (int y = 0; y < img->height / 2; ++y) {
        for (int x = 0; x < img->width; ++x) {
            size_t top    = (size_t)y * img->width + x;
            size_t bottom = (size_t)(img->height - 1 - y) * img->width + x;
            Pixel tmp = img->data[top];
            img->data[top] = img->data[bottom];
            img->data[bottom] = tmp;
        }
    }
}

Image *rotate90(const Image *img)
{
    /* Clockwise rotation: width and height swap, and pixel (x, y) moves
     * to (height - 1 - y, x) in the new image. */
    Image *out = image_create(img->height, img->width);
    if (!out)
        return NULL;

    for (int y = 0; y < img->height; ++y) {
        for (int x = 0; x < img->width; ++x) {
            int new_x = img->height - 1 - y;
            int new_y = x;
            out->data[(size_t)new_y * out->width + new_x] = img->data[(size_t)y * img->width + x];
        }
    }
    return out;
}

Image *crop_image(const Image *img, int x, int y, int width, int height)
{
    if (x < 0 || y < 0 || width <= 0 || height <= 0)
        return NULL;
    if (x >= img->width || y >= img->height)
        return NULL;
    if (width > img->width - x || height > img->height - y)
        return NULL;

    Image *out = image_create(width, height);
    if (!out)
        return NULL;

    for (int row = 0; row < height; ++row) {
        memcpy(&out->data[(size_t)row * width],
               &img->data[(size_t)(y + row) * img->width + x],
               (size_t)width * sizeof(Pixel));
    }
    return out;
}

int apply_blur(Image *img)
{
    size_t count = (size_t)img->width * img->height;
    Pixel *out = malloc(count * sizeof(Pixel));
    if (!out)
        return 0;

    for (int y = 0; y < img->height; ++y) {
        for (int x = 0; x < img->width; ++x) {
            int sum_r = 0, sum_g = 0, sum_b = 0, samples = 0;

            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < img->width && ny >= 0 && ny < img->height) {
                        Pixel p = img->data[(size_t)ny * img->width + nx];
                        sum_r += p.r;
                        sum_g += p.g;
                        sum_b += p.b;
                        ++samples;
                    }
                }
            }

            out[(size_t)y * img->width + x] = (Pixel){
                (unsigned char)(sum_r / samples),
                (unsigned char)(sum_g / samples),
                (unsigned char)(sum_b / samples)
            };
        }
    }

    memcpy(img->data, out, count * sizeof(Pixel));
    free(out);
    return 1;
}

int apply_sharpen(Image *img)
{
    static const int kernel[3][3] = {
        { 0, -1,  0 },
        { -1, 5, -1 },
        { 0, -1,  0 }
    };

    size_t count = (size_t)img->width * img->height;
    Pixel *out = malloc(count * sizeof(Pixel));
    if (!out)
        return 0;

    for (int y = 0; y < img->height; ++y) {
        for (int x = 0; x < img->width; ++x) {
            int sum_r = 0, sum_g = 0, sum_b = 0;

            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    /* Replicate the edge pixel instead of sampling
                     * outside the image bounds. */
                    int nx = x + kx, ny = y + ky;
                    if (nx < 0) nx = 0;
                    if (nx >= img->width) nx = img->width - 1;
                    if (ny < 0) ny = 0;
                    if (ny >= img->height) ny = img->height - 1;

                    int weight = kernel[ky + 1][kx + 1];
                    Pixel p = img->data[(size_t)ny * img->width + nx];
                    sum_r += weight * p.r;
                    sum_g += weight * p.g;
                    sum_b += weight * p.b;
                }
            }

            out[(size_t)y * img->width + x] = (Pixel){
                clamp_to_byte(sum_r),
                clamp_to_byte(sum_g),
                clamp_to_byte(sum_b)
            };
        }
    }

    memcpy(img->data, out, count * sizeof(Pixel));
    free(out);
    return 1;
}
