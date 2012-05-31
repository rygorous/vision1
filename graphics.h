#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include "common.h"

struct GfxBlock {
    GfxBlock();
    ~GfxBlock();

    void load(const char *filename);
    void resize(int w, int h);

    U8 *pixels;
    int w, h;
};

class Animation { // abstract interface
public:
    virtual ~Animation();

    virtual void tick() = 0;
    virtual void render() = 0;
    virtual bool is_done() const = 0;
    virtual void rewind() = 0;
};

class SavedScreen { // RAII
    U8 *data;

public:
    SavedScreen();
    ~SavedScreen();

    void restore();
};

extern Palette palette_a, palette_b;

void set_palette();
void set_palb_fade(int intensity);
void load_background(const char *filename);

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
