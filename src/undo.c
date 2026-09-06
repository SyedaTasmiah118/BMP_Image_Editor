#include "undo.h"

#include <stdlib.h>

typedef struct UndoNode {
    Image *image;
    struct UndoNode *next;
} UndoNode;

static UndoNode *undo_top = NULL;
static int stored_states = 0;
static size_t stored_bytes = 0;

/* Deep-copies the given image and pushes it onto the stack. Call this
 * BEFORE applying an operation, so it captures the "before" state. */
int undo_push(const Image *image)
{
    if (!image || !image->data)
        return 0;

    Image *snapshot = image_copy(image);
    if (!snapshot)
        return 0;

    UndoNode *node = malloc(sizeof(*node));
    if (!node) {
        image_free(snapshot);
        return 0;
    }

    node->image = snapshot;
    node->next = undo_top;
    undo_top = node;
    ++stored_states;
    stored_bytes += image_memory_bytes(snapshot);
    return 1;
}

/* Pops the most recent snapshot, frees the image it replaces, and
 * returns 1. Returns 0 (without touching *image) if the stack is empty. */
int undo_pop(Image **image)
{
    if (!image || !undo_top)
        return 0;

    UndoNode *node = undo_top;
    undo_top = node->next;
    --stored_states;
    stored_bytes -= image_memory_bytes(node->image);

    image_free(*image);
    *image = node->image;
    free(node);
    return 1;
}

int undo_count(void)
{
    return stored_states;
}

size_t undo_memory_bytes(void)
{
    return stored_bytes;
}

void undo_clear(void)
{
    while (undo_top) {
        UndoNode *node = undo_top;
        undo_top = node->next;
        image_free(node->image);
        free(node);
    }
    stored_states = 0;
    stored_bytes = 0;
}
