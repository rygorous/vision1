#define _CRT_SECURE_NO_DEPRECATE
#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "util.h"
#include "vars.h"
#include "graphics.h"
#include "font.h"
#include "script.h"
#include "mouse.h"

#pragma comment(lib, "winmm.lib")

U8 vga_screen[WIDTH * HEIGHT];
PalEntry vga_pal[256];

// ---- utils

static HWND hWnd = 0;

void error_exit(const char *fmt, ...)
{
	char buffer[2048];
	va_list arg;

	va_start(arg, fmt);
	vsprintf(buffer, fmt, arg);
	va_end(arg);

	MessageBox(hWnd, buffer, "vision1", MB_ICONERROR|MB_OK);
	exit(1);
}

// ---- painting

static int expand6(int x)
{
    x &= 63;
    return (x << 2) | (x >> 4);
}

static void paint(HWND hwnd, HDC hdc)
{
    RECT rc, r;
    GetClientRect(hwnd, &rc);

    BITMAPINFOHEADER bmh;
    ZeroMemory(&bmh, sizeof(bmh));
    bmh.biSize = sizeof(bmh);
    bmh.biWidth = WIDTH;
    bmh.biHeight = -HEIGHT;
    bmh.biPlanes = 1;
    bmh.biBitCount = 32;
    bmh.biCompression = BI_RGB;

    // set up palette
    U32 pal[256];
    for (int i=0; i < 256; i++) {
        U8 r = expand6(vga_pal[i].r);
        U8 g = expand6(vga_pal[i].g);
        U8 b = expand6(vga_pal[i].b);
        pal[i] = (r << 16) | (g << 8) | b;
    }

    // convert vga image to true color and draw
    U32 *bits = new U32[WIDTH*HEIGHT];
    for (int i=0; i < WIDTH*HEIGHT; i++)
        bits[i] = pal[vga_screen[i]];

    StretchDIBits(hdc, 0, 0, WIDTH * 2, HEIGHT * 2, 0, 0, WIDTH, HEIGHT, bits, (BITMAPINFO *)&bmh, DIB_RGB_COLORS, SRCCOPY);
    delete[] bits;

    // fill rest with black
    HBRUSH black = (HBRUSH) GetStockObject(BLACK_BRUSH);

    r = rc;
    r.left = WIDTH * 2;
    r.bottom = HEIGHT * 2;
    FillRect(hdc, &r, black);
    
    r = rc;
    r.top = HEIGHT * 2;
    FillRect(hdc, &r, black);
}

// ---- windows blurb

static LRESULT CALLBACK windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_ERASEBKGND:
		return 0;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
            paint(hWnd, hdc);
			EndPaint(hWnd, &ps);
		}
		return 0;

    case WM_CHAR:
        if (wParam == 27) // escape
            DestroyWindow(hWnd);
        return 0;

	case WM_SIZE:
		InvalidateRect(hWnd, NULL, FALSE);
		break;

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        mouse_x = LOWORD(lParam);
        mouse_y = HIWORD(lParam);
        if (uMsg == WM_LBUTTONDOWN)
            mouse_button |= 1;
        else if (uMsg == WM_LBUTTONUP)
            mouse_button &= ~1;
        break;

	default:
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static void createWindow(HINSTANCE hInstance)
{
	WNDCLASS wc;

	wc.style = 0;
	wc.lpfnWndProc = windowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "ryg.vision1";
	if (!RegisterClass(&wc))
		error_exit("RegisterClass failed!\n");

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r = { 0, 0, WIDTH*2, HEIGHT * 2 };
    AdjustWindowRect(&r, style, FALSE);

	hWnd = CreateWindow("ryg.vision1", "vision1", style, CW_USEDEFAULT, CW_USEDEFAULT,
		r.right - r.left, r.bottom - r.top, NULL, NULL, hInstance, NULL);
	if (!hWnd)
		error_exit("CreateWindow failed!\n");
}

static Slice current_script;

static void init()
{
    timeBeginPeriod(1);

    init_vars();
    init_font();
    init_mouse();

#if 0
    display_pic("grafix/back01.pic");
    display_face("chars/elo/face.frz");
#endif

    //fix_palette();

    //load_background("grafix/dock_l.pic");
    //load_background("grafix/dextras.mix");

    decode_level("../game/data/levels.dat", 1);
    // look directions: (GANGD)
    // 0 = outside (+y)
    // 1 = inside (-y)
    // 2 = cw (-x)
    // 3 = ccw (+x)
    // in maps:
    // 00 = accessible
    // 07 = skip this column
    // 80 = blocked

    current_script = read_xored("data/init.par");
    run_script(current_script, true);
}

static void shutdown()
{
    shutdown_font();
    shutdown_mouse();

    timeEndPeriod(1);
}

static void update()
{
#if 1
    static int last_tick;
    static int frame = 0;

    int now = GetTickCount();
    if (now - last_tick >= 100) {
        last_tick = now;

        // in grafix/:
        // wand01.gra: "set pieces" for 3d view
        // person*.gra: characters for 3d view
        // charset.gra: characters
        // gmod01.gra: decorations for 3d view 1
        // gmod02.gra: decorations for 3d view 2
        // newspics.gra: pics for infotimes

        //display_pic("grafix/braun.pic");
        /*static const char *bkgnd = "grafix/braun.pic";
        static const char *grafile = "grafix/wand01.gra";

        if (!display_gra(grafile, frame)) {
            frame = 0;
            //display_pic(bkgnd);
            display_gra(grafile, frame);
        }*/

        frame++;
    }
#endif

    run_script(current_script, false);
    game_frame();
}

static void render()
{
}

static bool msgloop()
{
    MSG msg;

    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            return false;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return true;
}

void frame()
{
    static U32 framelen = 14; // TODO make better!
    static U32 framestart = 0;

    // TODO mouse cursor handling here
    if (!msgloop())
        exit(1);

    HDC hdc = GetDC(hWnd);
    paint(hWnd, hdc);
    ReleaseDC(hWnd, hdc);

    for (;;) {
        U32 now = timeGetTime();
        if (now - framestart < framelen)
            Sleep(1);
        else {
            framestart = now;
            break;
        }
    }
}

int main(int argc, char **argv)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    createWindow(hInstance);

	ShowWindow(hWnd, SW_SHOW);

    init();

    for (;;) {
        if (!msgloop())
            break;
        else {
            update();
            render();
            frame();
        }
    }

    shutdown();
}