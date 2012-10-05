#include "corridor.h"
#include "common.h"
#include "util.h"
#include "graphics.h"
#include "vars.h"
#include <assert.h>

enum CorridorBlock {
    CB_FREE,
    CB_WALL,
    CB_DOOR,
};

// ---- level representation

// in maps:
// 00 = accessible
// 07 = skip this column
// 80 = blocked
enum MapBlock {
    MB_FREE     = 0,
    MB_SKIP     = 7,
    MB_SOLID    = 128,
};

static const int NLEVELS = 50;
static const int MAPW = 80;
static const int MAPH = 25; // concentric circles

static U8 map1[MAPH][MAPW];
static U8 map2[MAPH][MAPW];

static int decode_level_rle(U8 *dest, const Slice &src)
{
    int p = 0, q = 0;
    while (q < MAPW*MAPH) {
        int count = src[p++];
        U8 val = src[p++];
        while (count--)
            dest[q++] = val;
    }
    assert(q == MAPW*MAPH);
    return p;
}

static void debug_print_level()
{
    char buf[MAPW+1];
    buf[MAPW] = 0;
    
    // column numbers
    for (int x=1; x <= MAPW; x++)
        buf[x-1] = (x % 10) ? ' ' : '0' + (x/10);
    puts(buf);

    for (int x=1; x <= MAPW; x++)
        buf[x-1] = '0' + (x % 10);
    puts(buf);
    puts("");

    // actual level
    for (int y=0; y < MAPH; y++) {
        for (int x=0; x < MAPW; x++) {
            char ch = '?';
            switch (map1[y][x]) {
            case 0x00:  ch = '.'; break;    // can pass
            case 0x01:  ch = '>'; break;    // door ccw?
            case 0x02:  ch = '<'; break;    // door cw?
            case 0x03:  ch = 'v'; break;    // door out?
            case 0x04:  ch = '^'; break;    // door in?
            case 0x07:  ch = ' '; break;    // warp to next
            case 0x20:  ch = '!'; break;    // passable door?
            case 0x12:  ch = 'a'; break;    // ???
            case 0x13:  ch = 'b'; break;    // ???
            case 0x14:  ch = 'c'; break;    // ???
            case 0x80:  ch = '@'; break;    // obstructed
            }
            if (ch == '?')
                __debugbreak();
            buf[x] = ch;
        }
        puts(buf);
    }
}

static void load_level(int level)
{
    Slice s = read_file("data/levels.dat");
    Slice data = s(little_u16(&s[level*2]) + NLEVELS*2, little_u16(&s[(level+1)*2]) + NLEVELS*2);
    
    int offs = decode_level_rle(map1[0], data);
    decode_level_rle(map2[0], data(offs));

    debug_print_level();
}

// ---- player position and direction

// matches look direction (GANGD)
enum Dir {
    DIR_OUT = 0,    // out = +y
    DIR_IN  = 1,    // in  = -y
    DIR_CW  = 2,    // cw  = -x
    DIR_CCW = 3,    // ccw = +x
};

enum Rot {
    ROT_CCW,
    ROT_CW
};

struct Pos {
    int x, y;
};

static Dir rotate(Dir in, Rot how)
{
    static const Dir rot[2][4] = {
        { DIR_CCW, DIR_CW,  DIR_OUT, DIR_IN },  // ccw
        { DIR_CW,  DIR_CCW, DIR_IN, DIR_OUT }   // cw
    };
    return rot[how][in];
}

static int clamp(int x, int min, int max)
{
    return x < min ? min : x > max ? max : x;
}

static Pos step(const Pos &in, Dir d)
{
    static const int dx[4] = { 0, 0, -1, 1 };
    static const int dy[4] = { 1, -1, 0, 0 };
    Pos p = in;
    p.x = (p.x + dx[d]) % MAPW;
    p.y = clamp(p.y + dy[d], 0, MAPH-1);
}

static Pos advance(const Pos &in, Dir d)
{
    Pos p = in;
    do {
        p = step(p, d);
    } while (map1[p.y][p.x] == MB_SKIP);
    return p;
}

static Pos player_pos()
{
    Pos p;
    p.x = get_var_int("gangx") - 1; // game seems to use 1-based x but 0-based y?
    p.y = get_var_int("gangy");
}

static void set_player_pos(const Pos &p)
{
    set_var_int("gangx", p.x + 1);
    set_var_int("gangy", p.y);
}

// ---- rendering

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
    load_level(get_var_int("etage"));
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

            if (side == CB_FREE) // if turn is free, draw fork
                blit_corridor(s_fork[z], flipx);

            if (front != CB_FREE)
                blit_corridor(s_wall_ahead[z], flipx);

            // handle side
            if (side == CB_FREE) {
                if (map[mapy-1][1+lr] == CB_DOOR) // add door decal to side
                    blit_corridor(s_door_inturn[z], flipx);
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