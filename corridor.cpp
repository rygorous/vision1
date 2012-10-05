#include "corridor.h"
#include "common.h"
#include "util.h"
#include "graphics.h"
#include "vars.h"
#include "mouse.h"
#include <assert.h>

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
    ROT_CW,
    ROT_U,
};

struct Pos {
    int x, y;
};

static Dir rotate(Dir in, Rot how)
{
    static const Dir rot[3][4] = {
        { DIR_CCW, DIR_CW,  DIR_OUT, DIR_IN  }, // ccw
        { DIR_CW,  DIR_CCW, DIR_IN,  DIR_OUT }, // cw
        { DIR_IN,  DIR_OUT, DIR_CCW, DIR_CW  }, // u-turn
    };
    return rot[how][in];
}

static int clamp(int x, int min, int max)
{
    return x < min ? min : x > max ? max : x;
}

static Pos player_pos()
{
    Pos p;
    p.x = get_var_int("gangx") - 1; // game seems to use 1-based x but 0-based y?
    p.y = get_var_int("gangy");
    return p;
}

static void set_player_pos(const Pos &p)
{
    set_var_int("gangx", p.x + 1);
    set_var_int("gangy", p.y);
}

static Dir player_dir()
{
    return (Dir)get_var_int("gangd");
}

static void set_player_dir(Dir which)
{
    set_var_int("gangd", which);
}

// ---- level representation

// in maps:
// 00 = accessible
// 07 = skip this column
// 80 = blocked
enum MapBlock {
    MB_FREE     = 0,
    MB_DOOR_OUT = 1,
    MB_DOOR_IN  = 2,
    MB_DOOR_CW  = 3,
    MB_DOOR_CCW = 4,
    MB_SKIP     = 7,
    MB_DOOR     = 32,
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

static Pos step(const Pos &in, Dir d)
{
    static const int dx[4] = { 0, 0, -1, 1 };
    static const int dy[4] = { 1, -1, 0, 0 };
    Pos p = in;
    p.x = (p.x + dx[d]) % MAPW;
    p.y = clamp(p.y + dy[d], 0, MAPH-1);
    return p;
}

static Pos advance(const Pos &in, Dir d)
{
    Pos p = in;
    do {
        p = step(p, d);
    } while (map1[p.y][p.x] == MB_SKIP);
    return p;
}

static MapBlock map_at(const Pos &p)
{
    return (MapBlock)map1[p.y][p.x];
}

static bool is_visible_door(MapBlock b, Dir look_dir)
{
    return (b == MB_DOOR_OUT + look_dir || b == MB_DOOR);
}

// ---- rendering

static const int DEPTH = 6;

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
    load_background("grafix/corri01.pal");
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
    Pos pos = player_pos();
    Dir look_dir = (Dir)get_var_int("gangd");
    Dir rev_look = rotate(look_dir, ROT_U);
    Dir lrdir[2];
    lrdir[0] = rotate(look_dir, ROT_CCW);
    lrdir[1] = rotate(look_dir, ROT_CW);

    // determine draw depth (i.e. distance to next blocking object)
    int zmin = DEPTH-1;
    while (zmin > 0) {
        Pos next = advance(pos, look_dir);
        if (map1[next.y][next.x] != MB_FREE)
            break;
        pos = next;
        zmin--;
    }

    // unclear:
    // - cover model?
    // - door also depends on which side faces player, how does game encode this?
    for (int z=zmin; z < DEPTH; z++) { // depth *increases* towards viewer
        Pos frontpos = advance(pos, look_dir);
        MapBlock front = map_at(frontpos);

        for (int lr=0; lr < 2; lr++) {
            bool flipx = lr == 0;
            MapBlock side = map_at(advance(pos, lrdir[lr]));

            if (side == MB_FREE) // if turn is free, draw fork
                blit_corridor(s_fork[z], flipx);

            if (front != MB_FREE)
                blit_corridor(s_wall_ahead[z], flipx);

            // handle side
            if (side == MB_FREE) {
                MapBlock frontside = map_at(advance(frontpos, lrdir[lr]));
                if (is_visible_door(frontside, look_dir)) // add door decal to side
                    blit_corridor(s_door_inturn[z], flipx);
            } else {
                blit_corridor(s_wall_side[z], flipx);
                if (is_visible_door(side, lrdir[lr]))
                    blit_corridor(s_door_side[z], flipx);
            }

            // fix up corners
            if (front != MB_FREE && side != MB_FREE)
                blit_corridor(s_corner[z], flipx);

            // draw door on front wall
            if (is_visible_door(front, look_dir))
                blit_corridor(s_door_ahead[z], flipx);
        }

        pos = advance(pos, rev_look);
    }
}

void corridor_click(int code)
{
    Dir dir = player_dir();

    switch (code) {
    case MC_FORWARD:    set_player_pos(advance(player_pos(), dir)); break;
    case MC_TURNL:      set_player_dir(rotate(dir, ROT_CCW)); break;
    case MC_TURNR:      set_player_dir(rotate(dir, ROT_CW)); break;
    case MC_TURNU:      set_player_dir(rotate(dir, ROT_U)); break;
    }
}
