#include "corridor.h"
#include "common.h"
#include "util.h"
#include "graphics.h"
#include <assert.h>

enum CorridorBlock
{
    CB_FREE,
    CB_WALL,
    CB_DOOR,
};

static const int DEPTH = 6;

static PixelSlice s_hotspots;

static PixelSlice s_wall_side[DEPTH];
static PixelSlice s_wall_ahead[DEPTH];
static PixelSlice s_door_side[DEPTH];
static PixelSlice s_door_ahead[DEPTH];
static PixelSlice s_door_inturn[DEPTH];
static PixelSlice s_corner[DEPTH];
static PixelSlice s_fork[DEPTH];
static PixelSlice s_cover[DEPTH];
static PixelSlice s_empty;

static PixelSlice gfx_load(const Slice &s, const char *basename, int idx)
{
    char name[16];
    U8 type;
    sprintf(name, "%s%d", basename, idx);

    int offs = find_gra_item(s, name, &type);
    if (offs < 0 || type != 5)
        panic("bad graphics for corridor!");

    return load_delta_pixels(s(offs));
}

void corridor_init()
{
    s_hotspots = load_hot(read_file("grafix/corri.hot"));
    Slice lib = read_file("grafix/wand01.gra");

    for (int i=0; i < DEPTH; i++) {
        s_wall_side[i] = gfx_load(lib, "WAND", i);
        s_wall_ahead[i] = gfx_load(lib, "FRONTAL", i);
        s_door_side[i] = (i >= 1 && i <= 4) ? gfx_load(lib, "TUER", i) : s_wall_side[i];
        s_door_ahead[i] = gfx_load(lib, "FTUER", i);
        s_door_inturn[i] = (i >= 0 && i <= 4) ? gfx_load(lib, "GTUER", i) : PixelSlice();
        s_corner[i] = gfx_load(lib, "ECKE", i);
        s_fork[i] = gfx_load(lib, "GANG", i);
        s_cover[i] = (i >= 2) ? gfx_load(lib, "ABDECK", i) : PixelSlice();
    }
}

void corridor_shutdown()
{
    s_hotspots = PixelSlice();
    for (int i=0; i < DEPTH; i++) {
        s_wall_side[i] = PixelSlice();
        s_wall_ahead[i] = PixelSlice();
        s_door_side[i] = PixelSlice();
        s_door_ahead[i] = PixelSlice();
        s_corner[i] = PixelSlice();
        s_fork[i] = PixelSlice();
        s_cover[i] = PixelSlice();
    }
}

void corridor_start()
{
    Slice pal = read_file("grafix/corri01.pal");
    memcpy(palette_a, &pal[0], sizeof(Palette));
    set_palette();

    solid_fill(vga_screen, 0);
}

static void blit_corridor(const PixelSlice &what, bool flipx)
{
    static const int CX = 160, CY = 32;
    blit_transparent_shrink(vga_screen, CX, CY, what, 1, flipx);
}

void corridor_render()
{
    static const U8 map[7][3] = {
        { CB_WALL, CB_WALL, CB_WALL },
        { CB_WALL, CB_FREE, CB_WALL },
        { CB_WALL, CB_FREE, CB_WALL },
        { CB_WALL, CB_FREE, CB_DOOR },
        { CB_DOOR, CB_FREE, CB_FREE },
        { CB_FREE, CB_FREE, CB_WALL },
        { CB_WALL, CB_FREE, CB_WALL },
    };

    // unsolved:
    // - cover model?
    // - door also depends on which side faces player, how does game encode this?

    for (int z=0; z<6; z++) { // depth *increases* towards viewer
        int mapy = z + 1;

        for (int lr=-1; lr <= 1; lr += 2) {
            bool flipx = lr < 0;

            int front = map[mapy-1][1];
            int side = map[mapy][1+lr];

            if (side == CB_FREE && front != CB_FREE) {
                // turn to the side: draw wall over fork
                blit_corridor(s_fork[z], flipx);
                blit_corridor(s_wall_ahead[z], flipx);
            } else if (front != CB_FREE)
                blit_corridor(s_wall_ahead[z], flipx);

            // handle side
            if (side == CB_FREE) {
                if (front == CB_FREE) // both free: use unmodified fork
                    blit_corridor(s_fork[z], flipx);
                else {
                    if (map[mapy-1][1+lr] == CB_DOOR) // add door decal to side
                        blit_corridor(s_door_inturn[z], flipx);
                }
            } else {
                blit_corridor(s_wall_side[z], flipx);
                if (side == CB_DOOR)
                    blit_corridor(s_door_side[z], flipx);
            }

            // fix up corners
            if (front != CB_FREE && side != CB_FREE)
                blit_corridor(s_corner[z], flipx);

            // draw door on front wall
            if (front == CB_DOOR)
                blit_corridor(s_door_ahead[z], flipx);
        }
    }
}