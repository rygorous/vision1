#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_DEPRECATE
#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

U8 vga_screen[WIDTH * HEIGHT];
PalEntry vga_pal[256];

// ---- utils

static HWND hWnd = 0;

void errorExit(const char *fmt, ...)
{
	char buffer[2048];
	va_list arg;

	va_start(arg, fmt);
	vsprintf(buffer, fmt, arg);
	va_end(arg);

	MessageBox(hWnd, buffer, "imgresize", MB_ICONERROR|MB_OK);
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
		errorExit("RegisterClass failed!\n");

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r = { 0, 0, WIDTH*2, HEIGHT * 2 };
    AdjustWindowRect(&r, style, FALSE);

	hWnd = CreateWindow("ryg.vision1", "vision1", style, CW_USEDEFAULT, CW_USEDEFAULT,
		r.right - r.left, r.bottom - r.top, NULL, NULL, hInstance, NULL);
	if (!hWnd)
		errorExit("CreateWindow failed!\n");
}

static void init()
{
#if 1
    display_pic("grafix/back01.pic");
    display_face("chars/elo/face.frz");
#endif

    //display_blk("grafix/zsatz.blk");
    // figure out where "font metrics" are stored?

    // uhr.blk is special (delta coded!)
    //display_blk("grafix/wasser0.blk");

    //display_pic("grafix/dock_l.pic");
    //display_hot("grafix/dock_l.hot");

    //display_pic("grafix/erlebhot.pic");
    display_pic("grafix/infolift.pi");

    //display_raw_pic("grafix/int_wolk.pic");
    //display_gra("ani/intro.gra");
    //display_gra("grafix/roomset.gra");
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
        static const char *bkgnd = "grafix/vision4.pi";
        static const char *grafile = "ani/vision4.gra";

        if (!display_gra(grafile, frame)) {
            frame = 0;
            display_pic(bkgnd);
            display_gra(grafile, frame);
        }

        frame++;
    }
#endif
}

static void render()
{
}

int main(int argc, char **argv)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    createWindow(hInstance);

	ShowWindow(hWnd, SW_SHOW);

    init();

    for (;;) {
        MSG msg;

        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            update();
            render();

            HDC hdc = GetDC(hWnd);
            paint(hWnd, hdc);
            ReleaseDC(hWnd, hdc);
        }
    }
}