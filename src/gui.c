#include "gui.h"

#include "image.h"
#include "image_processing.h"
#include "undo.h"
#include "iupdraw.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIEW_MARGIN 12
#define LARGE_IMAGE_WARNING_BYTES (64u * 1024u * 1024u)

/* --- Application state ---
 * A single currently-open image, plus the widgets that need to be
 * updated whenever it changes. Kept as static/file-local state so the
 * rest of the program only ever talks to gui.c through gui.h. */
static Image *current = NULL;
static Ihandle *canvas = NULL;
static Ihandle *status_label = NULL;
static int modified = 0;

/* A scaled-down preview image is cached here so the canvas does not
 * have to resample pixels on every single redraw -- only when the
 * image or the canvas size actually changes. */
static Ihandle *preview = NULL;
static char preview_handle_name[64] = "";
static int preview_w = 0, preview_h = 0;
static int cached_canvas_w = -1, cached_canvas_h = -1;
static unsigned long preview_generation = 0;

/* ---------------------------------------------------------------- */
/* Status bar and preview management                                */
/* ---------------------------------------------------------------- */

static void update_status(const char *message)
{
    char text[200];
    if (current) {
        snprintf(text, sizeof text, "%s | %d x %d | Undo: %d%s",
                 message ? message : "Ready",
                 current->width, current->height,
                 undo_count(),
                 modified ? " | Unsaved changes" : "");
    } else {
        snprintf(text, sizeof text, "No image loaded");
    }
    IupSetStrAttribute(status_label, "TITLE", text);
}

static void destroy_preview(void)
{
    if (preview_handle_name[0])
        IupSetHandle(preview_handle_name, NULL);
    preview_handle_name[0] = '\0';

    if (preview)
        IupDestroy(preview);
    preview = NULL;
    preview_w = preview_h = 0;
}

/* Rebuilds the cached preview image to fit inside a canvas_w x canvas_h
 * area. Images that already fit are shown at 1:1 (never upscaled);
 * larger images are downsampled with simple nearest-neighbor sampling.
 * Returns 0 only on allocation failure. */
static int build_preview(int canvas_w, int canvas_h)
{
    int available_w = canvas_w - 2 * VIEW_MARGIN;
    int available_h = canvas_h - 2 * VIEW_MARGIN;
    if (!current || available_w < 1 || available_h < 1)
        return 0;

    int target_w = current->width;
    int target_h = current->height;

    if (target_w > available_w || target_h > available_h) {
        float scale_w = (float)available_w / (float)target_w;
        float scale_h = (float)available_h / (float)target_h;
        float scale = scale_w < scale_h ? scale_w : scale_h;
        target_w = (int)(target_w * scale);
        target_h = (int)(target_h * scale);
    }
    if (target_w < 1) target_w = 1;
    if (target_h < 1) target_h = 1;

    /* Nothing changed since the last build: keep the cached preview. */
    if (preview && preview_w == target_w && preview_h == target_h &&
        cached_canvas_w == canvas_w && cached_canvas_h == canvas_h)
        return 1;

    unsigned char *rgb = malloc((size_t)target_w * target_h * 3u);
    if (!rgb)
        return 0;

    for (int y = 0; y < target_h; ++y) {
        int source_y = y * current->height / target_h;
        for (int x = 0; x < target_w; ++x) {
            int source_x = x * current->width / target_w;
            Pixel p = current->data[(size_t)source_y * current->width + source_x];
            size_t d = ((size_t)y * target_w + x) * 3u;
            rgb[d] = p.r;
            rgb[d + 1] = p.g;
            rgb[d + 2] = p.b;
        }
    }

    Ihandle *new_preview = IupImageRGB(target_w, target_h, rgb);
    free(rgb);
    if (!new_preview)
        return 0;

    destroy_preview();
    ++preview_generation;
    snprintf(preview_handle_name, sizeof preview_handle_name, "IMAGE_PREVIEW_%lu", preview_generation);
    IupSetHandle(preview_handle_name, new_preview);

    preview = new_preview;
    preview_w = target_w;
    preview_h = target_h;
    cached_canvas_w = canvas_w;
    cached_canvas_h = canvas_h;
    return 1;
}

/* ---------------------------------------------------------------- */
/* Canvas drawing                                                    */
/* ---------------------------------------------------------------- */

