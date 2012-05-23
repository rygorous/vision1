#ifndef __COMMON_H__
#define __COMMON_H__

#define _CRT_SECURE_NO_WARNINGS

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

// shared stuff

struct PalEntry {
    U8 r, g, b;
};

typedef PalEntry Palette[256];

static const int WIDTH = 320;
static const int HEIGHT = 200;

extern U8 vga_screen[WIDTH * HEIGHT];
extern Palette vga_pal;

void error_exit(const char *fmt, ...);

#endif