#define _CRT_SECURE_NO_DEPRECATE
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

// ---- slices

struct Buffer
{
    U8 *data;
    U32 nrefs;

    Buffer(U32 capacity)
    {
        data = new U8[capacity];
        nrefs = 0;

        if (!data)
            errorExit("out of memory");
    }

    ~Buffer()
    {
        delete[] data;
    }

    void ref()      { if (this) nrefs++; }
    void unref()    { if (this && --nrefs == 0) delete this; }
};

Slice::Slice()
    : buf(0), ptr(0), length(0)
{
}

Slice::Slice(Buffer *b, U32 len)
    : buf(b), ptr(b->data), length(len)
{
    buf->ref();
}

Slice::Slice(const Slice &x)
    : buf(x.buf), ptr(x.ptr), length(x.length)
{
    buf->ref();
}

Slice::~Slice()
{
    buf->unref();
}

Slice Slice::make(U32 nbytes)
{
    return Slice(new Buffer(nbytes), nbytes);
}

Slice &Slice::operator =(const Slice &x)
{
    x.buf->ref();
    buf->unref();
    buf = x.buf;
    ptr = x.ptr;
    length = x.length;
    return *this;
}

const Slice Slice::operator()(U32 start, U32 end) const
{
    if (end < start)
        return Slice();

    if (start > length) start = length;
    if (end > length)   end = length;

    Slice s(*this);
    s.ptr += start;
    s.length = end - start;
    return s;
}

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

Slice try_read_file(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
        return Slice();

    int sz = fsize(f);
    Slice s = Slice::make(sz);

    fread(&s[0], sz, 1, f);
    fclose(f);

    return s;
}

Slice read_file(const char *filename)
{
    Slice s = try_read_file(filename);
    if (!s)
        errorExit("%s not found", filename);
    return s;
}

void write_file(const char *filename, const void *buf, int size)
{
    FILE *f = fopen(filename, "wb");
    if (!f)
        errorExit("couldn't open %s for writing", filename);

    fwrite(buf, size, 1, f);
    fclose(f);
}

Slice read_xored(const char *filename)
{
    Slice s = read_file(filename);
    if (!s || s.len() < 2)
        return s;

    int start;
    decrypt(&s[0], s.len(), &start);
    return s(start);
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

int find_gra_item(Slice grafile, const char *name, U8 *type)
{
    int dir_size = little_u16(&grafile[0]);
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
            int offs = little_u16(&grafile[pos + 1]);
            int seg = little_u16(&grafile[pos + 3]);
            return dir_size + (seg << 4) + offs;
        }

        // no match, continue looking
        pos += 1 + 4;
    }

    return -1;
}

static void decode_level_rle(U8 *dest, U8 *&src)
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
    Slice s = read_file(filename);
    U8 *bytes = &s[0];
    
    int offs = little_u16(bytes + level*2);
    int size = little_u16(bytes + level*2 + 2) - offs;
    U8 *prle = bytes + 100 + offs;
    U8 *pend = prle + size;

    U8 levelData[2*2000];
    decode_level_rle(levelData + 0*2000, prle);
    decode_level_rle(levelData + 1*2000, prle);

    write_file("out_level.dat", levelData, 2*2000);
    write_file("script_level.dat", prle, pend - prle);
}