static int canvas_action_cb(Ihandle *c)
{
    int w = 0, h = 0;
    IupDrawBegin(c);
    IupDrawGetSize(c, &w, &h);

    IupSetAttribute(c, "DRAWCOLOR", "255 255 255");
    IupSetAttribute(c, "DRAWSTYLE", "FILL");
    if (w > 0 && h > 0)
        IupDrawRectangle(c, 0, 0, w - 1, h - 1);

    if (current && build_preview(w, h)) {
        IupDrawImage(c, preview_handle_name, (w - preview_w) / 2, (h - preview_h) / 2, 0, 0);
    } else if (!current) {
        const char *message = "Open a 24-bit uncompressed BMP image to begin.";
        int text_w = 0, text_h = 0;
        IupSetAttribute(c, "DRAWCOLOR", "70 70 70");
        IupDrawGetTextSize(c, message, -1, &text_w, &text_h);
        IupDrawText(c, message, -1, (w - text_w) / 2, (h - text_h) / 2, text_w, text_h);
    }

    IupDrawEnd(c);
    return IUP_DEFAULT;
}

static int canvas_resize_cb(Ihandle *c, int w, int h)
{
    (void)c; (void)w; (void)h;
    /* The canvas changed size, so the cached preview no longer applies. */
    destroy_preview();
    cached_canvas_w = cached_canvas_h = -1;
    IupRedraw(canvas, 1);
    return IUP_DEFAULT;
}

static void refresh(const char *status_message)
{
    destroy_preview();
    cached_canvas_w = cached_canvas_h = -1;
    update_status(status_message);
    IupRedraw(canvas, 1);
    IupUpdate(canvas);
}

static void reset_session(void)
{
    destroy_preview();
    image_free(current);
    current = NULL;
    undo_clear();
    modified = 0;
    refresh(NULL);
}

/* ---------------------------------------------------------------- */
/* Shared guards used by every editing callback                      */
/* ---------------------------------------------------------------- */

static int require_image(Ihandle *self)
{
    if (current)
        return 1;
    IupMessageError(IupGetDialog(self), "Open a 24-bit uncompressed BMP first.");
    return 0;
}

/* Pushes the current image onto the Undo stack before an edit is
 * applied. If this fails (out of memory), the edit is not attempted,
 * and -- importantly -- any EARLIER undo history is left untouched. */
static int prepare_edit(Ihandle *self)
{
    if (!require_image(self))
        return 0;
    if (!undo_push(current)) {
        IupMessageError(IupGetDialog(self), "Not enough memory to create the Undo copy.");
        return 0;
    }
    return 1;
}

static int add_bmp_extension(char *path, size_t capacity)
{
    size_t len = strlen(path);
    if (len >= 4 &&
        path[len - 4] == '.' &&
        tolower((unsigned char)path[len - 3]) == 'b' &&
        tolower((unsigned char)path[len - 2]) == 'm' &&
        tolower((unsigned char)path[len - 1]) == 'p')
        return 1;
    if (len + 4 >= capacity)
        return 0;
    strcat(path, ".bmp");
    return 1;
}

/* ---------------------------------------------------------------- */
/* File operations                                                   */
/* ---------------------------------------------------------------- */

static int save_file(Ihandle *self)
{
    Ihandle *dialog = IupFileDlg();
    IupSetAttribute(dialog, "DIALOGTYPE", "SAVE");
    IupSetAttribute(dialog, "TITLE", "Save Edited BMP");
    IupSetAttribute(dialog, "FILTER", "*.bmp");
    IupSetAttribute(dialog, "FILTERINFO", "24-bit BMP Images");
    IupSetAttribute(dialog, "EXTDEFAULT", "bmp");
    IupSetAttribute(dialog, "NOCHANGEDIR", "YES");
    IupSetAttributeHandle(dialog, "PARENTDIALOG", IupGetDialog(self));
    IupPopup(dialog, IUP_CENTERPARENT, IUP_CENTERPARENT);

    if (IupGetInt(dialog, "STATUS") == -1) {
        IupDestroy(dialog);
        return 0;
    }

    const char *value = IupGetAttribute(dialog, "VALUE");
    char path[4096];
    snprintf(path, sizeof path, "%s", value ? value : "");
    IupDestroy(dialog);

    char error[256] = "Unknown error.";
    if (!add_bmp_extension(path, sizeof path) || !bmp_save(path, current, error, sizeof error)) {
        IupMessageError(IupGetDialog(self), error);
        return 0;
    }

    modified = 0;
    update_status("Saved");
    return 1;
}

/* If there are unsaved changes, asks the user whether to save them,
 * discard them, or cancel. Returns 1 if it is safe to proceed with
 * whatever the caller was about to do (open a new file, close, etc.). */
