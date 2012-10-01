#ifndef __FONT_H__
#define __FONT_H__

class PixelSlice;

class Font { // abstract interface
public:
    virtual ~Font();

    virtual void print(PixelSlice &target, int x, int y, const char *str, int len) const = 0;
    virtual int glyph_width(char ch) const = 0;
    virtual int str_width(const char *str, int len) const = 0;

    void print(PixelSlice &target, int x, int y, const char *str) const;
    int str_width(const char *str) const;
};

extern Font *bigfont, *bigfont_highlight;

void init_font();
void shutdown_font();

#endif
