#include "mouse.h"
#include "util.h"
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
}

static CursorImg cr_normal;
static CursorImg cr_turnl, cr_turnr, cr_turnu;
static CursorImg cr_question;
static CursorImg cr_grab;
static CursorImg cr_talk;
static CursorImg cr_forward;

void render_mouse_cursor(U32 *dest, const U32 *pal)
{
    // clip (in cursor space)
    const CursorImg &cursor = cr_normal;
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

static void load_cursor(CursorImg &cursor, Slice grafile, const char *name)
{
    U8 type;
    int offs = find_gra_item(grafile, name, &type);
    if (offs == -1 || type != 5)
        error_exit("error finding cursor image '%s'\n", name);

    U8 dest[16*WIDTH] = { 0 };
    int decoded = decode_delta(dest, &grafile[offs]);
    assert(decoded <= sizeof(dest));
    for (int y=0; y < 16; y++)
        memcpy(cursor.img[y], &dest[y*WIDTH], 16);
    cursor.hotx = cursor.hoty = 2;
}

void init_mouse()
{
    printf("pointers.gra:\n");
    Slice gfx = read_file("grafix/pointers.gra");

    load_cursor(cr_normal, gfx, "normal");
    load_cursor(cr_turnl, gfx, "drehlink");
    load_cursor(cr_turnr, gfx, "drehrech");
    load_cursor(cr_turnu, gfx, "umdrehen");
    load_cursor(cr_question, gfx, "fragezei");
    load_cursor(cr_grab, gfx, "grabsch");
    load_cursor(cr_talk, gfx, "laber");
    load_cursor(cr_forward, gfx, "vorw\x8erts");
}

void shutdown_mouse()
{
}
