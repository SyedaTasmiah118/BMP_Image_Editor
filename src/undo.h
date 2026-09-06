#ifndef UNDO_H
#define UNDO_H

#include "image.h"

/* A linked-list stack of deep-copied images. Supports unlimited levels
 * of Undo (the assignment only requires one, but a full stack costs
 * little extra code and is easy to explain in the viva). */

int undo_push(const Image *image);
int undo_pop(Image **image);
int undo_count(void);
size_t undo_memory_bytes(void);
void undo_clear(void);

#endif