static int protect_unsaved(Ihandle *self)
{
    if (!modified)
        return 1;

    int choice = IupAlarm("Unsaved Changes",
                           "The current image has unsaved changes.",
                           "Save Changes", "Discard", "Cancel");
    if (choice == 1)
        return save_file(self);
    return choice == 2;
}

static int open_cb(Ihandle *self)
{
    if (!protect_unsaved(self))
        return IUP_DEFAULT;

    Ihandle *dialog = IupFileDlg();
    IupSetAttribute(dialog, "DIALOGTYPE", "OPEN");
    IupSetAttribute(dialog, "TITLE", "Open BMP Image");
    IupSetAttribute(dialog, "FILTER", "*.bmp");
    IupSetAttribute(dialog, "FILTERINFO", "24-bit BMP Images");
    IupSetAttribute(dialog, "ALLOWNEW", "NO");
    IupSetAttribute(dialog, "NOCHANGEDIR", "YES");
    IupSetAttributeHandle(dialog, "PARENTDIALOG", IupGetDialog(self));
    IupPopup(dialog, IUP_CENTERPARENT, IUP_CENTERPARENT);

    if (IupGetInt(dialog, "STATUS") == -1) {
        IupDestroy(dialog);
        return IUP_DEFAULT;
    }

    const char *value = IupGetAttribute(dialog, "VALUE");
    char path[4096];
    snprintf(path, sizeof path, "%s", value ? value : "");
    IupDestroy(dialog);

    char error[256] = "Unknown error.";
    Image *loaded = bmp_load(path, error, sizeof error);
    if (!loaded) {
        IupMessageError(IupGetDialog(self), error);
        return IUP_DEFAULT;
    }

    if (image_memory_bytes(loaded) > LARGE_IMAGE_WARNING_BYTES) {
        char warning[240];
        snprintf(warning, sizeof warning,
                 "This image uses about %.1f MB per Undo snapshot. Continue?",
                 image_memory_bytes(loaded) / (1024.0 * 1024.0));
        if (IupAlarm("Large Image Warning", warning, "Continue", "Cancel", NULL) != 1) {
            image_free(loaded);
            return IUP_DEFAULT;
        }
    }

    image_free(current);
    current = loaded;
    undo_clear();
    modified = 0;
    refresh("Opened");
    return IUP_DEFAULT;
}

static int save_cb(Ihandle *self)
{
    if (!require_image(self) || !save_file(self))
        return IUP_DEFAULT;

    int choice = IupAlarm("Image Saved", "Choose the next action.",
                           "Save and Exit", "Keep Editing", "Start New");
    if (choice == 1)
        return IUP_CLOSE;
    if (choice == 3)
        reset_session();
    return IUP_DEFAULT;
}

static int new_cb(Ihandle *self)
{
    if (current && protect_unsaved(self) &&
        IupAlarm("Start New", "Remove the image and all Undo history?", "Start New", "Cancel", NULL) == 1)
        reset_session();
    return IUP_DEFAULT;
}

static int undo_cb(Ihandle *self)
{
    if (!require_image(self))
        return IUP_DEFAULT;

    if (!undo_pop(&current)) {
        IupMessage("Undo", "No earlier image remains in the Undo history.");
    } else {
        modified = 1;
        refresh("Undo");
    }
    return IUP_DEFAULT;
}

static int clear_history_cb(Ihandle *self)
{
    (void)self;
    if (!undo_count()) {
        IupMessage("Undo History", "Undo history is already empty.");
    } else if (IupAlarm("Clear History", "This cannot be undone.", "Clear", "Cancel", NULL) == 1) {
        undo_clear();
        update_status("History cleared");
    }
    return IUP_DEFAULT;
}

/* ---------------------------------------------------------------- */
/* Image manipulation callbacks                                      */
/* ---------------------------------------------------------------- */

/* Most operations follow the same pattern: push Undo state, apply the
 * in-place algorithm, mark the document modified, refresh the canvas. */
#define SIMPLE_EDIT_CALLBACK(NAME, FUNC, STATUS_TEXT)             \
    static int NAME(Ihandle *self)                                \
    {                                                              \
        if (prepare_edit(self)) {                                  \
            FUNC(current);                                        \
            modified = 1;                                          \
            refresh(STATUS_TEXT);                                  \
        }                                                          \
        return IUP_DEFAULT;                                        \
    }

SIMPLE_EDIT_CALLBACK(grayscale_cb, apply_grayscale, "Grayscale")
SIMPLE_EDIT_CALLBACK(invert_cb, apply_invert, "Inverted")
SIMPLE_EDIT_CALLBACK(hflip_cb, flip_horizontal, "Flipped horizontally")
SIMPLE_EDIT_CALLBACK(vflip_cb, flip_vertical, "Flipped vertically")

