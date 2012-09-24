#include "graphics.h"
#include "util.h"
#include "script.h"
#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Palette palette_a, palette_b;

// ---- pixel slices

struct PixelBuffer
{
    U8 *pixels;
    U32 nrefs;

    PixelBuffer(int w, int h)
    {
        assert(w >= 0 && h >= 0);
        pixels = new U8[w*h];
        nrefs = 0;
        
        if (!pixels)
            error_exit("out of memory");
    }

    ~PixelBuffer()
    {
        delete[] pixels;
    }

    static void ref(PixelBuffer *x)     { if (x) x->nrefs++; }
    static void unref(PixelBuffer *x)   { if (x && --x->nrefs == 0) delete x; }
};

PixelSlice::PixelSlice()
    : buf(0), pixels(0), w(0), h(0), stride(0)
{
}

PixelSlice::PixelSlice(PixelBuffer *buf, int w, int h)
    : buf(buf), pixels(buf->pixels), w(w), h(h), stride(w)
{
    PixelBuffer::ref(buf);
}

PixelSlice::PixelSlice(const PixelSlice &x)
    : buf(x.buf), pixels(x.pixels), w(x.w), h(x.h), stride(x.stride)
{
    PixelBuffer::ref(buf);
}

PixelSlice::~PixelSlice()
{
    PixelBuffer::unref(buf);
}

PixelSlice PixelSlice::make(int w, int h)
{
    return PixelSlice(new PixelBuffer(w, h), w, h);
}

PixelSlice PixelSlice::black(int w, int h)
{
    PixelSlice p = make(w, h);
    memset(p.row(0), 0, w*h);
    return p;
}

PixelSlice &PixelSlice::operator =(const PixelSlice &x)
{
    PixelBuffer::ref(x.buf);
    PixelBuffer::unref(buf);
    buf = x.buf;
    pixels = x.pixels;
    w = x.w;
    h = x.h;
    stride = x.stride;
    return *this;
}

const PixelSlice PixelSlice::slice(int x0, int y0, int x1, int y1) const
{
    if (x1 < x0 || y1 < y0)
        return PixelSlice();

    x0 = std::min(x0, w);
    x1 = std::min(x1, w);
    y0 = std::min(y0, h);
    y1 = std::min(y1, h);

    PixelSlice s(*this);
    s.pixels += y0 * s.stride + x0;
    s.w = x1 - x0;
    s.h = y1 - y0;
    return s;
}

PixelSlice PixelSlice::make_resized(int neww, int newh)
{
    PixelSlice s = black(neww, newh);
    blit(s, 0, 0, *this);
    return s;
}

static bool clipblit(Rect *sr, int dx, int dy, const PixelSlice &dest, const PixelSlice &src)
{
    // in source rect space
    sr->x0 = std::max(-dx, 0);
    sr->y0 = std::max(-dy, 0);
    sr->x1 = std::min(src.width(), dest.width() - dx);
    sr->y1 = std::min(src.height(), dest.height() - dy);
    return sr->x0 < sr->x1 && sr->y0 < sr->y1;
}

void blit(PixelSlice &dest, int dx, int dy, const PixelSlice &src)
{
    Rect sr;
    if (!clipblit(&sr, dx, dy, dest, src))
        return;

    int w = sr.x1 - sr.x0;
    for (int sy=sr.y0; sy < sr.y1; sy++)
        memcpy(dest.ptr(dx+sr.x0, dy+sy), src.ptr(sr.x0, sy), w);
}

void blit_transparent(PixelSlice &dest, int dx, int dy, const PixelSlice &src)
{
    Rect sr;
    if (!clipblit(&sr, dx, dy, dest, src))
        return;

    int w = sr.x1 - sr.x0;
    for (int sy=sr.y0; sy < sr.y1; sy++) {
        U8 *d = dest.ptr(dx+sr.x0, dy+sy);
        const U8 *s = src.ptr(sr.x0, sy);

        for (int x=0; x < w; x++) {
            if (s[x])
                d[x] = s[x];
        }
    }
}

// ---- functions

PixelSlice load_rle_pixels(const char *filename)
{
    Slice s = read_file(filename);
    PixelSlice p = PixelSlice::make(little_u16(&s[0]), little_u16(&s[2]));
    decode_rle(p.row(0), &s[4]);
    return p;
}

