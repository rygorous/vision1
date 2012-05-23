#include "mouse.h"
#include "util.h"

int mouse_x, mouse_y, mouse_button;

static Slice pointer_gfx;

void init_mouse()
{
    pointer_gfx = read_file("grafix/pointers.gra");
}

void shutdown_mouse()
{
    pointer_gfx = Slice();
}
