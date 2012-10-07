#ifndef __STR_H__
#define __STR_H__

#include <assert.h>
#include <stdarg.h>

class Str {
    char *buf;      // never 0!
    int alen, acap; // alloced len/cap

    void alloc(int maxlen);
    void init(const void *data, int len);
    void fini();
    void grow();
    void grow_to(int newlen);

    void move_from(Str &x)          { buf = x.buf; alen = x.alen; acap = x.acap; x.acap = 0; }

public:
    Str();
    Str(const char *cstr);
    Str(const Str &x);
    Str(Str &&x)                    { move_from(x); }
    explicit Str(int cap);
    Str(const char *st, const char *end) { init(st, (int) (end - st)); }
    ~Str();

    Str &operator =(const Str &x);
    Str &operator =(Str &&x)        { if (this != &x) { fini(); move_from(x); } return *this; }

    static Str pascl(const unsigned char *l); // from string with len byte (pascal)
    static Str fmt(const char *fmtstr, ...);
    static Str vfmt(const char *fmtstr, va_list arg);

    const char *c_str() const       { return buf; }
    bool empty() const              { return alen == 0; }
    int size() const                { return alen; }
    char back() const               { assert(alen); return buf[alen - 1]; }
    Str substr(int pos, int n=-1) const;

    char &operator[](int i)             { assert(i <= alen); return buf[i]; }
    const char &operator[](int i) const { assert(i <= alen); return buf[i]; }

    bool operator ==(const Str& b) const;
    bool operator !=(const Str& b) const;

    void push_back(char ch)         { if (alen + 1 >= acap) grow(); buf[alen++] = ch; buf[alen] = 0; }
    Str &operator +=(const Str &b);
};

Str operator +(const Str &a, const Str &b);

Str tolower(const Str &s);
Str replace_ext(const Str &filename, const Str &newext);

// All case insensitive
bool has_prefixi(const char *str, const char *prefix);
inline bool has_prefixi(const Str &str, const char *prefix) { return has_prefixi(str.c_str(), prefix); }
inline bool has_prefixi(const char *str, const Str &prefix) { return has_prefixi(str, prefix.c_str()); }
inline bool has_prefixi(const Str &str, const Str &prefix)  { return has_prefixi(str.c_str(), prefix.c_str()); }

bool has_suffixi(const char *str, const char *suffix);
inline bool has_suffixi(const Str &str, const char *prefix) { return has_suffixi(str.c_str(), prefix); }
inline bool has_suffixi(const char *str, const Str &prefix) { return has_suffixi(str, prefix.c_str()); }
inline bool has_suffixi(const Str &str, const Str &prefix)  { return has_suffixi(str.c_str(), prefix.c_str()); }

#endif