void fix_palette(Palette pal)
{
    static const PalEntry defaultHighPal[8] = {
        {  0,  0,  0 },
        { 20, 20, 20 },
        { 40, 40, 40 },
        { 60, 60, 60 },
        {  0,  0,  0 },
        { 28, 24,  4 },
        { 42, 38,  6 },
        { 63, 56,  9 },
    };

    pal[0].r = pal[0].g = pal[0].b = 0;
    memcpy(&pal[0xf8], defaultHighPal, 8 * sizeof(PalEntry));
}

void set_palette()
{
    memcpy(vga_pal, palette_a, sizeof(Palette));
}

void set_palb_fade(int intensity)
{
    //for (int i=0; i < 256; i++) {
    //    vga_pal[i].r = (palette_b[i].r * intensity) >> 8;
    //    vga_pal[i].g = (palette_b[i].g * intensity) >> 8;
    //    vga_pal[i].b = (palette_b[i].b * intensity) >> 8;
    //}

    U8 max = (intensity * 63) >> 8;
    for (int i=0; i < 256; i++) {
        vga_pal[i].r = std::min(palette_b[i].r, max);
        vga_pal[i].g = std::min(palette_b[i].g, max);
        vga_pal[i].b = std::min(palette_b[i].b, max);
    }
}

// ---- animation

Animation::~Animation()
{
}

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
    virtual void rewind();
};


int ColorCycleAnimation::next_offs(int cur) const
{
    return (cur + count + (dir ? -1 : 1)) % count;
}

void ColorCycleAnimation::render_pal(Palette out, const Palette in, int offs1, int offs2, int t) const
{
    // 2-color cycles were probably intended to have sharp transitions
    if (count <= 2)
        t = 0;

    for (int i=0; i < count; i++) {
        const PalEntry &src1 = in[first + ((i + offs1) % count)];
        const PalEntry &src2 = in[first + ((i + offs2) % count)];
        PalEntry &dst = out[first + i];

        dst.r = src1.r + (((src2.r - src1.r) * t) >> 8);
        dst.g = src1.g + (((src2.g - src1.g) * t) >> 8);
        dst.b = src1.b + (((src2.b - src1.b) * t) >> 8);
    }
}

ColorCycleAnimation::ColorCycleAnimation(int first, int last, int delay, int dir)
    : first(first), last(last), delay(delay), dir(dir), count(last - first + 1),
    cur_offs(0), cur_tick(0)
{
    memcpy(orig_a, palette_a, sizeof(Palette));
    memcpy(orig_b, palette_b, sizeof(Palette));
}

void ColorCycleAnimation::tick()
{
    if (++cur_tick < delay)
        return;

    cur_tick = 0;
    cur_offs = next_offs(cur_offs);
}

void ColorCycleAnimation::render()
{
    render_pal(palette_a, orig_a, cur_offs, next_offs(cur_offs), 256 * cur_tick / delay);
    render_pal(palette_b, orig_b, cur_offs, next_offs(cur_offs), 256 * cur_tick / delay);
    set_palette();
}

bool ColorCycleAnimation::is_done() const
{
    return false;
}

void ColorCycleAnimation::rewind()
{
    cur_tick = cur_offs = 0;
}

Animation *new_color_cycle_anim(int first, int last, int delay, int dir)
{
    return new ColorCycleAnimation(first, last, delay, dir);
}

class BigAnimation : public Animation { // .ani / .big files
    U8 *data;
    int frame_size;

    int posx, posy;
    int w, h;
    int last_frame, wait_frames;
    int cur_frame, cur_tick;
    int flags;

    const U8 *get_frame(int frame) const;

public:
    BigAnimation(const char *filename, int flags);
    virtual ~BigAnimation();

    virtual void tick();
    virtual void render();
    virtual bool is_done() const;
    virtual void rewind();
};

const U8 *BigAnimation::get_frame(int frame) const
{
    if (!data || frame < 0 || frame > last_frame)
        return nullptr;

    return data + frame * frame_size;
}

