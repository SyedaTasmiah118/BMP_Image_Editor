#include "gui.h"
#include "iup.h"

int main(int argc, char **argv)
{
    if (IupOpen(&argc, &argv) != IUP_NOERROR)
        return 1;

    Ihandle *dialog = create_main_dialog();
    if (!dialog) {
        IupClose();
        return 1;
    }

    IupShowXY(dialog, IUP_CENTER, IUP_CENTER);
    IupMainLoop();

    gui_cleanup();
    IupDestroy(dialog);
    IupClose();
    return 0;
}
