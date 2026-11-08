#include "workbench_chrome.hpp"
#include "workbench/scrollbar.hpp"
#include "workbench/workbench_graphics.hpp"

#include <commctrl.h>
#include <windowsx.h>

#include <memory>
#include <string>

namespace {
struct ScrollState {
    std::function<void(int, int)> scroll;
    bool dragging = false;
    int grab = 0;
};

workbench::Scrollbar geometry(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&client), 2);
    RECT bounds{};
    GetWindowRect(window, &bounds);
    OffsetRect(&client, -bounds.left, -bounds.top);
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_ALL;
    GetScrollInfo(window, SB_VERT, &info);
    return workbench::verticalScrollbar(
        {client.right, client.top,
         client.right + GetSystemMetricsForDpi(SM_CXVSCROLL, GetDpiForWindow(window)),
         client.bottom},
        info.nMax - info.nMin + 1, static_cast<int>(info.nPage), info.nPos);
}

void paintScroll(HWND window, const ScrollState& state) {
    HDC dc = GetWindowDC(window);
    if (!dc) {
        return;
    }
    const auto bar = geometry(window);
    workbench::fill(dc, bar.track, RGB(105, 105, 105));
    workbench::bevel(dc, bar.track, true);
    workbench::fill(dc, bar.thumb, RGB(170, 170, 170));
    workbench::bevel(dc, bar.thumb, state.dragging);
    for (const auto button : {bar.up, bar.down}) {
        workbench::fill(dc, button, RGB(170, 170, 170));
        workbench::bevel(dc, button);
        const int x = (button.left + button.right) / 2;
        const int y = (button.top + button.bottom) / 2;
        for (int row = 0; row < 4; ++row) {
            const int line = button.top == bar.up.top ? y - 2 + row : y + 2 - row;
            workbench::fill(dc, {x - row, line, x + row + 1, line + 1},
                            bar.maximum ? RGB(0, 0, 0) : RGB(110, 110, 110));
        }
    }
    ReleaseDC(window, dc);
}

POINT cursor(HWND window) {
    POINT point{};
    GetCursorPos(&point);
    RECT rect{};
    GetWindowRect(window, &rect);
    return {point.x - rect.left, point.y - rect.top};
}

LRESULT CALLBACK scrollProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                            DWORD_PTR data) {
    auto& state = *reinterpret_cast<ScrollState*>(data);
    if (message == WM_NCLBUTTONDOWN && wParam == HTVSCROLL) {
        const auto bar = geometry(window);
        const auto point = cursor(window);
        if (bar.maximum && PtInRect(&bar.thumb, point)) {
            state.dragging = true;
            state.grab = point.y - bar.thumb.top;
            SetCapture(window);
        } else if (PtInRect(&bar.up, point)) {
            state.scroll(SB_LINEUP, 0);
        } else if (PtInRect(&bar.down, point)) {
            state.scroll(SB_LINEDOWN, 0);
        } else {
            state.scroll(point.y < bar.thumb.top ? SB_PAGEUP : SB_PAGEDOWN, 0);
        }
        paintScroll(window, state);
        return 0;
    }
    if (message == WM_NCHITTEST) {
        RECT rect{};
        GetWindowRect(window, &rect);
        POINT point{GET_X_LPARAM(lParam) - rect.left, GET_Y_LPARAM(lParam) - rect.top};
        const auto bar = geometry(window);
        RECT area{bar.track.left, bar.track.top, bar.track.right, bar.down.bottom};
        if (PtInRect(&area, point)) {
            return HTVSCROLL;
        }
    }
    if (message == WM_MOUSEMOVE && state.dragging) {
        state.scroll(SB_THUMBTRACK,
                     workbench::rowFromThumb(geometry(window), cursor(window).y, state.grab));
        paintScroll(window, state);
        return 0;
    }
    if ((message == WM_LBUTTONUP || message == WM_CANCELMODE) && GetCapture() == window) {
        ReleaseCapture();
        return 0;
    }
    if (message == WM_CAPTURECHANGED) {
        state.dragging = false;
        paintScroll(window, state);
    }
    if (message == WM_NCPAINT || message == WM_PAINT || message == WM_SIZE ||
        message == WM_MOUSEWHEEL || message == WM_VSCROLL || message == WM_NCMOUSEMOVE) {
        const auto result = DefSubclassProc(window, message, wParam, lParam);
        paintScroll(window, state);
        return result;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, scrollProc, id);
        delete &state;
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

LRESULT CALLBACK statusProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                            DWORD_PTR) {
    if (message == WM_PAINT) {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(window, &ps);
        RECT bounds{};
        GetClientRect(window, &bounds);
        workbench::fill(dc, bounds, RGB(170, 170, 170));
        HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                                 FIXED_PITCH, L"Consolas");
        auto old = SelectObject(dc, font);
        const int count = static_cast<int>(SendMessageW(window, SB_GETPARTS, 0, 0));
        for (int i = 0; i < count; ++i) {
            RECT rect{};
            SendMessageW(window, SB_GETRECT, i, reinterpret_cast<LPARAM>(&rect));
            workbench::bevel(dc, rect, true);
            const size_t length = LOWORD(SendMessageW(window, SB_GETTEXTLENGTHW, i, 0));
            std::wstring text(length + 1, L'\0');
            SendMessageW(window, SB_GETTEXTW, i, reinterpret_cast<LPARAM>(text.data()));
            rect.left += 6;
            workbench::label(dc, rect, text, RGB(0, 0, 0),
                             DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        SelectObject(dc, old);
        DeleteObject(font);
        EndPaint(window, &ps);
        return 0;
    }
    if (message == WM_ERASEBKGND) {
        return 1;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, statusProc, id);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}
} // namespace

bool installWorkbenchDocumentScrollbar(HWND window, std::function<void(int, int)> scroll) {
    auto state = std::make_unique<ScrollState>();
    state->scroll = std::move(scroll);
    if (!SetWindowSubclass(window, scrollProc, 4, reinterpret_cast<DWORD_PTR>(state.get()))) {
        return false;
    }
    state.release();
    return true;
}

bool installWorkbenchStatus(HWND status) {
    return SetWindowSubclass(status, statusProc, 5, 0) != FALSE;
}
