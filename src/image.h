#ifndef IMAGE_H
#define IMAGE_H

#include <stddef.h>

/* One pixel: three 8-bit color channels, as suggested by the assignment. */
typedef struct {
    unsigned char r, g, b;
} Pixel;

/* A complete image: dimensions plus a dynamically allocated pixel array.
 * Pixel (x, y) is stored at data[y * width + x]. */
typedef struct {
    int width, height;
    Pixel *data;
} Image;

Image *image_create(int width, int height);
Image *image_copy(const Image *source);
void image_free(Image *image);
size_t image_memory_bytes(const Image *image);

/* Reads/writes 24-bit uncompressed BMP files only, per the assignment spec. */
Image *bmp_load(const char *path, char *error, size_t error_size);
int bmp_save(const char *path, const Image *image, char *error, size_t error_size);

#endif