static int brightness_cb(Ihandle *self)
{
    char input[40] = "20";
    if (!require_image(self) || !IupGetText("Brightness (-255 to 255)", input, sizeof input))
        return IUP_DEFAULT;

    errno = 0;
    char *end;
    long value = strtol(input, &end, 10);
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')
        ++end;

    if (errno || end == input || *end || value < -255 || value > 255) {
        IupMessageError(IupGetDialog(self), "Brightness must be an integer from -255 to 255.");
        return IUP_DEFAULT;
    }

    if (prepare_edit(self)) {
        apply_brightness(current, (int)value);
        modified = 1;
        refresh("Brightness adjusted");
    }
    return IUP_DEFAULT;
}

/* Operations below replace `current` with a brand-new image, so on
 * failure the Undo entry that prepare_edit() just pushed is simply
 * left in place -- it just means "undo brings back the same image",
 * which is harmless. The rest of the Undo history is never wiped. */

static int rotate_cb(Ihandle *self)
{
    if (!prepare_edit(self))
        return IUP_DEFAULT;

    Image *rotated = rotate90(current);
    if (!rotated) {
        IupMessageError(IupGetDialog(self), "Not enough memory to rotate the image.");
        return IUP_DEFAULT;
    }

    image_free(current);
    current = rotated;
    modified = 1;
    refresh("Rotated 90 degrees");
    return IUP_DEFAULT;
}

static int crop_cb(Ihandle *self)
{
    char input[100] = "0 0 100 100";
    if (!require_image(self) || !IupGetText("Crop: x y width height", input, sizeof input))
        return IUP_DEFAULT;

    int x, y, w, h;
    char extra;
    if (sscanf(input, " %d %d %d %d %c", &x, &y, &w, &h, &extra) != 4 ||
        x < 0 || y < 0 || w <= 0 || h <= 0 ||
        x >= current->width || y >= current->height ||
        w > current->width - x || h > current->height - y) {
        IupMessageError(IupGetDialog(self), "Invalid crop rectangle. Values must stay inside the image.");
        return IUP_DEFAULT;
    }

    if (!prepare_edit(self))
        return IUP_DEFAULT;

    Image *cropped = crop_image(current, x, y, w, h);
    if (!cropped) {
        IupMessageError(IupGetDialog(self), "Not enough memory to crop the image.");
        return IUP_DEFAULT;
    }

    image_free(current);
    current = cropped;
    modified = 1;
    refresh("Cropped");
    return IUP_DEFAULT;
}

static int blur_cb(Ihandle *self)
{
    if (!prepare_edit(self))
        return IUP_DEFAULT;

    if (!apply_blur(current)) {
        IupMessageError(IupGetDialog(self), "Not enough memory to blur the image.");
    } else {
        modified = 1;
        refresh("Blurred");
    }
    return IUP_DEFAULT;
}

static int sharpen_cb(Ihandle *self)
{
    if (!prepare_edit(self))
        return IUP_DEFAULT;

    if (!apply_sharpen(current)) {
        IupMessageError(IupGetDialog(self), "Not enough memory to sharpen the image.");
    } else {
        modified = 1;
        refresh("Sharpened");
    }
    return IUP_DEFAULT;
}

/* ---------------------------------------------------------------- */
/* Informational dialogs                                             */
/* ---------------------------------------------------------------- */

static int info_cb(Ihandle *self)
{
    if (!require_image(self))
        return IUP_DEFAULT;

    char text[400];
    snprintf(text, sizeof text,
             "Width: %d\nHeight: %d\nPixels: %llu\n"
             "Image memory: %.2f MB\nUndo states: %d\nUndo memory: %.2f MB\n"
             "Modified: %s",
             current->width, current->height,
             (unsigned long long)current->width * current->height,
             image_memory_bytes(current) / (1024.0 * 1024.0),
             undo_count(),
             undo_memory_bytes() / (1024.0 * 1024.0),
             modified ? "Yes" : "No");
    IupMessage("Image Information", text);
    return IUP_DEFAULT;
}

static int help_cb(Ihandle *self)
{
    (void)self;
    IupMessage("Help",
               "Open a 24-bit uncompressed BMP.\n"
               "Brightness: enter a value from -255 to 255.\n"
               "Crop: enter \"x y width height\".\n"
               "The image is shown at 1:1 when space allows, "
               "and scaled down (never up) only when necessary.");
    return IUP_DEFAULT;
}

