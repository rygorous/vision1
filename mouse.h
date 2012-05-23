#ifndef __MOUSE_H__
#define __MOUSE_H__

#include "common.h"

extern int mouse_x, mouse_y, mouse_button;

#define MOUSE_CURSORS \
    /* id       filename        hotx/y */ \
    X(NULL,     nullptr,        8, 8) \
    X(NORMAL,   "normal",       2, 2) \
    X(TURNL,    "drehlink",     8, 8) \
    X(TURNR,    "drehrech",     8, 8) \
    X(TURNU,    "umdrehen",     8, 8) \
    X(QUESTION, "fragezei",     8, 8) \
    X(GRAB,     "grabsch",      8, 8) \
    X(TALK,     "laber",        8, 8) \
    X(FORWARD,  "vorw\x8erts",  8, 8)

enum MouseCursor {
#define X(id, filename, hotx, hoty) MC_ ## id,
    MOUSE_CURSORS
#undef X
};

void set_mouse_cursor(MouseCursor which);
void render_mouse_cursor(U32 *dest, const U32 *pal);

void init_mouse();
void shutdown_mouse();

#endif
