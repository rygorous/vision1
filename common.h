#ifndef __COMMON_H__
#define __COMMON_H__

// types
typedef unsigned char   U8;
typedef unsigned short  U16;
typedef unsigned int    U32;
typedef signed char     S8;
typedef signed short    S16;
typedef signed int      S32;

// main
static const int WIDTH = 320;
static const int HEIGHT = 200;

struct PalEntry {
    U8 r, g, b;
};

extern U8 vga_screen[WIDTH * HEIGHT];
extern PalEntry vga_pal[256];

void errorExit(const char *fmt, ...);

// util
U8 *read_file(const char *filename, int *size=0);
void read_file_to(void *buf, const char *filename);

void display_raw_pic(const char *filename); // .hot
void display_pic(const char *filename); // .pic or .pi
void display_hot(const char *filename); // .hot
void display_face(const char *filename); // .frz
bool display_gra(const char *filename, int index);
void display_blk(const char *filename);

#endif