static int about_cb(Ihandle *self)
{
    (void)self;
    IupMessage("About", "IUP BMP Image Editor\nBuilt with structured C99 and IUP.");
    return IUP_DEFAULT;
}

static int close_cb(Ihandle *dialog)
{
    if (!modified)
        return IupAlarm("Exit", "Exit Image Editor?", "Exit", "Cancel", NULL) == 1
                   ? IUP_CLOSE : IUP_IGNORE;

    int choice = IupAlarm("Unsaved Changes", "Save before exit?",
                           "Save and Exit", "Discard and Exit", "Cancel");
    if (choice == 1)
        return save_file(dialog) ? IUP_CLOSE : IUP_IGNORE;
    if (choice == 2)
        return IUP_CLOSE;
    return IUP_IGNORE;
}

/* ---------------------------------------------------------------- */
/* Layout                                                             */
/* ---------------------------------------------------------------- */

static Ihandle *toolbar_button(const char *title, Icallback callback)
{
    Ihandle *button = IupButton(title);
    IupSetCallback(button, "ACTION", callback);
    IupSetAttribute(button, "PADDING", "8x4");
    return button;
}

Ihandle *create_main_dialog(void)
{
    Ihandle *file_row = IupHbox(
        IupFill(),
        toolbar_button("Open", (Icallback)open_cb),
        toolbar_button("Save", (Icallback)save_cb),
        toolbar_button("Start New", (Icallback)new_cb),
        toolbar_button("Undo", (Icallback)undo_cb),
        toolbar_button("Clear History", (Icallback)clear_history_cb),
        toolbar_button("Image Info", (Icallback)info_cb),
        IupFill(),
        NULL);

    Ihandle *edit_row = IupHbox(
        IupFill(),
        toolbar_button("Grayscale", (Icallback)grayscale_cb),
        toolbar_button("Brightness", (Icallback)brightness_cb),
        toolbar_button("Invert", (Icallback)invert_cb),
        toolbar_button("Flip Horizontal", (Icallback)hflip_cb),
        toolbar_button("Flip Vertical", (Icallback)vflip_cb),
        toolbar_button("Rotate 90 CW", (Icallback)rotate_cb),
        toolbar_button("Crop", (Icallback)crop_cb),
        toolbar_button("Blur 3x3", (Icallback)blur_cb),
        toolbar_button("Sharpen", (Icallback)sharpen_cb),
        toolbar_button("Help", (Icallback)help_cb),
        toolbar_button("About", (Icallback)about_cb),
        IupFill(),
        NULL);

    IupSetAttribute(file_row, "GAP", "6");
    IupSetAttribute(edit_row, "GAP", "6");

    Ihandle *toolbar = IupVbox(file_row, edit_row, NULL);
    IupSetAttribute(toolbar, "GAP", "8");

    canvas = IupCanvas();
    IupSetAttribute(canvas, "EXPAND", "YES");
    IupSetAttribute(canvas, "BGCOLOR", "255 255 255");
    IupSetCallback(canvas, "ACTION", (Icallback)canvas_action_cb);
    /* RESIZE_CB is actually invoked by IUP as int(Ihandle*, int, int), not
     * as plain Icallback -- this is normal for IUP's C API (every
     * callback name shares one generic pointer type, and IUP internally
     * calls it with the right number of arguments for that callback).
     * The intermediate void(*)(void) cast documents that this mismatch
     * is intentional, so it no longer trips -Wcast-function-type-mismatch. */
    IupSetCallback(canvas, "RESIZE_CB", (Icallback)(void (*)(void))canvas_resize_cb);

    status_label = IupLabel("No image loaded");
    IupSetAttribute(status_label, "ALIGNMENT", "ACENTER");
    IupSetAttribute(status_label, "EXPAND", "HORIZONTAL");

    Ihandle *layout = IupVbox(toolbar, canvas, status_label, NULL);
    IupSetAttribute(layout, "MARGIN", "12x10");
    IupSetAttribute(layout, "GAP", "7");

    Ihandle *dialog = IupDialog(layout);
    IupSetAttribute(dialog, "TITLE", "IUP BMP Image Editor");
    IupSetAttribute(dialog, "RASTERSIZE", "900x650");
    IupSetAttribute(dialog, "MINSIZE", "600x400");
    IupSetCallback(dialog, "CLOSE_CB", (Icallback)close_cb);

    return dialog;
}

void gui_cleanup(void)
{
    destroy_preview();
    image_free(current);
    current = NULL;
    undo_clear();
    canvas = NULL;
    status_label = NULL;
}
