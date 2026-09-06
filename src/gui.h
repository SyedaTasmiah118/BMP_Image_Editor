#ifndef GUI_H
#define GUI_H

#include "iup.h"

/* Builds the whole application window (toolbar, canvas, status bar)
 * and wires up every callback. main.c only calls this once. */
Ihandle *create_main_dialog(void);

/* Releases everything gui.c owns (current image, preview image, undo
 * history). Call once after the IUP main loop returns. */
void gui_cleanup(void);

#endif
