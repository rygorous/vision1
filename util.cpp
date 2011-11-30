#define _CRT_SECURE_NO_DEPRECATE
#include "common.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int fsize(FILE *f)
{
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    int sz = ftell(f);
    fseek(f, pos, SEEK_SET);
    return sz;
}

U8 *read_file(const char *filename, int *size)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
        errorExit("%s not found", filename);

    int sz = fsize(f);
    U8 *buf = new U8[sz];
    if (!buf) {
        fclose(f);
        errorExit("out of memory");
    }

    fread(buf, sz, 1, f);
    fclose(f);

    if (size) *size = sz;

    return buf;
}

void read_file_to(void *buf, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
        errorExit("%s not found", filename);

    fread(buf, fsize(f), 1, f);
    fclose(f);
}

// ---- decoding helpers

static int le16(U8 *p)
{
    return p[0] + p[1]*256;
}

static int get_le16(U8 *&p)
{
    int val = le16(p);
    p += 2;
    return val;
}

static void decode_rle(U8 *dst, U8 *src)
{
    U8 *start = src;
    U8 *orig_dst = dst;

    for (;;) {
        U8 cmd = *src++;
        if (cmd == 0xff) { // run
            U8 len = *src++;
            if (!len)
                break;

            U8 val = *src++;
            memset(dst, val, len);
            dst += len;
        } else
            *dst++ = cmd;
    }

    printf("rle: 0x%x bytes decoded (to %d bytes)\n", src - start, dst - orig_dst);
}

static void decode_delta(U8 *dst, U8 *p)
{
    U8 *orig_dst = dst;
    U8 *start = p;

    for (;;) {
        int skip = get_le16(p);
        if (!skip)
            break;

        dst += skip;

        int len = get_le16(p);
        memcpy(dst, p, len);
        dst += len;
        p += len;

        p++; // what does this byte do?
    }

    printf("delta: 0x%x bytes decoded (to %d bytes)\n", p - start, dst - orig_dst);
}

static void overlay(U8 *dst, U8 *src, int count)
{
    for (int i=0; i < count; i++)
        if (src[i])
            dst[i] = src[i];
}

// actual display functions

void display_raw_pic(const char *filename)
{
    int size;
    U8 *bytes = read_file(filename, &size);
    assert(size == WIDTH*HEIGHT + 256*sizeof(PalEntry));

    memcpy(vga_pal, bytes, 256 * sizeof(PalEntry));
    memcpy(vga_screen, bytes + 256 * sizeof(PalEntry), WIDTH * HEIGHT);

    delete[] bytes;
}

void display_pic(const char *filename)
{
    int size;
    U8 *bytes = read_file(filename, &size);
    U8 *p = bytes;

    memcpy(vga_pal, p, 256 * sizeof(PalEntry));
    p += 256 * sizeof(PalEntry);

    // TODO do something with this!
    int w = get_le16(p);
    int h = get_le16(p);

    decode_rle(vga_screen, p);
    delete[] bytes;
}

void display_hot(const char *filename)
{
    int size;
    U8 *bytes = read_file(filename, &size);
    U8 *p = bytes;

    int w = get_le16(p);
    int h = get_le16(p);

    U8 *buf = new U8[w*h];
    decode_rle(buf, p);
    delete[] bytes;

    // no clue why it's stored the way it is...
    w /= 2;
    h *= 2;

    for (int y=0; y < h*2; y++)
    {
        U8 *srcrow = buf + (y/2) * w;
        U8 *dstrow = vga_screen + y * WIDTH;
        for (int x=0; x < w*2; x++)
            if (srcrow[x/2])
                dstrow[x] = srcrow[x/2];
    }

    delete[] buf;
}

void display_face(const char *filename)
{
    int size;
    U8 *bytes = read_file(filename, &size);
    U8 *p = bytes;
    U8 *end = p + size;

    // skip first 128 pal entries
    // TODO figure out how actual palette remapping works in game
    p += 128 * sizeof(PalEntry);
    memcpy(vga_pal + 128, p, 128 * sizeof(PalEntry));
    p += 128 * sizeof(PalEntry);

    decode_delta(vga_screen, p);
    delete[] bytes;
}

bool display_gra(const char *filename, int index)
{
    int size;
    U8 *bytes = read_file(filename, &size);
    U8 *p = bytes;
    U8 *end = p + size;

    int dir_size = le16(p);
    int start_offs = dir_size;

    int pos = 2;
    int counter = 0;
    int type = 0;

    while (pos < dir_size) {
        char name[256];
        int len = 0;
        while (p[pos+len] >= 0x20) {
            name[len] = p[pos+len];
            len++;
        }

        type = p[pos+len];
        name[len++] = 0;
        pos += len;

        if (len == 1) {
            delete[] bytes;
            return false;
        }

        U16 offs = le16(p + pos + 0);
        U16 seg = le16(p + pos + 2);
        pos += 4;

        int file_offs = (seg << 4) + offs + dir_size;
        if (index == counter) {
            printf("showing %s\n", name);
            start_offs = file_offs;
            break;
        }

        counter++;
    }

    if (type == 5)
        decode_delta(vga_screen, p + start_offs);
    else if (type == 8) {
        p += start_offs;
        int w = get_le16(p);
        int h = get_le16(p);
        U8 *buf = new U8[w*h];
        decode_rle(buf, p);

        int x0 = (WIDTH - w) / 2;
        int y0 = (HEIGHT - h) / 2;

        for (int y=0; y < h; y++)
            memcpy(vga_screen + (y+y0) * WIDTH + x0, buf + y*w, w);

        delete[] buf;
    } else
        printf("UNKNOWN TYPE 0x%x!\n", type);

    delete[] bytes;

    return true;
}

void display_blk(const char *filename)
{
    int size;
    U8 *bytes = read_file(filename, &size);
    U8 *p = bytes;
    U8 *end = p + size;

    int w = get_le16(p);
    int h = get_le16(p);
    U8 *buf = new U8[w*h];
    decode_rle(buf, p);
    delete[] bytes;

    int x0 = (WIDTH - w) / 2;
    int y0 = (HEIGHT - h) / 2;

    for (int y=0; y < h; y++)
        overlay(vga_screen + (y0+y)*WIDTH + x0, buf + y*w, w);

    delete[] buf;
}