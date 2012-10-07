#include "mouse.h"
#include "util.h"
#include "graphics.h"
#include "str.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

int mouse_x, mouse_y, mouse_button;

namespace {
    struct CursorImg {
        U8 img[16][16];
        int hotx, hoty;
    };

    static const struct CursorDesc {
        const char *filename;
        int hotx, hoty;
        char code;
    } cursor_desc[] = {
#define X(id, code, filename, hotx, hoty) { filename, hotx, hoty, code },
        MOUSE_CURSORS
#undef X
    };
}

static CursorImg cursors[ARRAY_COUNT(cursor_desc)];
static MouseCursor cur_cursor;

void set_mouse_cursor(MouseCursor which)
{
    cur_cursor = which;
}

void render_mouse_cursor(U32 *dest, const U32 *pal)
{
    // clip (in cursor space)
    const CursorImg &cursor = cursors[cur_cursor];
    int rel_x = mouse_x - cursor.hotx;
    int rel_y = mouse_y - cursor.hoty;
    int x0 = std::max(0, 0 - rel_x);
    int y0 = std::max(0, 0 - rel_y);
    int x1 = std::min(16, vga_screen.width() - rel_x);
    int y1 = std::min(16, vga_screen.height() - rel_y);
    if (x0 >= x1 || y0 >= y1)
        return;

    int width = vga_screen.width();
    int offs = rel_y * width + rel_x;
    for (int y=y0; y < y1; y++) {
        for (int x=x0; x < x1; x++) {
            if (cursor.img[y][x])
                dest[offs + y*width + x] = pal[cursor.img[y][x]];
        }
    }
}

MouseCursor get_mouse_cursor_from_char(char ch)
{
    MouseCursor ret = MC_NULL;
    ch = tolower(ch);
    for (int i=0; i < ARRAY_COUNT(cursor_desc); i++)
        if (cursor_desc[i].code == ch)
            return (MouseCursor)i;
    return ret;
}

void mouse_init()
{
    set_mouse_cursor(MC_NORMAL);
    Slice gfx = read_file("grafix/pointers.gra");

    for (int i=0; i < ARRAY_COUNT(cursor_desc); i++) {
        PixelSlice img = PixelSlice::black(16, 16);

        if (cursor_desc[i].filename) {
            U8 type;
            int offs = find_gra_item(gfx, cursor_desc[i].filename, &type);
            if (offs == -1 || type != 5)
                panic("error finding cursor image '%s'\n", cursor_desc[i].filename);

            img = load_delta_pixels(gfx(offs));
        }

        CursorImg &cursor = cursors[i];
        for (int y=0; y < 16; y++)
            memcpy(cursor.img[y], img.ptr(0, y), 16);
        cursor.hotx = cursor_desc[i].hotx;
        cursor.hoty = cursor_desc[i].hoty;
    }
}

void mouse_shutdown()
{
}
