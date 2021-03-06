#include "font.h"
#include "graphics.h"
#include "util.h"
#include "str.h"
#include <algorithm>
#include <string.h>
#include <assert.h>

// ---- data from the game executable

static const U8 widths_small[160] = {
//   0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8, // 0x00
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8, // 0x10
     4,  3,  5,  7,  7,  8,  8,  3,  5,  5,  7,  5,  4,  5,  3,  6, // 0x20
     5,  4,  6,  6,  6,  5,  5,  6,  5,  5,  3,  4,  5,  5,  5,  6, // 0x30
    13,  7,  6,  6,  6,  5,  5,  6,  6,  3,  6,  7,  5,  7,  7,  6, // 0x40
     6,  6,  6,  6,  7,  6,  7,  9,  7,  7,  6,  5,  6,  5,  8,  7, // 0x50
     3,  7,  6,  6,  6,  6,  5,  6,  6,  3,  5,  6,  4,  7,  6,  6, // 0x60
     6,  6,  6,  6,  5,  6,  7,  9,  7,  7,  7,  5,  3,  5,  7,  4, // 0x70
     5,  6,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7, // 0x80
     7,  7,  7,  7,  6,  7,  7,  7,  7,  6,  6,  6,  6,  6,  6,  6, // 0x90
};

static const U8 widths_big[160] = {
//   0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8, // 0x00
     8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8, // 0x10
     6,  3,  5,  7,  7,  9,  8,  3,  5,  5,  7,  7,  4,  7,  3,  6, // 0x20
     6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  3,  4,  5,  7,  5,  7, // 0x30
    13,  9,  8,  8,  9,  8,  8,  9,  9,  5,  8,  9,  8, 12,  9,  9, // 0x40
     8,  9,  9,  8,  9, 10, 10, 13,  9,  9,  8,  5,  6,  5,  8,  7, // 0x50
     3,  8,  8,  7,  8,  7,  5,  7,  9,  5,  6,  9,  5, 13,  9,  7, // 0x60
     8,  8,  8,  7,  6,  9,  9, 13,  8,  9,  7,  5,  3,  5,  7,  6, // 0x70
     5,  9,  7,  7,  8,  7,  7,  7,  7,  7,  7,  7,  7,  7,  9,  7, // 0x80
     7,  7,  7,  7,  7,  7,  7,  7,  7,  9, 10,  7,  7,  7,  7,  7, // 0x90
};

static const U16 glyph_offsets[128] = {
    0x3330, 0x0d00, 0x0d10, 0x3290, 0x0d20, 0x0d30, 0x2670, 0x0da0,
    0x0d50, 0x0d60, 0x2670, 0x0d80, 0x1900, 0x0d90, 0x0db0, 0x0d40,
    0x2680, 0x2690, 0x26a0, 0x26b0, 0x3200, 0x3210, 0x3220, 0x3230,
    0x3240, 0x3250, 0x1920, 0x1930, 0x0d50, 0x0d70, 0x0d60, 0x1910,
    0x3280, 0x0000, 0x0010, 0x0020, 0x0030, 0x0040, 0x0050, 0x0060,
    0x0070, 0x0080, 0x0090, 0x00a0, 0x00b0, 0x00c0, 0x00d0, 0x00e0,
    0x00f0, 0x3260, 0x0100, 0x0110, 0x0120, 0x0130, 0x0c80, 0x0c90,
    0x0ca0, 0x0cb0, 0x0cc0, 0x0d50, 0x0d40, 0x0d60, 0x32a0, 0x0d90,
    0x0da0, 0x1950, 0x1960, 0x1970, 0x1980, 0x1990, 0x19a0, 0x19b0,
    0x19c0, 0x19d0, 0x19e0, 0x19f0, 0x1a00, 0x1a10, 0x1a20, 0x1a30,
    0x2580, 0x2590, 0x25a0, 0x25b0, 0x25c0, 0x25d0, 0x25e0, 0x25f0,
    0x2600, 0x2610, 0x2620, 0x0d50, 0x3270, 0x0d60, 0x3330, 0x3330,
    0x3330, 0x2650, 0x2670, 0x2670, 0x2630, 0x2670, 0x2670, 0x2670,
    0x2670, 0x2670, 0x2670, 0x2670, 0x2670, 0x2670, 0x0cd0, 0x2670,
    0x2670, 0x2670, 0x2670, 0x2670, 0x2640, 0x2670, 0x2670, 0x2670,
    0x2670, 0x0ce0, 0x0cf0, 0x2660, 0x2670, 0x2670, 0x2670, 0x2670,
};

