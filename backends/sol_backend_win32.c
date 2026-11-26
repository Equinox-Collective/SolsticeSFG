#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdlib.h>
#include <string.h>

#include "sol_backend.h"

#define SOL__WIN32_CLASS_NAME "SolsticeSFGWindowClass"
#define SOL__WIN32_EV_CAP 256

struct sol_window {
    HWND hwnd;
    HDC mem_dc;
    HBITMAP dib;
    HBITMAP old_bmp;
    uint32_t *fb_pixels;
    sol_surface_t surface;

    sol_event_t queue[SOL__WIN32_EV_CAP];
    uint32_t q_head;
    uint32_t q_tail;
    bool destroyed;
};

static void sol__win32_push_event(sol_window_t *win, const sol_event_t *ev) {
    uint32_t next_tail = (win->q_tail + 1u) % SOL__WIN32_EV_CAP;
    if (next_tail == win->q_head) {
        return; /* Drop oldest overflow cleanly */
    }
    win->queue[win->q_tail] = *ev;
    win->q_tail = next_tail;
}

static void sol__win32_free_dib(sol_window_t *win) {
    if (win->mem_dc != NULL && win->old_bmp != NULL) {
        SelectObject(win->mem_dc, win->old_bmp);
        win->old_bmp = NULL;
    }
    if (win->dib != NULL) {
        DeleteObject(win->dib);
        win->dib = NULL;
    }
    if (win->mem_dc != NULL) {
        DeleteDC(win->mem_dc);
        win->mem_dc = NULL;
    }
    win->fb_pixels = NULL;
}

static bool sol__win32_resize_dib(sol_window_t *win, int32_t width, int32_t height) {
    BITMAPINFO bmi;
    HDC screen_dc;
    HDC new_dc;
    HBITMAP new_dib;
    void *raw_bits = NULL;

    if (width <= 0 || height <= 0) {
        return false;
    }

    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)width;
    bmi.bmiHeader.biHeight = -(LONG)height; /* Negative height = top-down framebuffer */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    screen_dc = GetDC(win->hwnd);
    if (screen_dc == NULL) {
        return false;
    }

    new_dc = CreateCompatibleDC(screen_dc);
    new_dib = CreateDIBSection(screen_dc, &bmi, DIB_RGB_COLORS, &raw_bits, NULL, 0);
    ReleaseDC(win->hwnd, screen_dc);

    if (new_dc == NULL || new_dib == NULL || raw_bits == NULL) {
        if (new_dib != NULL) {
            DeleteObject(new_dib);
        }
        if (new_dc != NULL) {
            DeleteDC(new_dc);
        }
        return false;
    }

    sol__win32_free_dib(win);
    win->mem_dc = new_dc;
    win->dib = new_dib;
    win->old_bmp = (HBITMAP)SelectObject(win->mem_dc, win->dib);
    win->fb_pixels = (uint32_t *)raw_bits;

    memset(win->fb_pixels, 0, (size_t)width * (size_t)height * sizeof(uint32_t));
    sol_surface_init(&win->surface, win->fb_pixels, width, height, width);
    return true;
}

