#ifndef __UTIL_H__
#define __UTIL_H__

#include "common.h"

class Str;
struct Buffer;

class Slice {
    Buffer *buf;    // underlying storage
    U8 *ptr;        // start of data
    U32 length;     // length

    Slice(Buffer *b, U32 len);

    void fini();
    void move_from(Slice &x)            { buf = x.buf; ptr = x.ptr; length = x.length; x.buf = 0; }

public:
    explicit Slice();
    Slice(Slice &&x)                    { move_from(x); }
    Slice(const Slice &x);
    ~Slice();

    static Slice make(U32 nbytes);

    Slice &operator =(const Slice& x);
    Slice &operator =(Slice &&x)        { if (this != &x) { fini(); move_from(x); } return *this; }

    // actual slicing
    const Slice operator ()(U32 start, U32 end=~0u) const;
    Slice operator ()(U32 start, U32 end=~0u)   { return (Slice) ((const Slice&) *this)(start, end); }

    Slice clone() const;

    const U8 &operator [](U32 i) const  { return ptr[i]; }
    U8 &operator [](U32 i)              { return ptr[i]; }

    operator void *() const             { return buf ? buf : nullptr; }
    U32 len() const                     { return length; }
};

Slice try_read_file(const Str &filename);
Slice read_file(const Str &filename);
void write_file(const Str &filename, const void *buf, int size);
Slice try_read_xored(const Str &filename);
Slice read_xored(const Str &filename);

int little_u16(const U8 *p);

void print_hex(const char *name, const Slice &what, int bytes_per_line=16);
void list_gra_contents(const Slice &grafile); // for debugging
int find_gra_item(const Slice &grafile, const Str &name, U8 *type);

Str to_string(const Slice &sl);

Slice chop(Slice &from, int len); // return first len bytes of "from", modifies "from" to be the rest
Slice chop_until(Slice &from, U8 sep); // chop until first 'sep' byte - sep itself isn't included in either Slice!
Slice chop_line(Slice &scan_buf); // returns first line, slices it off scan_buf
Slice eat_heading_space(Slice text); // eats any white space characters at start
int scan_int(Slice &scan_buf);

#endif
