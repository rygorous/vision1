#ifndef __COMMON_H__
#define __COMMON_H__

#define _CRT_SECURE_NO_WARNINGS

#include <string>

// types
typedef unsigned char   U8;
typedef unsigned short  U16;
typedef unsigned int    U32;
typedef signed char     S8;
typedef signed short    S16;
typedef signed int      S32;

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define ARRAY_COUNT(x) (sizeof(x)/sizeof(*(x)))

// main
static const int WIDTH = 320;
static const int HEIGHT = 200;

struct PalEntry {
    U8 r, g, b;
};

typedef PalEntry Palette[256];

extern U8 vga_screen[WIDTH * HEIGHT];
extern Palette vga_pal;

void errorExit(const char *fmt, ...);
void frame();

// util
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

    U8 operator [](U32 i) const     { return ptr[i]; }
    U8 &operator [](U32 i)          { return ptr[i]; }

    operator void *() const         { return buf ? buf : NULL; }
    U32 len() const                 { return length; }
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
Slice read_xored(const char *filename);

int little_u16(const U8 *p);
void decode_rle(U8 *dst, const U8 *src);
void decode_transparent_rle(U8 *dst, const U8 *src);
void decode_delta(U8 *dst, const U8 *src);
void decode_delta_gfx(U8 *dst, int x, int y, const U8 *src, int scale, bool flipX);
void decrypt(U8 *buffer, int nbytes, int *start);

int find_gra_item(Slice grafile, const char *name, U8 *type);

void decode_level(const char *filename, int level);

// graphics
struct GfxBlock {
    GfxBlock();
    ~GfxBlock();

    void load(const char *filename);
    void resize(int w, int h);

    U8 *pixels;
    int w, h;
};

class Animation { // abstract interface
public:
    virtual ~Animation() = 0;

    virtual void tick() = 0;
    virtual void render() = 0;
    virtual bool is_done() const = 0;
};

class BigAnimation : public Animation { // .ani / .big files
    U8 *data;
    int frame_size;

    int posx, posy;
    int w, h;
    int last_frame, wait_frames;
    int cur_frame, cur_tick;
    bool reversed;

    const U8 *get_frame(int frame) const;

public:
    BigAnimation(const char *filename, bool reverse_playback);
    virtual ~BigAnimation();

    virtual void tick();
    virtual void render();
    virtual bool is_done() const;
};

extern Palette palette_a, palette_b;

void set_palette();
void set_palb_fade(int intensity);
void load_background(const char *filename);

// font
struct Font {
    Font();

    void load(const char *filename, bool isBig, const U8 *palette);
    void print(int x, int y, char *str);

private:
    void print_glyph(int x, int y, int glyph);

    GfxBlock gfx;
    const U8 *widths;
    U8 pal[16];
};

// script

bool eval_bool_expr(const Slice &expr);
void run_script(Slice code, bool init);

// vars

void init_vars();

int get_var_int(const std::string &name);
void set_var_int(const std::string &name, int value);

std::string get_var_str(const std::string &name);
void set_var_str(const std::string &name, const std::string &value);

#endif