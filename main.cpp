#define _CRT_SECURE_NO_DEPRECATE
#include <Windows.h>
#include <crtdbg.h>
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
#include "corridor.h"

#pragma comment(lib, "winmm.lib")

// ---- utils

static HWND hWnd = 0;

void panic(const char *fmt, ...)
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

    int w = vga_screen.width();
    int h = vga_screen.height();

    BITMAPINFOHEADER bmh;
    ZeroMemory(&bmh, sizeof(bmh));
    bmh.biSize = sizeof(bmh);
    bmh.biWidth = w;
    bmh.biHeight = -h;
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
    U32 *bits = new U32[w * h];
    for (int y=0; y < h; y++) {
        U32 *dst = bits + y * w;
        const U8 *src = game_get_screen_row(y);
        for (int x=0; x < w; x++)
            dst[x] = pal[src[x]];
    }

    // render the cursor if required
    POINT ptCursor;
    if (GetCursorPos(&ptCursor) &&
        ScreenToClient(hwnd, &ptCursor) &&
        ptCursor.x >= rc.left && ptCursor.y >= rc.top &&
        ptCursor.x < rc.right && ptCursor.y < rc.bottom)
        render_mouse_cursor(bits, pal);

    StretchDIBits(hdc, 0, 0, w * 2, h * 2, 0, 0, w, h, bits, (BITMAPINFO *)&bmh, DIB_RGB_COLORS, SRCCOPY);
    delete[] bits;

    // fill rest with black
    HBRUSH black = (HBRUSH) GetStockObject(BLACK_BRUSH);

    r = rc;
    r.left = w * 2;
    r.bottom = h * 2;
    FillRect(hdc, &r, black);
    
    r = rc;
    r.top = h * 2;
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

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(0);
            return TRUE;
        }
        break;

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        mouse_x = LOWORD(lParam) / 2;
        mouse_y = HIWORD(lParam) / 2;
        if (uMsg == WM_LBUTTONDOWN)
            mouse_button |= 1;
        else if (uMsg == WM_LBUTTONUP)
            mouse_button &= ~1;
        return 0;

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
		panic("RegisterClass failed!\n");

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r = { 0, 0, vga_screen.width()*2, vga_screen.height()*2 };
    AdjustWindowRect(&r, style, FALSE);

	hWnd = CreateWindow("ryg.vision1", "vision1", style, CW_USEDEFAULT, CW_USEDEFAULT,
		r.right - r.left, r.bottom - r.top, NULL, NULL, hInstance, NULL);
	if (!hWnd)
		panic("CreateWindow failed!\n");
}

static void init()
{
    timeBeginPeriod(1);
    srand(timeGetTime());

    graphics_init();
    vars_init();
    font_init();
    mouse_init();
    corridor_init();

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

    Slice s = read_file("chars/silizian/face.frz");

    /*// use top 128 palette entries from .frz
    // but keep topmost 8 as they are (used for text)
    memcpy(&palette_a[128], &s[128*sizeof(PalEntry)], 0x78*sizeof(PalEntry));
    memcpy(&palette_b[128], &s[128*sizeof(PalEntry)], 0x78*sizeof(PalEntry));

    const U8 *faceimg = &s[256*sizeof(PalEntry)];
    decode_delta_gfx(vga_screen, 0, 24, faceimg, 2, true);
    set_palette();
    for (int i=0; i < 10000; i++)
        game_frame();*/
}

static void shutdown()
{
    game_shutdown();
    font_shutdown();
    mouse_shutdown();
    graphics_shutdown();
    corridor_shutdown();

    timeEndPeriod(1);
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

    if (!msgloop())
        throw 1;

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
#ifdef _DEBUG
    //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_CHECK_CRT_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    init();

    HINSTANCE hInstance = GetModuleHandle(NULL);
    createWindow(hInstance);

	ShowWindow(hWnd, SW_SHOW);

    //game_command("welt init");
    game_command("welt 08360900");

    try {
        for (;;) {
            if (!msgloop())
                break;

            game_script_tick();
            game_frame();
        }
    } catch (int) {
    }

    shutdown();
}