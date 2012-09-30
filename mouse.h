#ifndef __MOUSE_H__
#define __MOUSE_H__

#include "common.h"

extern int mouse_x, mouse_y, mouse_button;

#define MOUSE_CURSORS \
    /* id       code    filename      hotx hoty */ \
    X(NULL,     '@',    nullptr,         0,  0) \
    X(NORMAL,   'n',    "normal",        2,  2) \
    X(TURNL,    'l',    "drehlink",      2,  4) \
    X(TURNR,    'r',    "drehrech",     10,  4) \
    X(TURNU,    'u',    "umdrehen",      8, 12) \
    X(QUESTION, 'f',    "fragezei",      8,  8) \
    X(GRAB,     'g',    "grabsch",       8,  8) \
    X(TALK,     's',    "laber",         8, 14) \
    X(FORWARD,  'v',    "vorw\x8erts",   8,  4)

enum MouseCursor {
#define X(id, code, filename, hotx, hoty) MC_ ## id,
    MOUSE_CURSORS
#undef X
};

void set_mouse_cursor(MouseCursor which);
void render_mouse_cursor(U32 *dest, const U32 *pal);
MouseCursor get_mouse_cursor_from_char(char ch);

void init_mouse();
void shutdown_mouse();

#endif
