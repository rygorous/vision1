#include "corridor.h"
#include "common.h"
#include "util.h"
#include "str.h"
#include "graphics.h"
#include "vars.h"
#include "mouse.h"
#include "script.h"
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
    DIR_CCW = 3     // ccw = +x
};

enum Rot {
    ROT_CCW,
    ROT_CW,
    ROT_U 
};

enum Sector {
    SEC_RED,
    SEC_GREEN,
    SEC_BLUE,
    SEC_YELLOW
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

enum Map2Block {
	M2B_EMPTY	= 0,
	M2B_LIFT	= 2,
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
    p.y = (p.y + dy[d] + MAPH) % MAPH;
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
	if (p.y == 0) // game does this only when looking in
		return M2B_LIFT;
    else if (p.y >= 1 && p.y < MAPH)
        return map2[p.y][p.x];
    else
        return M2B_EMPTY;
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
    return b >= MB_DOOR_OUT && b <= MB_DOOR_CCW || b == MB_DOOR;
    //return (b == MB_DOOR_OUT + look_dir || b == MB_DOOR);
}

static Sector sector_from_pos(const Pos &p)
{
    static const Sector quadrant2sector[4] = { SEC_RED, SEC_GREEN, SEC_BLUE, SEC_YELLOW };
    static const int bias = (MAPW - 1) / 8;
    return quadrant2sector[((p.x + bias) % MAPW) / (MAPW / 4)];
}

// ---- objects

struct ObjectDesc {
    Slice script;
    Str gfx_name;
    PixelSlice gfx_lr[2], gfx_m;
    U8 x, y, flipX;
	char cursor;
};
static std::vector<ObjectDesc> s_objtab;

static void load_dsc()
{
    s_objtab.clear();
    Slice dsc = read_file("grafix/corri01.dsc");
    chop_until(dsc, 0); // skip until first 0 byte

    while (dsc.len()) {
        // header:
        //   void *img[3];
        //   U8 x, y;
        //   U8 unk;
        //   U8 cursorType, imgType;
        Slice header = chop(dsc, 17);
        Str name = to_string(chop_until(dsc, '\r'));
        Slice script = chop_until(dsc, 0);

        //print_hex(Str::fmt("%2d %8s: ", s_objtab.size(), name), header, 17);

        ObjectDesc d;
        d.script = script;
        d.gfx_name = name;
        d.x = header[12];
        d.y = header[13];
        d.flipX = header[14];
        d.cursor = header[15];
        // imgtype = header[16]
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

    static void blit_chunk(const PixelSlice &what, bool flipx, Sector sec)
    {
        static const int CX = 160, CY = 32;
        static const int nremap = 2;
        static const U8 remap_src[nremap] = { 0x02, 0x7f };
        static const U8 remap_dst[4][nremap] = {
            { 0x4f, 0x4a, },
            { 0x2f, 0x2c },
            { 0x34, 0x37 },
            { 0x5e, 0x58 },
        };

        blit_transparent_shrink(vga_screen, CX, CY, what.replace_colors(remap_src, remap_dst[sec], nremap), 1, flipx);
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
        PixelSlice clipscreen = vga_screen.slice(0, 32, 320, 144);
		PixelSlice &hotspots = game_get_hotspots();

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
            int revz = DEPTH-1 - z;
            Pos frontpos = advance(pos, look_dir);
            Sector frontsec = sector_from_pos(frontpos);
            MapBlock front = map_at(frontpos);

            for (int lr=0; lr < 2; lr++) {
                bool flipx = lr == 0;
                Pos sidepos = advance(pos, lrdir[lr]);
                Sector sidesec = sector_from_pos(sidepos);
                MapBlock side = map_at(sidepos);
                Pos frontsidepos = advance(frontpos, lrdir[lr]);
                MapBlock frontside = map_at(frontsidepos);

                if (side == MB_FREE) // if turn is free, draw fork
                    blit_chunk(fork[z], flipx, frontsec);

                if (front != MB_FREE)
                    blit_chunk(wall_ahead[z], flipx, frontsec);

                // handle side
                if (side == MB_FREE) {
                    if (is_visible_door(frontside, look_dir)) // add door decal to side
                        blit_chunk(door_inturn[z], flipx, sector_from_pos(frontsidepos));
                } else {
                    blit_chunk(wall_side[z], flipx, sidesec);
                    if (is_visible_door(side, lrdir[lr]))
                        blit_chunk(door_side[z], flipx, sidesec);
                }

                // fix up corners
                if (front != MB_FREE && side != MB_FREE)
                    blit_chunk(corner[z], flipx, frontsec);

                // draw door on front wall
                if (is_visible_door(front, look_dir))
                    blit_chunk(door_ahead[z], flipx, frontsec);
                
                // objects on the side
                if (front == MB_FREE && frontside != MB_FREE) {
                    U8 objtype = map2_at(frontsidepos);
                    if (objtype && revz <= 2) {
                        static const int ytab[3] = { 0, 30, 45 };
                        const ObjectDesc &obj = s_objtab[objtype - 1];

						const PixelSlice &gfx = obj.gfx_lr[lr ^ obj.flipX];
						int x = 160 - flipx;
						int y = ytab[revz];

                        blit_transparent_shrink(clipscreen, x, y, gfx, 1 << revz, flipx);
						if (revz == 0) {
							int hotidx = flipx ? 7 : 5;
							blit_to_mask(hotspots, hotidx, x, y, gfx, flipx);
							game_hotspot_define(hotidx, obj.cursor);
						}
                    }
                }
            }

            // front objects
            U8 objtype = map2_at(frontpos);
            if (objtype && revz <= 2 && z == zmin) {
                static const int ytab[2][3] = {
                    { 0, 26, 43 },
                    { 0, 31, 45 }
                };
                const ObjectDesc &obj = s_objtab[objtype - 1];

                int x = obj.x >> revz;
                int y = ytab[obj.y != 0][revz] + (obj.y >> revz);
                x = obj.flipX ? 159 + x : 160 - x;

                blit_transparent_shrink(clipscreen, x, y, obj.gfx_m, 1 << revz, obj.flipX != 0);
				if (revz == 0) {
					blit_to_mask(hotspots, 6, x, y, obj.gfx_m, obj.flipX != 0);
					game_hotspot_define(6, obj.cursor);
				}
            }

            pos = advance(pos, rev_look);
        }
    }
};

// ---- corridor main

static CorridorGfx *s_gfx;
static Slice s_hotbg;

void corridor_init()
{
    load_dsc();

    s_gfx = new CorridorGfx;
    s_gfx->init("grafix/wand01.gra");
	s_hotbg = read_file("grafix/corrihot.dat");
}

void corridor_shutdown()
{
    delete s_gfx;
	s_hotbg = Slice();
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
	PixelSlice &hotspots = game_get_hotspots();
	assert(hotspots.width() * hotspots.height() == s_hotbg.len());
	memcpy(hotspots.row(0), &s_hotbg[0], s_hotbg.len());

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
