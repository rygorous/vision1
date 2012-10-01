#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include "common.h"

struct PixelBuffer;
class Slice;

struct Rect {
    int x0, y0;
    int x1, y1;
};

class PixelSlice {
    PixelBuffer *buf;   // underlying storage
    U8 *pixels;         // start of data
    int w, h, stride;

    PixelSlice(PixelBuffer *buf, int w, int h);

public:
    explicit PixelSlice();
    PixelSlice(const PixelSlice &x);
    ~PixelSlice();

    static PixelSlice make(int w, int h);
    static PixelSlice black(int w, int h);

    PixelSlice &operator =(const PixelSlice &x);

    const PixelSlice slice(int x0, int y0, int x1, int y1) const;
    PixelSlice slice(int x0, int y0, int x1, int y1) { return (PixelSlice) ((const PixelSlice&) *this).slice(x0, y0, x1, y1); }

    PixelSlice clone();
    PixelSlice reinterpret(int neww, int newh);
    PixelSlice make_resized(int neww, int newh);

    const U8 *row(int y) const          { return pixels + y * stride; }
    U8 *row(int y)                      { return pixels + y * stride; }

    const U8 *ptr(int x, int y) const   { return pixels + y * stride + x; }
    U8 *ptr(int x, int y)               { return pixels + y * stride + x; }

    operator void *() const     { return buf ? buf : nullptr; }
    int width() const           { return w; }
    int height() const          { return h; }
};

void solid_fill(PixelSlice &dest, int color);
void blit(PixelSlice &dest, int dx, int dy, const PixelSlice &src);
void blit_transparent(PixelSlice &dest, int dx, int dy, const PixelSlice &src);
void blit_transparent_shrink(PixelSlice &dest, int dx, int dy, const PixelSlice &src, int shrink, bool flipX);

class Animation { // abstract interface
public:
    virtual ~Animation();

    virtual void tick() = 0;
    virtual void render(PixelSlice &screen) = 0;
    virtual bool is_done() const = 0;
    virtual void rewind() = 0;
};

class SavedScreen { // RAII
    U8 *pals;
    PixelSlice pixels;

public:
    SavedScreen();
    ~SavedScreen();

    void restore();
};

extern PixelSlice vga_screen;
extern Palette vga_pal;

extern Palette palette_a, palette_b;

void graphics_init();
void graphics_shutdown();

PixelSlice load_rle_pixels(const Slice &data, int w, int h);
PixelSlice load_rle_with_header(const Slice &data);
PixelSlice load_hot(const Slice &data);
PixelSlice load_delta_pixels(const Slice &data);

void set_palette();
void set_palb_fade(int intensity);
void load_background(const char *filename, int screen=0); // 0=VGA, 1..4=scroll screen

// big anim flags
enum {
    BA_REVERSE      = 1,
    BA_LOOP         = 2,
    BA_PING_PONG    = 4,
};

Animation *new_color_cycle_anim(int first, int last, int delay, int dir);
Animation *new_big_anim(const char *filename, int flags);
Animation *new_mega_anim(const char *grafilename, const char *prefix, int first_frame,
    int last_frame, int posx, int posy, int delay, int scale, int flip);

#endif
