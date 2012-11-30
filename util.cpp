#define _CRT_SECURE_NO_DEPRECATE
#include "common.h"
#include "util.h"
#include "str.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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
            panic("out of memory");
    }

    ~Buffer()
    {
        delete[] data;
    }

    static void ref(Buffer *x)      { if (x) x->nrefs++; }
    static void unref(Buffer *x)    { if (x && --x->nrefs == 0) delete x; }
};

void Slice::fini()
{
    Buffer::unref(buf);
}

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
    fini();
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

Slice Slice::clone() const
{
    Slice s = make(len());
    memcpy(&s[0], ptr, len());
    return s;
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

Slice try_read_file(const Str &filename)
{
    FILE *f = fopen(filename.c_str(), "rb");
    if (!f)
        return Slice();

    int sz = fsize(f);
    Slice s = Slice::make(sz);

    fread(&s[0], sz, 1, f);
    fclose(f);

    return s;
}

Slice read_file(const Str &filename)
{
    Slice s = try_read_file(filename.c_str());
    if (!s)
        panic("%s not found", filename.c_str());
    return s;
}

void write_file(const Str &filename, const void *buf, int size)
{
    FILE *f = fopen(filename.c_str(), "wb");
    if (!f)
        panic("couldn't open %s for writing", filename.c_str());

    fwrite(buf, size, 1, f);
    fclose(f);
}

static void dexor(U8 *buffer, int nbytes, int *start)
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

Slice try_read_xored(const Str &filename)
{
    Slice s = try_read_file(filename);
    if (!s || s.len() < 2)
        return s;

    int start;
    dexor(&s[0], s.len(), &start);
    return s(start);
}

Slice read_xored(const Str &filename)
{
    Slice s = try_read_xored(filename);
    if (!s)
        panic("%s not found", filename.c_str());
    return s;
}

// ---- decoding helpers

int little_u16(const U8 *p)
{
    return p[0] + p[1]*256;
}

static bool is_printable(char ch)
{
    return (ch >= 32 && ch <= 127);
}

void print_hex(const Str &desc, const Slice &what, int bytes_per_line)
{
    printf("%s", desc.c_str());
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

int find_gra_item(const Slice &grafile, const Str &name, U8 *type)
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

Str to_string(const Slice &sl)
{
    Slice &s = (Slice &)sl;
    return Str((const char *)&s[0], (const char *)&s[0] + s.len());
}

Slice chop(Slice &from, int len)
{
    Slice first = from(0, len);
    from = from(len);
    return first;
}

Slice chop_until(Slice &from, U8 sep)
{
    void *p = memchr(&from[0], sep, from.len());
    int len = from.len();
    if (p)
        len = (int) ((U8 *)p - &from[0]);

    Slice s = from(0, len);
    from = from(len + 1);
    return s;
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
