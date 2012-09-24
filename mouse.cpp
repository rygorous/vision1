#include "mouse.h"
#include "util.h"
#include "graphics.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int mouse_x, mouse_y, mouse_button;

namespace {
    struct CursorImg {
        U8 img[16][16];
        int hotx, hoty;
    };

    static const struct CursorDesc {
        const char *filename;
        int hotx, hoty;
    } cursor_desc[] = {
#define X(id, filename, hotx, hoty) { filename, hotx, hoty },
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
    int x1 = std::min(16, WIDTH - rel_x);
    int y1 = std::min(16, HEIGHT - rel_y);
    if (x0 >= x1 || y0 >= y1)
        return;

    int offs = rel_y * WIDTH + rel_x;
    for (int y=y0; y < y1; y++) {
        for (int x=x0; x < x1; x++) {
            if (cursor.img[y][x])
                dest[offs + y*WIDTH + x] = pal[cursor.img[y][x]];
        }
    }
}

void init_mouse()
{
    set_mouse_cursor(MC_NORMAL);
    Slice gfx = read_file("grafix/pointers.gra");

    for (int i=0; i < ARRAY_COUNT(cursor_desc); i++) {
        U8 dest[16*WIDTH] = { 0 };

        if (cursor_desc[i].filename) {
            U8 type;
            int offs = find_gra_item(gfx, cursor_desc[i].filename, &type);
            if (offs == -1 || type != 5)
                error_exit("error finding cursor image '%s'\n", cursor_desc[i].filename);

            int decoded = decode_delta(dest, &gfx[offs]);
            assert(decoded <= sizeof(dest));
        }

        CursorImg &cursor = cursors[i];
        for (int y=0; y < 16; y++)
            memcpy(cursor.img[y], &dest[y*WIDTH], 16);
        cursor.hotx = cursor_desc[i].hotx;
        cursor.hoty = cursor_desc[i].hoty;
    }
}

void shutdown_mouse()
{
}
