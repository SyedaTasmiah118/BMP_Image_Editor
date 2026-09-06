#include "image.h"
#include "image_processing.h"
#include "undo.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    char error[128];

    /* Create a tiny 2x2 image with known pixel values. */
    Image *original = image_create(2, 2);
    assert(original);
    original->data[0] = (Pixel){255, 0, 0};
    original->data[1] = (Pixel){0, 255, 0};
    original->data[2] = (Pixel){0, 0, 255};
    original->data[3] = (Pixel){10, 20, 30};

    /* Round-trip through BMP save/load. */
    assert(bmp_save("tests/test.bmp", original, error, sizeof error));
    Image *reloaded = bmp_load("tests/test.bmp", error, sizeof error);
    assert(reloaded);

    /* Undo stack: push, apply an edit, pop back, then confirm the
     * stack is now empty. */
    assert(undo_push(reloaded));
    apply_grayscale(reloaded);
    assert(undo_pop(&reloaded));
    assert(!undo_pop(&reloaded));

    /* Convolution-style filters should both succeed. */
    assert(apply_blur(reloaded));
    assert(apply_sharpen(reloaded));

    /* Geometry operations. */
    Image *rotated = rotate90(reloaded);
    assert(rotated);
    assert(rotated->width == reloaded->height && rotated->height == reloaded->width);

    Image *cropped = crop_image(reloaded, 0, 0, 1, 1);
    assert(cropped);
    assert(cropped->width == 1 && cropped->height == 1);

    image_free(original);
    image_free(reloaded);
    image_free(rotated);
    image_free(cropped);
    undo_clear();

    puts("All core tests passed.");
    return 0;
}
