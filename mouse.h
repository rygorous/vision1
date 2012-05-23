#ifndef __MOUSE_H__
#define __MOUSE_H__

#include "common.h"

extern int mouse_x, mouse_y, mouse_button;

void render_mouse_cursor(U32 *dest, const U32 *pal);

void init_mouse();
void shutdown_mouse();

#endif
