#include "corridor.h"
#include "common.h"
#include "util.h"
#include "graphics.h"

static PixelSlice s_hotspots;
static PixelSlice s_wall[6];

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

    for (int i=0; i < ARRAY_COUNT(s_wall); i++)
        s_wall[i] = gfx_load(lib, "WAND", i);
}

void corridor_shutdown()
{
    s_hotspots = PixelSlice();
    for (int i=0; i < ARRAY_COUNT(s_wall); i++)
        s_wall[i] = PixelSlice();
}

void corridor_start()
{
    Slice pal = read_file("grafix/corri01.pal");
    memcpy(palette_a, &pal[0], sizeof(Palette));
    set_palette();
}

void corridor_render()
{
    // fill background black
    solid_fill(vga_screen, 0);

    for (int i=0; i < 6; i++) {
        blit_transparent_shrink(vga_screen, 160, 32, s_wall[i], 1, false);
        blit_transparent_shrink(vga_screen, 160, 32, s_wall[i], 1, true);
    }
}