BigAnimation::BigAnimation(const char *filename, int flags)
    : data(nullptr), frame_size(0), posx(0), posy(0), w(0), h(0),
    last_frame(-1), wait_frames(1), cur_frame(0), cur_tick(0),
    flags(flags)
{
    // read file and header stuff
    Slice s = read_file(filename);
    if (s.len() < 11)
        return;

    U8 mode = s[0];
    // bytes 1-3???
    posx = little_u16(&s[4]);
    posy = s[6];
    w = s[7];
    h = s[8];
    last_frame = s[9];
    int fps = s[10];

    wait_frames = 70 / fps;
    frame_size = w * h;
    int nbytes = (last_frame + 1) * frame_size;
    data = new U8[nbytes];

    // read contents (frames are stored in reverse order!)
    if (mode > 0x60) {
        U32 pos = 11;
        for (int frame = last_frame; frame >= 0; frame--) {
            U8 *dst = (U8 *)get_frame(frame);
            if (frame != last_frame)
                memcpy(dst, get_frame(frame + 1), frame_size);

            assert(pos + 2 <= s.len());
            int src_size = little_u16(&s[pos]);
            decode_transparent_rle(dst, &s[pos + 2]);
            pos += src_size;
        }
    } else {
        assert(s.len() == nbytes + 11);
        memcpy(data, &s[11], (last_frame + 1) * frame_size);
    }
}

BigAnimation::~BigAnimation()
{
    delete[] data;
}

void BigAnimation::tick()
{
    if (++cur_tick >= wait_frames) {
        cur_tick = 0;
        cur_frame++;
        if (cur_frame > last_frame) {
            if (flags & BA_PING_PONG) {
                flags ^= BA_REVERSE;
                cur_frame = 1;
            } else if (flags & BA_LOOP)
                cur_frame = 0;
        }
    }
}

void BigAnimation::render()
{
    if (cur_tick)
        return;

    const U8 *frame = get_frame((flags & BA_REVERSE) ? last_frame - cur_frame : cur_frame);
    if (!frame)
        return;

    for (int y=0; y < h; y++)
        memcpy(&vga_screen[(y + posy) * WIDTH + posx], &frame[y * w], w);
}

bool BigAnimation::is_done() const
{
    return cur_frame > last_frame;
}

void BigAnimation::rewind()
{
    cur_frame = cur_tick = 0;
}

Animation *new_big_anim(const char *filename, int flags)
{
    return new BigAnimation(filename, flags);
}

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
    virtual void rewind();
};

MegaAnimation::MegaAnimation(const char *grafilename, const char *prefix, int first_frame,
    int last_frame, int posx, int posy, int delay, int scale, int flip)
    : first_frame(first_frame), last_frame(last_frame), posx(posx), posy(posy),
    delay(delay), scale(scale), flip(flip), cur_frame(first_frame), cur_tick(0)
{
    strcpy(nameprefix, prefix);
    grafile = read_file(grafilename);
    loops_left = last_frame / 1000;
    this->last_frame %= 1000;
}

MegaAnimation::~MegaAnimation()
{
}

void MegaAnimation::tick()
{
    if (++cur_tick >= delay) {
        cur_tick = 0;
        cur_frame++;
        if (cur_frame > last_frame && loops_left) {
            cur_frame = first_frame;
            loops_left--;
        }
    }
}

void MegaAnimation::render()
{
    if (cur_tick)
        return;

    U8 type;
    char name[32];
    sprintf(name, "%s%d", nameprefix, cur_frame);
    int offs = find_gra_item(grafile, name, &type);
    if (offs < 0 || type != 5)
        error_exit("bad anim! (prefix=%s frame=%d offs=%d type=%d)", nameprefix, cur_frame, offs, type);

    decode_delta_gfx(vga_screen, posx, posy, &grafile[offs], scale, flip != 0);
}

bool MegaAnimation::is_done() const
{
    return cur_frame > last_frame;
}

void MegaAnimation::rewind()
{
    cur_frame = first_frame;
    cur_tick = 0;
}

Animation *new_mega_anim(const char *grafilename, const char *prefix, int first_frame, int last_frame,
    int posx, int posy, int delay, int scale, int flip)
{
    return new MegaAnimation(grafilename, prefix, first_frame, last_frame, posx, posy, delay, scale, flip);
}

// ---- screen saving

