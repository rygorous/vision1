#ifndef __FONT_H__
#define __FONT_H__

struct Font {
    Font();

    void load(const char *filename, const U8 *widths, const U8 *palette);
    void print(int x, int y, const char *str, int len);
    void print(int x, int y, const char *str);
    int glyph_width(U8 ch) const;
    int str_width(const char *str, int len) const;

private:
    void print_glyph(int x, int y, int glyph);
    static int glyph_index(U8 ch);

    GfxBlock gfx;
    const U8 *widths;
    U8 pal[16];
};

extern Font bigfont;

void init_font();

#endif
