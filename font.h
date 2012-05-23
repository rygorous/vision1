#ifndef __FONT_H__
#define __FONT_H__

class Font { // abstract interface
public:
    virtual ~Font();

    virtual void print(int x, int y, const char *str, int len) const = 0;
    virtual int glyph_width(char ch) const = 0;
    virtual int str_width(const char *str, int len) const = 0;

    void print(int x, int y, const char *str) const;
    int str_width(const char *str) const;
};

extern Font *bigfont;

void init_font();
void shutdown_font();

#endif