namespace {
    static const struct SaveDesc {
        void *ptr;
        int size;
    } save_what[] = {
        { vga_screen, sizeof(vga_screen) },
        { vga_pal,    sizeof(vga_pal) },
        { palette_a,  sizeof(palette_a) },
        { palette_b,  sizeof(palette_b) },
    };
}

SavedScreen::SavedScreen()
{
    int total_size = 0;
    for (int i=0; i < ARRAY_COUNT(save_what); i++)
        total_size += save_what[i].size;

    data = new U8[total_size];

    U8 *p = data;
    for (int i=0; i < ARRAY_COUNT(save_what); i++) {
        memcpy(p, save_what[i].ptr, save_what[i].size);
        p += save_what[i].size;
    }
}

SavedScreen::~SavedScreen()
{
    restore();
    delete[] data;
}

void SavedScreen::restore()
{
    U8 *p = data;
    for (int i=0; i < ARRAY_COUNT(save_what); i++) {
        memcpy(save_what[i].ptr, p, save_what[i].size);
        p += save_what[i].size;
    }
}

// ---- .mix files

struct MixItem {
    U8 pasNameStr[31];
    U8 para1l, para1h;
    U8 para2, para3;
    U8 flipX;
};

static void scale_palette(Palette pal, int numColors, int r, int g, int b)
{
    for (int i=0; i < numColors; i++) {
        pal[i].r = MIN(pal[i].r * r / 100, 63);
        pal[i].g = MIN(pal[i].g * g / 100, 63);
        pal[i].b = MIN(pal[i].b * b / 100, 63);
    }
}

static void flipx_screen()
{
    for (int y=0; y < HEIGHT; y++) {
        U8 *row = vga_screen + y*WIDTH;
        for (int x=0; x < WIDTH/2; x++) {
            U8 a = row[x];
            U8 b = row[WIDTH-1-x];
            row[x] = b;
            row[WIDTH-1-x] = a;
        }
    }
}

static void decode_mix(MixItem *items, int count, const char *vbFilename)
{
    // background
    load_background(PascalStr(items->pasNameStr));
    scale_palette(palette_a, 128, items->para1l, items->para2, items->para3);
    if (items->flipX)
        flipx_screen();

    // library
    Slice libFile = read_file(PascalStr(items[1].pasNameStr));
    Slice vbFile = try_read_xored(vbFilename);
    int hotIndex = 0;

    if (vbFile) {
        Slice line = chop_line(vbFile);
        if (line[0] == '#')
            hotIndex = scan_int(line(1));
    }

    // items
    for (int i=2; i < count; i++) {
        PascalStr name(items[i].pasNameStr);
        U8 type;
        int offs = find_gra_item(libFile, name, &type);
        if (offs < 0) {
            printf("didn't find %s!\n", (const char*)name);
            continue;
        }

        Slice vbLine = chop_line(vbFile);
        if (vbLine.len() == 0 || eval_bool_expr(vbLine)) {
            int x = items[i].para1l + (items[i].para1h << 8);
            int y = items[i].para2;

            if (type == 5) // delta
                decode_delta_gfx(vga_screen, x, y, &libFile[offs], items[i].para3, items[i].flipX != 0);
            else if (type == 8) // RLE
                decode_rle(vga_screen + y*WIDTH + x, &libFile[offs]);
        } else {
            // TODO disable hotspot no. hotIndex
        }

        hotIndex++;
    }
}

void load_background(const char *filename)
{
    Slice s = read_file(filename);

    if (has_suffix(filename, ".mix")) {
        char *vbFilename = replace_ext(filename, ".vb");
        decode_mix((MixItem *)&s[0], s.len() / sizeof(MixItem), vbFilename);
        free(vbFilename);
    } else {
        if (!has_suffix(filename, ".pal")) {
            // gross, but this is the original logic from the game
            if (s.len() > 63990)
                memcpy(vga_screen, &s[768], WIDTH * HEIGHT);
            else if (little_u16(&s[768]) == 320 && little_u16(&s[770]) == 200)
                decode_rle(vga_screen, &s[772]);
            else
                decode_delta(vga_screen, &s[768]);
        }

        memcpy(palette_a, &s[0], 768);
        fix_palette(palette_a);
    }
}