// ---- font code

Font::~Font()
{
}

void Font::print(PixelSlice &target, int x, int y, const char *str) const
{
    print(target, x, y, str, strlen(str));
}

int Font::str_width(const char *str) const
{
    return str_width(str, strlen(str));
}

int Font::str_width(const Str &str) const
{
    return str_width(&str[0], str.size());
}

class BitmapFont : public Font {
public:
    BitmapFont(const char *filename, const U8 *widths, const U8 *palette);

    virtual void print(PixelSlice &target, int x, int y, const char *str, int len) const;
    virtual int glyph_width(char ch) const;
    virtual int str_width(const char *str, int len) const;

private:
    void print_glyph(PixelSlice &target, int x, int y, int glyph) const;
    static int glyph_index(U8 ch);

    PixelSlice gfx;
    const U8 *widths;
    U8 pal[16];
};


BitmapFont::BitmapFont(const char *filename, const U8 *widths, const U8 *palette)
    : widths(widths)
{
    gfx = load_rle_with_header(read_file(filename));
    gfx = gfx.make_resized(320, gfx.height());
    memcpy(pal, palette, sizeof(pal));
}

void BitmapFont::print(PixelSlice &target, int x, int y, const char *str, int len) const
{
    for (int i=0; i < len; i++) {
        int glyph = glyph_index(str[i]);
        if (glyph >= 0x20)
            print_glyph(target, x, y, glyph - 0x20);
        x += widths[glyph]-1;
    }
}

int BitmapFont::glyph_width(char ch) const
{
    int glyph = glyph_index(ch);
    return widths[glyph]-1;
}

int BitmapFont::str_width(const char *str, int len) const
{
    int w = 0;
    for (int i=0; i < len; i++)
        w += glyph_width(str[i]);
    return w;
}

void BitmapFont::print_glyph(PixelSlice &target, int posx, int posy, int glyph) const
{
    // clip (in glyph space)
    int x0 = std::max(0, 0 - posx);
    int y0 = std::max(0, 0 - posy);
    int x1 = std::min(16, target.width() - posx);
    int y1 = std::min(10, target.height() - posy);

    const U8 *srcp = gfx.row(y0) + glyph_offsets[glyph];

    for (int y=y0; y < y1; y++) {
        U8 *dst = target.ptr(posx, posy + y);

        for (int x=x0; x < x1; x++) {
            U8 col = srcp[x];
            if (col) {
                assert(col >= 0xf0);
                dst[x] = pal[col - 0xf0];
            }
        }

        srcp += gfx.width();
    }
}

int BitmapFont::glyph_index(U8 ch)
{
    if (ch == 0xe1)
        return 0x9b;
    else if (ch >= 0xa0) // not in the original game!
        return 0;
    else
        return ch;
}

Font *bigfont, *bigfont_highlight;

static const U8 fontpal_big_default[16] = {
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const U8 fontpal_big_yellow[16] = {
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xfc, 0xfd, 0xfe, 0xff, 0xf8, 0xf9, 0xfa, 0xfb
};

static const U8 fontpal_small_default[16] = {
    0xf0, 0xf1, 0xf2, 0xf3, 0x30, 0x2a, 0x25, 0x16,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

void font_init()
{
    bigfont = new BitmapFont("grafix/zsatz.blk", widths_big, fontpal_big_default);
    bigfont_highlight = new BitmapFont("grafix/zsatz.blk", widths_big, fontpal_big_yellow);
}

void font_shutdown()
{
    delete bigfont;
    delete bigfont_highlight;
}
