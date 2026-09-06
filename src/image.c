#include "image.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *out, size_t n, const char *msg)
{
    if (out && n)
        snprintf(out, n, "%s", msg);
}

/* --- Little-endian helpers for the BMP header fields --- */

static uint16_t read_u16(FILE *f, int *ok)
{
    unsigned char b[2];
    if (fread(b, 1, 2, f) != 2) { *ok = 0; return 0; }
    return (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
}

static uint32_t read_u32(FILE *f, int *ok)
{
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) { *ok = 0; return 0; }
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static int write_u16(FILE *f, uint16_t v)
{
    unsigned char b[2] = { (unsigned char)v, (unsigned char)(v >> 8) };
    return fwrite(b, 1, 2, f) == 2;
}

static int write_u32(FILE *f, uint32_t v)
{
    unsigned char b[4] = {
        (unsigned char)v, (unsigned char)(v >> 8),
        (unsigned char)(v >> 16), (unsigned char)(v >> 24)
    };
    return fwrite(b, 1, 4, f) == 4;
}

/* --- Allocation --- */

Image *image_create(int width, int height)
{
    if (width <= 0 || height <= 0)
        return NULL;
    if ((size_t)width > SIZE_MAX / (size_t)height)
        return NULL;

    size_t count = (size_t)width * (size_t)height;
    if (count > SIZE_MAX / sizeof(Pixel))
        return NULL;

    Image *img = malloc(sizeof(*img));
    if (!img)
        return NULL;

    img->data = calloc(count, sizeof(Pixel));
    if (!img->data) {
        free(img);
        return NULL;
    }

    img->width = width;
    img->height = height;
    return img;
}

Image *image_copy(const Image *src)
{
    if (!src || !src->data)
        return NULL;

    Image *dst = image_create(src->width, src->height);
    if (!dst)
        return NULL;

    size_t count = (size_t)src->width * (size_t)src->height;
    memcpy(dst->data, src->data, count * sizeof(Pixel));
    return dst;
}

void image_free(Image *img)
{
    if (!img)
        return;
    free(img->data);
    img->data = NULL;
    free(img);
}

size_t image_memory_bytes(const Image *img)
{
    if (!img)
        return 0;
    return sizeof(Image) + (size_t)img->width * (size_t)img->height * sizeof(Pixel);
}

/* --- BMP loading ---
 * Only 24-bit, uncompressed, BITMAPINFOHEADER (40-byte) BMP files are
 * accepted. Both bottom-up (positive height) and top-down (negative
 * height) row orders are supported; anything else is rejected with a
 * clear error message rather than crashing. */

Image *bmp_load(const char *path, char *error, size_t error_size)
{
    if (!path) {
        set_error(error, error_size, "No file path was provided.");
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        set_error(error, error_size, "Could not open the selected file.");
        return NULL;
    }

    int ok = 1;
    uint16_t signature = read_u16(f, &ok);
    (void)read_u32(f, &ok);          /* file size, unused */
    (void)read_u16(f, &ok);          /* reserved1 */
    (void)read_u16(f, &ok);          /* reserved2 */
    uint32_t pixel_offset = read_u32(f, &ok);
    uint32_t dib_header_size = read_u32(f, &ok);
    int32_t width = (int32_t)read_u32(f, &ok);
    int32_t height = (int32_t)read_u32(f, &ok);
    uint16_t planes = read_u16(f, &ok);
    uint16_t bits_per_pixel = read_u16(f, &ok);
    uint32_t compression = read_u32(f, &ok);

    int valid = ok && signature == 0x4D42u /* "BM" */
        && dib_header_size >= 40u
        && planes == 1u
        && bits_per_pixel == 24u
        && compression == 0u
        && width > 0
        && height != 0
        && height != INT32_MIN;

    if (!valid) {
        set_error(error, error_size, "Only 24-bit uncompressed BMP files are supported.");
        fclose(f);
        return NULL;
    }

    int top_down = height < 0;
    if (top_down)
        height = -height;

    Image *img = image_create((int)width, (int)height);
    if (!img) {
        set_error(error, error_size, "Not enough memory to load this image.");
        fclose(f);
        return NULL;
    }

    size_t row_bytes = (size_t)img->width * 3u;
    size_t padding = (4u - row_bytes % 4u) % 4u;

    if (fseek(f, (long)pixel_offset, SEEK_SET) != 0) {
        set_error(error, error_size, "Invalid BMP pixel-data offset.");
        goto fail;
    }

    for (int file_row = 0; file_row < img->height; ++file_row) {
        /* BMP rows are bottom-up by default: the first row in the file
         * is the bottom of the image unless the header says otherwise. */
        int y = top_down ? file_row : img->height - 1 - file_row;

        for (int x = 0; x < img->width; ++x) {
            unsigned char bgr[3];
            if (fread(bgr, 1, 3, f) != 3) {
                set_error(error, error_size, "Incomplete BMP pixel data.");
                goto fail;
            }
            Pixel *p = &img->data[(size_t)y * img->width + x];
            p->b = bgr[0];
            p->g = bgr[1];
            p->r = bgr[2];
        }

        if (padding) {
            unsigned char pad_bytes[3];
            if (fread(pad_bytes, 1, padding, f) != padding) {
                set_error(error, error_size, "Incomplete BMP row padding.");
                goto fail;
            }
        }
    }

    fclose(f);
    return img;

fail:
    fclose(f);
    image_free(img);
    return NULL;
}

/* --- BMP saving ---
 * Always written bottom-up, 24-bit, uncompressed, with a standard
 * 40-byte BITMAPINFOHEADER, so every file this program writes can also
 * be read back by itself (and by any standard BMP viewer). */

int bmp_save(const char *path, const Image *img, char *error, size_t error_size)
{
    if (!path || !img || !img->data) {
        set_error(error, error_size, "No image is available to save.");
        return 0;
    }

    size_t row_bytes = (size_t)img->width * 3u;
    size_t padding = (4u - row_bytes % 4u) % 4u;
    uint64_t pixel_data_size = (uint64_t)(row_bytes + padding) * (uint64_t)img->height;
    uint64_t total_size = 54u + pixel_data_size;

    if (total_size > UINT32_MAX) {
        set_error(error, error_size, "Image is too large for this BMP writer.");
        return 0;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        set_error(error, error_size, "Could not create the output file.");
        return 0;
    }

#define CHECKED(expr) \
    do { if (!(expr)) { set_error(error, error_size, "Failed while writing the BMP file."); fclose(f); return 0; } } while (0)

    CHECKED(write_u16(f, 0x4D42));                     /* "BM" signature   */
    CHECKED(write_u32(f, (uint32_t)total_size));        /* file size        */
    CHECKED(write_u16(f, 0));                           /* reserved1        */
    CHECKED(write_u16(f, 0));                           /* reserved2        */
    CHECKED(write_u32(f, 54));                          /* pixel data offset*/
    CHECKED(write_u32(f, 40));                          /* DIB header size  */
    CHECKED(write_u32(f, (uint32_t)img->width));
    CHECKED(write_u32(f, (uint32_t)img->height));
    CHECKED(write_u16(f, 1));                           /* color planes     */
    CHECKED(write_u16(f, 24));                          /* bits per pixel   */
    CHECKED(write_u32(f, 0));                           /* no compression   */
    CHECKED(write_u32(f, (uint32_t)pixel_data_size));
    CHECKED(write_u32(f, 2835));                         /* ~72 DPI          */
    CHECKED(write_u32(f, 2835));
    CHECKED(write_u32(f, 0));                           /* palette colors   */
    CHECKED(write_u32(f, 0));                           /* important colors */

    unsigned char zeros[3] = { 0, 0, 0 };
    for (int y = img->height - 1; y >= 0; --y) {
        for (int x = 0; x < img->width; ++x) {
            Pixel p = img->data[(size_t)y * img->width + x];
            unsigned char bgr[3] = { p.b, p.g, p.r };
            CHECKED(fwrite(bgr, 1, 3, f) == 3);
        }
        if (padding)
            CHECKED(fwrite(zeros, 1, padding, f) == padding);
    }

#undef CHECKED

    if (fclose(f) != 0) {
        set_error(error, error_size, "Could not finalize the BMP file.");
        return 0;
    }
    return 1;
}
