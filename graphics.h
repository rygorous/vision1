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
};

class ColorCycleAnimation : public Animation {
    int first, last;
    int delay, dir;
    int count;
    int cur_offs, cur_tick;
    Palette orig_a, orig_b;

    int next_offs(int cur) const;
    void render_pal(Palette out, const Palette in, int offs1, int offs2, int t) const;

public:
    ColorCycleAnimation(int first, int last, int delay, int dir);

    virtual void tick();
    virtual void render();
    virtual bool is_done() const;
};

class BigAnimation : public Animation { // .ani / .big files
    U8 *data;
    int frame_size;

    int posx, posy;
    int w, h;
    int last_frame, wait_frames;
    int cur_frame, cur_tick;
    bool reversed;

    const U8 *get_frame(int frame) const;

public:
    BigAnimation(const char *filename, bool reverse_playback);
    virtual ~BigAnimation();

    virtual void tick();
    virtual void render();
    virtual bool is_done() const;
    void rewind();
};

class MegaAnimation : public Animation { // .gra files
    Slice grafile;
    char nameprefix[12];
    int first_frame, last_frame;
    int posx, posy;
    int delay;
    int scale, flip;
    int cur_frame, cur_tick;
    int loops_left;

public:
    MegaAnimation(const char *grafilename, const char *prefix, int first_frame, int last_frame,
        int posx, int posy, int delay, int scale, int flip);
    virtual ~MegaAnimation();

    virtual void tick();
    virtual void render();
    virtual bool is_done() const;
};

class SavedScreen {
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

#endif
