#define _CRT_SECURE_NO_DEPRECATE
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

// ---- string handling

PascalStr::PascalStr(const U8 *pstr)
{
    int len = pstr[0];
    data = new char[len + 1];
    memcpy(data, pstr + 1, len);
    data[len] = 0;
}

PascalStr::~PascalStr()
{
    delete[] data;
}

bool has_suffix(const char *str, const char *suffix)
{
    int len = strlen(str);
    int lensuf = strlen(suffix);
    if (lensuf > len)
        return false;

    return _stricmp(str + (len - lensuf), suffix) == 0;
}

char *replace_ext(const char *filename, const char *newext)
{
    const char *dot = strrchr(filename, '.');
    if (!dot)
        return _strdup(filename);
    else {
        int lenpfx = dot - filename;
        int lenext = strlen(newext);
        char *str = (char *)malloc(lenpfx + lenext + 1);
        memcpy(str, filename, lenpfx);
        memcpy(str + lenpfx, newext, lenext);
        str[lenpfx + lenext] = 0;
        return str;
    }
}

// ---- file IO

static int fsize(FILE *f)
{
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    int sz = ftell(f);
    fseek(f, pos, SEEK_SET);
    return sz;
}

U8 *try_read_file(const char *filename, int *size)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
        return 0;

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

U8 *read_file(const char *filename, int *size)
{
    U8 *out = try_read_file(filename, size);
    if (!out)
        errorExit("%s not found", filename);
    return out;
}

void read_file_to(void *buf, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
        errorExit("%s not found", filename);

    fread(buf, fsize(f), 1, f);
    fclose(f);
}

void write_file(const char *filename, const void *buf, int size)
{
    FILE *f = fopen(filename, "wb");
    if (!f)
        errorExit("couldn't open %s for writing", filename);

    fwrite(buf, size, 1, f);
    fclose(f);
}

U8 *read_xored(const char *filename, int *size)
{
    int sz, start;
    if (!size)
        size = &sz;

    U8 *bytes = read_file(filename, size);
    if (!bytes || *size < 2)
        return bytes;

    decrypt(bytes, *size, &start);
    if (start) {
        *size -= start;
        memcpy(bytes, bytes + start, *size);
    }
    return bytes;
}

// ---- decoding helpers

int little_u16(const U8 *p)
{
    return p[0] + p[1]*256;
}

static int get_le16(U8 *&p)
{
    int val = little_u16(p);
    p += 2;
    return val;
}

void decode_rle(U8 *dst, const U8 *src)
{
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
}

void decode_delta(U8 *dst, const U8 *p)
{
    for (;;) {
        int skip = little_u16(p);
        p += 2;
        if (!skip)
            break;

        dst += skip;

        int len = little_u16(p);
        memcpy(dst, p + 2, len);
        dst += len;
        p += len + 2;

        p++; // what does this byte do?
    }
}

void decode_delta_gfx(U8 *dst, int x, int y, const U8 *p, int scale, bool flipX)
{
    // our dest coords are in quarter-pixel coordinates.
    int step = 4 / scale;
    int linepos = 0;
    int linemax = WIDTH * 4; // number of quarter-pixels to see before we go to next screen line
    unsigned drawmax = WIDTH * step;

    dst += y * WIDTH + x;

    if (flipX) {
        step = -step;
        linepos = drawmax + step;
        // move dst slightly to compensate for high linepos
        dst -= linepos / 4;
    }

    for (;;) {
        int skip = little_u16(p);
        p += 2;
        if (!skip)
            break;
        linepos += step * skip;

        // did we advance into the next line?
        while (linepos < 0) {
            linepos += linemax;
            dst += WIDTH;
            y++;
        }

        while (linepos >= linemax) {
            linepos -= linemax;
            dst += WIDTH;
            y++;
        }

        // bottom clip
        if (y >= 144)
            break;

        // actual pixels!
        int len = little_u16(p);
        if (y < 32) // top clip
            linepos += len * step;
        else {
            for (int i=0; i < len; i++) {
                if ((unsigned) linepos < drawmax && (linepos & 3) == 0)
                    dst[linepos >> 2] = p[i+2];

                linepos += step;
            }
        }

        p += len + 2;
        p++; // what does this byte do?
    }
}

void decrypt(U8 *buffer, int nbytes, int *start)
{
    if (buffer[0] == 0x0a && buffer[1] == 0x00) // already de-xored
        *start = 2;
    else if (buffer[0] == 0x5c) {
        U8 key = buffer[0];
        for (int i=0; i < nbytes; i++)
            buffer[i] ^= key;

        *start = 2;
    } else
        *start = 0;
}

int find_gra_item(U8 *grafile, const char *name, U8 *type)
{
    int dir_size = little_u16(grafile);
    int pos = 2;
    while (pos < dir_size) {
        int len = 0, matchlen = 0;
        while (grafile[pos+len] >= ' ') {
            if (name[matchlen] &&
                toupper(grafile[pos+len]) == toupper(name[matchlen]))
                matchlen++;
            len++;
        }
        pos += len;

        if (len == matchlen && !name[matchlen]) { // found our file
            *type = grafile[pos];
            int offs = little_u16(grafile + pos + 1);
            int seg = little_u16(grafile + pos + 3);
            return dir_size + (seg << 4) + offs;
        }

        // no match, continue looking
        pos += 1 + 4;
    }

    return -1;
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

    int dir_size = little_u16(p);
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

        U16 offs = little_u16(p + pos + 0);
        U16 seg = little_u16(p + pos + 2);
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
    U8 *bytes = read_file(filename);
    U8 *p = bytes;

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

static void decodeLevelRLE(U8 *dest, U8 *&src)
{
    int offs = 0;
    while (offs < 2000) {
        int count = *src++;
        U8 val = *src++;
        while (count--)
            dest[offs++] = val;
    }
}

void decode_level(const char *filename, int level)
{
    U8 *bytes = read_file(filename);
    
    int offs = little_u16(bytes + level*2);
    int size = little_u16(bytes + level*2 + 2) - offs;
    U8 *prle = bytes + 100 + offs;
    U8 *pend = prle + size;

    U8 levelData[2*2000];
    decodeLevelRLE(levelData + 0*2000, prle);
    decodeLevelRLE(levelData + 1*2000, prle);

    write_file("out_level.dat", levelData, 2*2000);
    write_file("script_level.dat", prle, pend - prle);
}