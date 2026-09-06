#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include "image.h"

/* All operations below manipulate raw pixel data directly, as required
 * by the assignment -- none of them call a ready-made image-processing
 * function from an external library. */

void apply_grayscale(Image *image);
void apply_brightness(Image *image, int delta);
void apply_invert(Image *image);
void flip_horizontal(Image *image);
void flip_vertical(Image *image);

/* These return a new image (dimensions change or a new buffer is
 * required); the caller is responsible for freeing the result. */
Image *rotate90(const Image *image);
Image *crop_image(const Image *image, int x, int y, int width, int height);

/* Convolution-style filters use a separate output buffer internally so
 * that newly computed pixels never influence later ones. Return 0 on
 * allocation failure, 1 on success. */
int apply_blur(Image *image);
int apply_sharpen(Image *image); /* optional bonus feature */

#endif
