#ifndef __UTIL_H__
#define __UTIL_H__

#include "common.h"

struct Buffer;

class Slice {
    Buffer *buf;    // underlying storage
    U8 *ptr;        // start of data
    U32 length;     // length

    Slice(Buffer *b, U32 len);

public:
    explicit Slice();
    Slice(const Slice &x);
    ~Slice();

    static Slice make(U32 nbytes);

    Slice &operator =(const Slice& x);

    // actual slicing
    const Slice operator ()(U32 start, U32 end=~0u) const;
    Slice operator ()(U32 start, U32 end=~0u)   { return (Slice) ((const Slice&) *this)(start, end); }

    const U8 &operator [](U32 i) const  { return ptr[i]; }
    U8 &operator [](U32 i)              { return ptr[i]; }

    operator void *() const             { return buf ? buf : nullptr; }
    U32 len() const                     { return length; }
};

struct PascalStr {
    explicit PascalStr(const U8 *pstr);
    ~PascalStr();

    operator const char *() const { return data; }

    char *data;
};

bool has_suffix(const char *str, const char *suffix); // case insensitive
char *replace_ext(const char *filename, const char *newext);

Slice try_read_file(const char *filename);
Slice read_file(const char *filename);
void write_file(const char *filename, const void *buf, int size);
Slice try_read_xored(const char *filename);
Slice read_xored(const char *filename);

int little_u16(const U8 *p);
void decode_rle(U8 *dst, const U8 *src);
void decode_transparent_rle(U8 *dst, const U8 *src);
int decode_delta(U8 *dst, const U8 *src); // returns number of bytes decoded
void decode_delta_gfx(U8 *dst, int x, int y, const U8 *src, int scale, bool flipX);
void decrypt(U8 *buffer, int nbytes, int *start);

void print_hex(const char *name, const Slice &what, int bytes_per_line=16);
void list_gra_contents(const Slice &grafile); // for debugging
int find_gra_item(const Slice &grafile, const char *name, U8 *type);

void decode_level(const char *filename, int level);

#endif
