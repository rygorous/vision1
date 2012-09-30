#define _CRT_SECURE_NO_DEPRECATE
#include "common.h"
#include "util.h"
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
            error_exit("out of memory");
    }

    ~Buffer()
    {
        delete[] data;
    }

    static void ref(Buffer *x)      { if (x) x->nrefs++; }
    static void unref(Buffer *x)    { if (x && --x->nrefs == 0) delete x; }
};

Slice::Slice()
    : buf(0), ptr(0), length(0)
{
}

Slice::Slice(Buffer *b, U32 len)
    : buf(b), ptr(b->data), length(len)
{
    Buffer::ref(buf);
}

Slice::Slice(const Slice &x)
    : buf(x.buf), ptr(x.ptr), length(x.length)
{
    Buffer::ref(buf);
}

Slice::~Slice()
{
    Buffer::unref(buf);
}

Slice Slice::make(U32 nbytes)
{
    return Slice(new Buffer(nbytes), nbytes);
}

Slice &Slice::operator =(const Slice &x)
{
    Buffer::ref(x.buf);
    Buffer::unref(buf);
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
        error_exit("%s not found", filename);
    return s;
}

void write_file(const char *filename, const void *buf, int size)
{
    FILE *f = fopen(filename, "wb");
    if (!f)
        error_exit("couldn't open %s for writing", filename);

    fwrite(buf, size, 1, f);
    fclose(f);
}

Slice try_read_xored(const char *filename)
{
    Slice s = try_read_file(filename);
    if (!s || s.len() < 2)
        return s;

    int start;
    decrypt(&s[0], s.len(), &start);
    return s(start);
}

Slice read_xored(const char *filename)
{
    Slice s = try_read_xored(filename);
    if (!s)
        error_exit("%s not found", filename);
    return s;
}

// ---- decoding helpers

int little_u16(const U8 *p)
{
    return p[0] + p[1]*256;
}

void decrypt(U8 *buffer, int nbytes, int *start)
{
    if (buffer[0] == 0x0a && buffer[1] == 0x00) // already de-xored
        *start = 2;
    else if (buffer[0] == 0x5c) {
        U8 key = buffer[1];
        for (int i=0; i < nbytes; i++)
            buffer[i] ^= key;

        *start = 2;
    } else
        *start = 0;
}

static bool is_printable(char ch)
{
    return (ch >= 32 && ch <= 127);
}

void print_hex(const char *desc, const Slice &what, int bytes_per_line)
{
    printf("%s:\n", desc);
    int len = what.len();
    for (int i=0; i < len; i += bytes_per_line) {
        int j;
        printf("[%04x]", i);
        for (j=i; j < i + bytes_per_line && j < len; j++)
            printf(" %02x", what[j]);
        printf("%*s", (i + bytes_per_line - j) * 3 + 1, "");
        for (j=i; j < i + bytes_per_line && j < len; j++)
            putc(is_printable(what[j]) ? what[j] : '.', stdout);
        putc('\n', stdout);
    }
    printf("\n");
}

void list_gra_contents(const Slice &grafile)
{
    int dir_size = little_u16(&grafile[0]);
    int pos = 2;
    while (pos < dir_size) {
        int len = 0;
        while (grafile[pos+len] >= ' ')
            len++;
        U8 type = grafile[pos+len];
        printf("  %.*s type=%d\n", len, &grafile[pos], type);
        pos += len + 1 + 4;
    }
}

int find_gra_item(const Slice &grafile, const char *name, U8 *type)
{
    int dir_size = little_u16(&grafile[0]);
    int pos = 2;
    while (pos < dir_size) {
        int len = 0, matchlen = 0;
        while (grafile[pos+len] >= ' ') {
            if (name[matchlen] &&
                toupper(grafile[pos+len]) == toupper((U8)name[matchlen]))
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

std::string to_string(const Slice &sl)
{
    Slice &s = (Slice &)sl;
    return std::string(&s[0], &s[0] + s.len());
}

static bool islinespace(U8 ch)
{
    return ch == '\r' || ch == '\n';
}

Slice chop_line(Slice &buf)
{
    // find end of this line
    U32 pos = 0;
    while (pos < buf.len() && !islinespace(buf[pos]))
        pos++;
    Slice line = buf(0, pos);

    // find LF to find start of next line
    while (pos < buf.len() && buf[pos] != '\n')
        pos++;
    if (pos < buf.len())
        pos++;
    buf = buf(pos);

    return line;
}

Slice eat_heading_space(Slice text)
{
    U32 pos = 0;
    while (pos < text.len() && (text[pos] == ' ' || text[pos] == '\t'))
        pos++;
    return text(pos);
}

int scan_int(Slice &buf)
{
    U32 pos = 0;
    int val = 0, sign = 1;

    if (buf.len() && buf[0] == '-') {
        pos++;
        sign = -1;
    }

    while (pos < buf.len() && isdigit(buf[pos]))
        val = (val * 10) + (buf[pos++] - '0');

    buf = buf(pos);
    return val * sign;
}
