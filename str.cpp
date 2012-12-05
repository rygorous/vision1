#define _CRT_SECURE_NO_DEPRECATE
#include "str.h"
#include "common.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <malloc.h>
#include <ctype.h>

void Str::alloc(int maxlen)
{
    alen = 0;
    acap = maxlen + 1;
    buf = (char *)malloc(acap);
    if (!buf)
        panic("out of memory");
    buf[0] = 0;
}

void Str::init(const void *data, int len)
{
    if (!data || !len) {
        static char empty[] = "";
        buf = empty;
        alen = acap = 0;
    } else {
        alloc(len);
        alen = len;
        memcpy(buf, data, len);
        buf[len] = 0;
    }
}

void Str::fini()
{
    if (acap)
        free(buf);
}

void Str::grow()
{
    grow_to(acap*2);
}

void Str::grow_to(int newlen)
{
    assert(newlen >= acap);
    newlen = MAX(newlen, 16);
    newlen = MAX(newlen, acap+acap/2); // geometric growth factor
    if (!acap)
        alloc(newlen);
    else {
        acap = newlen + 1;
        buf = (char *)realloc(buf, acap);
        if (!buf)
            panic("out of memory");
    }
}

Str::Str()
{
    init(0, 0);
}

Str::Str(const char *cstr)
{
    init(cstr, strlen(cstr));
}

Str::Str(const Str &x)
{
    init(x.buf, x.alen);
}

Str::Str(int cap)
{
    alloc(cap);
}

Str::~Str()
{
    fini();
}

Str &Str::operator =(const Str &x)
{
    if (this != &x) {
        fini();
        init(x.buf, x.size());
    }
    return *this;
}

Str Str::pascl(const unsigned char *l)
{
    Str s;
    s.init(l + 1, l[0]);
    return s;
}

Str Str::fmt(const char *fmtstr, ...)
{
    va_list arg;
    va_start(arg, fmtstr);
    Str s = vfmt(fmtstr, arg);
    va_end(arg);
    return s;
}

#ifdef _MSC_VER
#define vsprintf_count(fmt, arg) _vscprintf(fmt, arg)
#define va_copy(dest, src) dest = src
#else
#define vsprintf_count(fmt, arg) vsnprintf(0, 0, fmt, arg)
#endif

Str Str::vfmt(const char *fmtstr, va_list arg)
{
    va_list argc;

    va_copy(argc, arg);
    int len = vsprintf_count(fmtstr, argc);

    Str s;
    s.alloc(len);
    va_copy(argc, arg);
    vsnprintf(s.buf, s.acap, fmtstr, argc);
    s.alen = len;

    return s;
}

Str Str::substr(int pos, int n) const
{
    if (pos >= alen)
        return Str();

    if (n < 0)
        n = alen;
    return Str(&buf[pos], &buf[pos+MIN(alen-pos, n)]);
}

bool Str::operator ==(const Str& b) const
{
    return alen == b.alen && memcmp(buf, b.buf, alen) == 0;
}

bool Str::operator !=(const Str &b) const
{
    return !(*this == b);
}

Str &Str::operator +=(const Str &b)
{
    int sumlen = alen + b.alen;
    if (sumlen + 1 > acap)
        grow_to(sumlen);
    memcpy(&buf[alen], b.buf, b.alen);
    alen += b.alen;
    buf[alen] = 0;
    return *this;
}

Str operator +(const Str &a, const Str &b)
{
    Str s(a.size() + b.size());
    s += a;
    s += b;
    return s;
}

Str tolower(const Str &s)
{
    Str x = s;
    for (int i=0; i < x.size(); i++)
        x[i] = tolower(x[i]);
    return x;
}

Str replace_ext(const Str &filename, const Str &newext)
{
    const char *dot = strrchr(&filename[0], '.');
    if (!dot)
        return filename;
    else
        return filename.substr(0, (int)(dot - &filename[0])) + newext;
}

Str chop(Str &from, int len)
{
    Str ret = from.substr(0, len);
    from = from.substr(len);
    return ret;
}

Str chop_until(Str &from, char sep)
{
    int len = from.size();
    int pos = 0;
    while (pos < len && from[pos] != sep)
        pos++;

    Str ret = from.substr(0, pos);
    from = from.substr(pos + 1);
    return ret;
}

bool has_prefixi(const char *str, const char *prefix)
{
    int pos = 0;
    while (prefix[pos] && tolower(str[pos]) == tolower(prefix[pos]))
        pos++;
    return prefix[pos] == 0;
}

bool has_suffixi(const char *str, const char *suffix)
{
    int len = strlen(str);
    int lensuf = strlen(suffix);
    if (lensuf > len)
        return false;

    int offs = len - lensuf;
    for (int i=0; suffix[i]; i++)
        if (tolower(str[offs+i]) != tolower(suffix[i]))
            return false;

    return true;
}
