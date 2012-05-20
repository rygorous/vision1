#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Palette palette_a, palette_b;

GfxBlock::GfxBlock()
{
    pixels = nullptr;
    w = h = 0;
}

GfxBlock::~GfxBlock()
{
    delete[] pixels;
}

void GfxBlock::load(const char *filename)
{
    delete[] pixels;

    Slice s = read_file(filename);
    w = little_u16(&s[0]);
    h = little_u16(&s[2]);
    pixels = new U8[w*h];
    decode_rle(pixels, &s[4]);
}

void GfxBlock::resize(int neww, int newh)
{
    if (!pixels)
        return;

    U8 *newpix = new U8[neww*newh];
    memset(newpix, 0, neww*newh);
    for (int y=0; y < MIN(h, newh); y++)
        memcpy(newpix + y*neww, pixels + y*w, MIN(w, neww));

    delete[] pixels;
    pixels = newpix;
    w = neww;
    h = newh;
}

const U8 *Animation::get_frame(int frame) const
{
    if (!data || frame < 0 || frame >= num_frames)
        return nullptr;

    return data + frame * frame_size;
}

Animation::Animation()
    : data(nullptr)
{
    clear();
}

Animation::~Animation()
{
    clear();
}

void Animation::clear()
{
    delete[] data;
    data = nullptr;
    frame_size = 0;
    posx = posy = w = h = 0;
    num_frames = wait_frames = 0;
    cur_frame = cur_tick = 0;
}

void Animation::load(const char *filename)
{
    clear();

    // read file and header stuff
    Slice s = read_file(filename);
    if (s.len() < 11)
        return;

    posx = little_u16(&s[4]);
    posy = s[6];
    w = s[7];
    h = s[8];
    num_frames = s[9];
    int fps = s[10];

    wait_frames = 70 / fps;
    frame_size = w * h;
    data = new U8[num_frames * frame_size];

    // read contents (frames are stored in reverse order!)
    int pos = 11;
    for (int frame = num_frames - 1; frame >= 0; frame--) {
        U8 *dst = (U8 *)get_frame(frame);
        if (frame != num_frames - 1)
            memcpy(dst, get_frame(frame + 1), frame_size);

        int src_size = little_u16(&s[pos]);
        decode_rle(dst, &s[pos + 2]);
        pos += src_size;
    }
}

void Animation::tick()
{
    if (++cur_tick >= wait_frames) {
        cur_tick = 0;
        cur_frame++;
    }
}

void Animation::render()
{
    const U8 *frame = get_frame(cur_frame);
    if (!frame)
        return;

    for (int y=0; y < h; y++)
        memcpy(&vga_screen[(y + posy) * WIDTH + posx], &frame[y * w], w);
}

bool Animation::is_done() const
{
    return cur_frame >= num_frames;
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
    for (int i=0; i < 256; i++) {
        vga_pal[i].r = (palette_b[i].r * intensity) >> 8;
        vga_pal[i].g = (palette_b[i].g * intensity) >> 8;
        vga_pal[i].b = (palette_b[i].b * intensity) >> 8;
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

    // items
    for (int i=2; i < count; i++) {
        PascalStr name(items[i].pasNameStr);
        U8 type;
        int offs = find_gra_item(libFile, name, &type);
        if (offs < 0) {
            printf("didn't find %s!\n", (const char*)name);
            continue;
        }

        // TODO evaluate VB file!

        int x = items[i].para1l + (items[i].para1h << 8);
        int y = items[i].para2;

        if (type == 5) // delta
            decode_delta_gfx(vga_screen, x, y, &libFile[offs], items[i].para3, items[i].flipX != 0);
        else if (type == 8) // RLE
            decode_rle(vga_screen + y*WIDTH + x, &libFile[offs]);
    }

    /*
    int vbSize;
    if (U8 *vbBytes = try_read_file(vbFilename, &vbSize)) {
        int pos;
        decrypt(vbBytes, vbSize, &pos);
        while (pos < vbSize) {
            char command[101], *p = command;

            // get next command
            while (pos < vbSize && vbBytes[pos] >= ' ') {

            }
            pos += 2; // skip CRLF
        }

        delete[] vbBytes;
    }*/
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