static LRESULT CALLBACK sol__win32_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    sol_window_t *win = (sol_window_t *)(uintptr_t)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    if (win == NULL) {
        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }

    switch (msg) {
    case WM_CLOSE: {
        sol_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = SOL_EVENT_QUIT;
        sol__win32_push_event(win, &ev);
        return 0;
    }

    case WM_DESTROY:
        win->destroyed = true;
        return 0;

    case WM_SIZE: {
        int32_t w = (int32_t)LOWORD(lparam);
        int32_t h = (int32_t)HIWORD(lparam);
        if (w > 0 && h > 0 && (w != win->surface.width || h != win->surface.height)) {
            if (sol__win32_resize_dib(win, w, h)) {
                sol_event_t ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = SOL_EVENT_RESIZE;
                ev.width = w;
                ev.height = h;
                sol__win32_push_event(win, &ev);
            }
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (win->mem_dc != NULL) {
            BitBlt(hdc, 0, 0, win->surface.width, win->surface.height, win->mem_dc, 0, 0, SRCCOPY);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE: {
        sol_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = SOL_EVENT_MOUSE_MOVE;
        ev.x = (int32_t)(int16_t)LOWORD(lparam);
        ev.y = (int32_t)(int16_t)HIWORD(lparam);
        sol__win32_push_event(win, &ev);
        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        sol_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = SOL_EVENT_MOUSE_DOWN;
        ev.x = (int32_t)(int16_t)LOWORD(lparam);
        ev.y = (int32_t)(int16_t)HIWORD(lparam);
        ev.button = (msg == WM_LBUTTONDOWN)
                        ? SOL_MOUSE_LEFT
                        : ((msg == WM_RBUTTONDOWN) ? SOL_MOUSE_RIGHT : SOL_MOUSE_MIDDLE);
        SetCapture(hwnd);
        sol__win32_push_event(win, &ev);
        return 0;
    }

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        sol_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = SOL_EVENT_MOUSE_UP;
        ev.x = (int32_t)(int16_t)LOWORD(lparam);
        ev.y = (int32_t)(int16_t)HIWORD(lparam);
        ev.button = (msg == WM_LBUTTONUP)
                        ? SOL_MOUSE_LEFT
                        : ((msg == WM_RBUTTONUP) ? SOL_MOUSE_RIGHT : SOL_MOUSE_MIDDLE);
        ReleaseCapture();
        sol__win32_push_event(win, &ev);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        POINT pt;
        sol_event_t ev;
        memset(&ev, 0, sizeof(ev));
        pt.x = (LONG)(int16_t)LOWORD(lparam);
        pt.y = (LONG)(int16_t)HIWORD(lparam);
        ScreenToClient(hwnd, &pt);
        ev.type = SOL_EVENT_MOUSE_WHEEL;
        ev.x = (int32_t)pt.x;
        ev.y = (int32_t)pt.y;
        ev.wheel_delta = (int32_t)(int16_t)HIWORD(wparam) / 120;
        sol__win32_push_event(win, &ev);
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        sol_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = SOL_EVENT_KEY_DOWN;
        ev.keycode = (uint32_t)wparam;
        sol__win32_push_event(win, &ev);
        return 0;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP: {
        sol_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = SOL_EVENT_KEY_UP;
        ev.keycode = (uint32_t)wparam;
        sol__win32_push_event(win, &ev);
        return 0;
    }

    default:
        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
}

sol_window_t *sol_window_create(const char *title, int32_t width, int32_t height) {
    static bool s_class_registered = false;
    HINSTANCE hinst = GetModuleHandleA(NULL);
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT wr;
    sol_window_t *win;

    if (width <= 0 || height <= 0) {
        return NULL;
    }

    if (!s_class_registered) {
        WNDCLASSA wc;
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = sol__win32_proc;
        wc.hInstance = hinst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.lpszClassName = SOL__WIN32_CLASS_NAME;
        if (!RegisterClassA(&wc)) {
            return NULL;
        }
        s_class_registered = true;
    }

    win = (sol_window_t *)calloc(1u, sizeof(sol_window_t));
    if (win == NULL) {
        return NULL;
    }

    wr.left = 0;
    wr.top = 0;
    wr.right = (LONG)width;
    wr.bottom = (LONG)height;
    AdjustWindowRectEx(&wr, style, FALSE, 0);

    win->hwnd = CreateWindowExA(0, SOL__WIN32_CLASS_NAME, (title != NULL) ? title : "SolsticeSFG",
                                style, CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left,
                                wr.bottom - wr.top, NULL, NULL, hinst, NULL);
    if (win->hwnd == NULL) {
        free(win);
        return NULL;
    }

    SetWindowLongPtrA(win->hwnd, GWLP_USERDATA, (LONG_PTR)(uintptr_t)win);
    if (!sol__win32_resize_dib(win, width, height)) {
        DestroyWindow(win->hwnd);
        free(win);
        return NULL;
    }

    ShowWindow(win->hwnd, SW_SHOW);
    UpdateWindow(win->hwnd);
    return win;
}

void sol_window_destroy(sol_window_t *win) {
    if (win == NULL) {
        return;
    }
    sol__win32_free_dib(win);
    if (win->hwnd != NULL && !win->destroyed) {
        SetWindowLongPtrA(win->hwnd, GWLP_USERDATA, 0);
        DestroyWindow(win->hwnd);
    }
    free(win);
}

void sol_window_set_title(sol_window_t *win, const char *title) {
    if (win != NULL && win->hwnd != NULL && title != NULL) {
        SetWindowTextA(win->hwnd, title);
    }
}

bool sol_window_poll_event(sol_window_t *win, sol_event_t *out_ev) {
    MSG msg;

    if (win == NULL || out_ev == NULL) {
        return false;
    }

    if (win->q_head == win->q_tail) {
        while (PeekMessageA(&msg, NULL, 0u, 0u, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    if (win->q_head == win->q_tail) {
        return false;
    }

    *out_ev = win->queue[win->q_head];
    win->q_head = (win->q_head + 1u) % SOL__WIN32_EV_CAP;
    return true;
}

sol_surface_t *sol_window_surface(sol_window_t *win) {
    if (win == NULL) {
        return NULL;
    }
    return &win->surface;
}

void sol_window_present(sol_window_t *win) {
    HDC hdc;
    if (win == NULL || win->hwnd == NULL || win->mem_dc == NULL) {
        return;
    }
    hdc = GetDC(win->hwnd);
    if (hdc != NULL) {
        BitBlt(hdc, 0, 0, win->surface.width, win->surface.height, win->mem_dc, 0, 0, SRCCOPY);
        ReleaseDC(win->hwnd, hdc);
    }
}

void sol_window_present_rect(sol_window_t *win, sol_rect_t dirty) {
    HDC hdc;
    sol_rect_t r;

    if (win == NULL || win->hwnd == NULL || win->mem_dc == NULL) {
        return;
    }

    r = sol_rect_intersect(SOL_RECT(0, 0, win->surface.width, win->surface.height), dirty);
    if (r.w <= 0 || r.h <= 0) {
        return;
    }

    hdc = GetDC(win->hwnd);
    if (hdc != NULL) {
        BitBlt(hdc, r.x, r.y, r.w, r.h, win->mem_dc, r.x, r.y, SRCCOPY);
        ReleaseDC(win->hwnd, hdc);
    }
}

uint64_t sol_time_ms(void) {
    static LARGE_INTEGER s_freq;
    static bool s_init = false;
    LARGE_INTEGER now;

    if (!s_init) {
        QueryPerformanceFrequency(&s_freq);
        s_init = true;
    }
    QueryPerformanceCounter(&now);
    if (s_freq.QuadPart == 0) {
        return 0u;
    }
    return (uint64_t)((now.QuadPart * 1000LL) / s_freq.QuadPart);
}

void sol_sleep_ms(uint32_t ms) {
    Sleep((DWORD)ms);
}
