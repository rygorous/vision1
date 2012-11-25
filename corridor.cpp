#include "corridor.h"
#include "common.h"
#include "util.h"
#include "str.h"
#include "graphics.h"
#include "vars.h"
#include "mouse.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>

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
    puts("-");

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

    puts("-");
    for (int y=0; y < MAPH; y++) {
        for (int x=0; x < MAPW; x++) {
            char ch = '?';
            U8 what = map2[y][x];
            if (what == 0)
                ch = '.';
            else if (what >= 1 && what <= 26)
                ch = 'A' + (what - 1);
            else if (what >= 27 && what <= 52)
                ch = 'a' + (what - 27);
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
    p.x = (p.x + dx[d] + MAPW) % MAPW;
    p.y = clamp(p.y + dy[d], 0, MAPH-1);
    return p;
}

static MapBlock map_at(const Pos &p)
{
    assert(p.x >= 0 && p.x < MAPW);
    if (p.y >= 0 && p.y < MAPH)
        return (MapBlock)map1[p.y][p.x];
    else
        return MB_SOLID;
}

static U8 map2_at(const Pos &p)
{
    assert(p.x >= 0 && p.x < MAPW);
    if (p.y >= 0 && p.y < MAPH)
        return map2[p.y][p.x];
    else
        return 0;
}

static Pos advance(const Pos &in, Dir d)
{
    Pos p = in;
    do {
        p = step(p, d);
    } while (map_at(p) == MB_SKIP);
    return p;
}

static bool is_visible_door(MapBlock b, Dir look_dir)
{
    return (b == MB_DOOR_OUT + look_dir || b == MB_DOOR);
}

// ---- objects

struct ObjectDesc {
    Slice header;
    Slice script;
    Str gfx_name;
    PixelSlice gfx_lr[2], gfx_m;
};
static std::vector<ObjectDesc> s_objtab;

static void load_dsc()
{
    s_objtab.clear();
    Slice dsc = read_file("grafix/corri01.dsc");
    chop_until(dsc, 0); // skip until first 0 byte

    while (dsc.len()) {
        Slice header = chop(dsc, 17);
        Str name = to_string(chop_until(dsc, '\r'));
        Slice script = chop_until(dsc, 0);

        ObjectDesc d;
        d.header = header;
        d.script = script;
        d.gfx_name = name;
        s_objtab.push_back(d);
    }
}

static PixelSlice load_dsc_slice(const Slice &objlib, const Str &name)
{
    U8 type;
    int offs = find_gra_item(objlib, name, &type);
    if (offs >= 0) {
        if (type == 5)
            return load_delta_pixels(objlib(offs));
        else if (type == 8)
            return load_rle_with_header(objlib(offs));
    }

    return PixelSlice();
}

static void load_dsc_gfx(const Slice &objlib)
{
    for (auto it = s_objtab.begin(); it != s_objtab.end(); ++it) {
        const Str &name = it->gfx_name;
        it->gfx_lr[1] = load_dsc_slice(objlib, name + ".r");
        if (name[0] == '_') // underscore prefix = symmetric
            it->gfx_lr[0] = it->gfx_lr[1];
        else
            it->gfx_lr[0] = load_dsc_slice(objlib, name + ".l");
        it->gfx_m = load_dsc_slice(objlib, name + ".m");
    }
}

// ---- rendering

class CorridorGfx {
    static const int DEPTH = 6;

    // building blocks
    PixelSlice wall_side[DEPTH];
    PixelSlice wall_ahead[DEPTH];
    PixelSlice door_side[DEPTH];
    PixelSlice door_ahead[DEPTH];
    PixelSlice door_inturn[DEPTH];
    PixelSlice corner[DEPTH];
    PixelSlice fork[DEPTH];
    PixelSlice cover[DEPTH];
    PixelSlice empty;

    static PixelSlice load(const Slice &lib, const char *basename, int idx)
    {
        U8 type;
        int offs = find_gra_item(lib, Str::fmt("%s%d", basename, idx), &type);
        if (offs < 0 || type != 5)
            panic("bad graphics for corridor!");
        return load_delta_pixels(lib(offs));
    }

    static void blit_chunk(const PixelSlice &what, bool flipx)
    {
        static const int CX = 160, CY = 32;
        blit_transparent_shrink(vga_screen, CX, CY, what, 1, flipx);
    }

public:
    void init(const Str &libfilename)
    {
        Slice lib = read_file(libfilename);
        for (int i=0; i < DEPTH; i++) {
            wall_side[i] = load(lib, "WAND", i);
            wall_ahead[i] = load(lib, "FRONTAL", i);
            door_side[i] = (i >= 1 && i <= 4) ? load(lib, "TUER", i) : wall_side[i];
            door_ahead[i] = load(lib, "FTUER", i);
            door_inturn[i] = (i >= 0 && i <= 4) ? load(lib, "GTUER", i) : PixelSlice();
            corner[i] = load(lib, "ECKE", i);
            fork[i] = load(lib, "GANG", i);
            cover[i] = (i >= 2) ? load(lib, "ABDECK", i) : PixelSlice();
        }
    }

    void render(Pos pos, Dir look_dir)
    {
        Dir rev_look = rotate(look_dir, ROT_U);
        Dir lrdir[2];
        lrdir[0] = rotate(look_dir, ROT_CCW);
        lrdir[1] = rotate(look_dir, ROT_CW);

        // determine draw depth (i.e. distance to next blocking object)
        int zmin = DEPTH-1;
        while (zmin > 0) {
            Pos next = advance(pos, look_dir);
            if (map_at(next) != MB_FREE)
                break;
            pos = next;
            zmin--;
        }

        // unclear:
        // - cover model?
        for (int z=zmin; z < DEPTH; z++) { // depth *increases* towards viewer
            Pos frontpos = advance(pos, look_dir);
            MapBlock front = map_at(frontpos);

            for (int lr=0; lr < 2; lr++) {
                bool flipx = lr == 0;
                MapBlock side = map_at(advance(pos, lrdir[lr]));

                if (side == MB_FREE) // if turn is free, draw fork
                    blit_chunk(fork[z], flipx);

                if (front != MB_FREE)
                    blit_chunk(wall_ahead[z], flipx);

                // handle side
                if (side == MB_FREE) {
                    MapBlock frontside = map_at(advance(frontpos, lrdir[lr]));
                    if (is_visible_door(frontside, look_dir)) // add door decal to side
                        blit_chunk(door_inturn[z], flipx);
                } else {
                    blit_chunk(wall_side[z], flipx);
                    if (is_visible_door(side, lrdir[lr]))
                        blit_chunk(door_side[z], flipx);
                }

                // fix up corners
                if (front != MB_FREE && side != MB_FREE)
                    blit_chunk(corner[z], flipx);

                // draw door on front wall
                if (is_visible_door(front, look_dir))
                    blit_chunk(door_ahead[z], flipx);
            }

            // objects
            int revz = DEPTH-1 - z;
            U8 item = map2_at(frontpos);
            if (item && revz >= 0 && revz <= 2) {
                const ObjectDesc &obj = s_objtab[item - 1];

                print_hex(obj.gfx_name.c_str(), obj.header);
                PixelSlice clipscreen = vga_screen.slice(0, 32, 320, 144);
                blit_transparent_shrink(clipscreen, 0, 0, obj.gfx_m, 1 << revz, false);
            }

            pos = advance(pos, rev_look);
        }
    }
};

// ---- corridor main

static CorridorGfx *s_gfx;

void corridor_init()
{
    load_dsc();

    s_gfx = new CorridorGfx;
    s_gfx->init("grafix/wand01.gra");
}

void corridor_shutdown()
{
    delete s_gfx;
}

void corridor_start()
{
    int level = get_var_int("etage");

    solid_fill(vga_screen, 0);
    load_level(level);

    const char *libname = "grafix/gmod02.gra";
    if (level >= 10 && level <= 42)
        libname = "grafix/gmod01.gra";

    Slice objlib = read_file(libname);
    load_dsc_gfx(objlib);

    // determine which palette to load
    int pal = map2[0][21]; // magic index from the game.
    int hour = get_var_int("st");
    if (hour >= 1 && hour <= 5)
        pal = 98;
    
    load_palette(Str::fmt("grafix/palette.%d", pal));
    set_palette();
}

void corridor_render()
{
    s_gfx->render(player_pos(), player_dir());
}

void corridor_click(int code)
{
    Dir dir = player_dir();

    switch (code) {
    case MC_TURNL:      set_player_dir(rotate(dir, ROT_CCW)); break;
    case MC_TURNR:      set_player_dir(rotate(dir, ROT_CW)); break;
    case MC_TURNU:      set_player_dir(rotate(dir, ROT_U)); break;
    case MC_FORWARD:
        {
            Pos newpos = advance(player_pos(), dir);
            if (map_at(newpos) == MB_FREE)
                set_player_pos(newpos);
        }
        break;
    }
}
