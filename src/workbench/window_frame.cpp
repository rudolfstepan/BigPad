#include "window_frame.hpp"

#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <memory>

namespace retro_frame {
namespace {
constexpr UINT_PTR subclassId = 0x52455452;
constexpr const wchar_t* stateProperty = L"RetroFrame.State";
struct State {
    Colors colors;
    UINT pressed = 0;
};

struct Layout {
    RECT window;
    RECT title;
    RECT close;
    RECT minimize;
    RECT maximize;
};

Layout layout(HWND window) {
    RECT bounds{};
    GetWindowRect(window, &bounds);
    const UINT dpi = GetDpiForWindow(window);
    const int border = MulDiv(4, dpi, 96);
    const int height = MulDiv(28, dpi, 96);
    const LONG width = bounds.right - bounds.left;
    const int buttonWidth = MulDiv(34, dpi, 96);
    Layout result{};
    result.window = {0, 0, width, bounds.bottom - bounds.top};
    result.title = {border, border, width - border, border + height};
    result.close = {border, border, border + buttonWidth, border + height};
    result.maximize = {width - border - buttonWidth, border, width - border, border + height};
    result.minimize = {result.maximize.left - buttonWidth, border, result.maximize.left,
                       border + height};
    return result;
}

bool hasCaption(HWND window) {
    return (GetWindowLongPtrW(window, GWL_STYLE) & WS_CAPTION) == WS_CAPTION;
}

UINT buttonAt(HWND window, POINT point) {
    const auto frame = layout(window);
    const auto style = GetWindowLongPtrW(window, GWL_STYLE);
    if (PtInRect(&frame.close, point) && (style & WS_SYSMENU)) {
        return SC_CLOSE;
    }
    if (PtInRect(&frame.minimize, point) && (style & WS_MINIMIZEBOX)) {
        return SC_MINIMIZE;
    }
    if (PtInRect(&frame.maximize, point) && (style & WS_MAXIMIZEBOX)) {
        return IsZoomed(window) ? SC_RESTORE : SC_MAXIMIZE;
    }
    return 0;
}

POINT windowPoint(HWND window, POINT screen) {
    RECT bounds{};
    GetWindowRect(window, &bounds);
    return {screen.x - bounds.left, screen.y - bounds.top};
}

void fill(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void paint(HWND window, const State& state) {
    if (!hasCaption(window) || IsIconic(window)) {
        return;
    }
    HDC dc = GetWindowDC(window);
    if (!dc) {
        return;
    }
    const auto frame = layout(window);
    const auto& colors = state.colors;
    RECT client{};
    GetClientRect(window, &client);
    MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&client), 2);
    RECT bounds{};
    GetWindowRect(window, &bounds);
    OffsetRect(&client, -bounds.left, -bounds.top);
    // The complete non-client surface is ours; no themed Windows caption is painted.
    fill(dc, {0, 0, frame.window.right, frame.title.bottom}, colors.border);
    fill(dc, {0, frame.title.bottom, client.left, frame.window.bottom}, colors.border);
    fill(dc, {client.right, frame.title.bottom, frame.window.right, frame.window.bottom},
         colors.border);
    fill(dc, {0, client.bottom, frame.window.right, frame.window.bottom}, colors.border);
    fill(dc, {0, 0, frame.window.right, 1}, RGB(255, 255, 255));
    fill(dc, {0, 0, 1, frame.window.bottom}, RGB(255, 255, 255));
    fill(dc, {frame.window.right - 2, 0, frame.window.right, frame.window.bottom}, RGB(25, 25, 25));
    fill(dc, {0, frame.window.bottom - 2, frame.window.right, frame.window.bottom},
         RGB(25, 25, 25));
    fill(dc, frame.title, GetForegroundWindow() == window ? colors.title : colors.inactive);
    const UINT dpi = GetDpiForWindow(window);
    HFONT font = CreateFontW(-MulDiv(16, dpi, 96), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             NONANTIALIASED_QUALITY, FIXED_PITCH, L"Consolas");
    HGDIOBJ previousFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, colors.text);
    wchar_t title[1024]{};
    GetWindowTextW(window, title, 1024);
    RECT textRect = frame.title;
    textRect.left = frame.close.right + MulDiv(10, dpi, 96);
    textRect.right = frame.minimize.left - MulDiv(8, dpi, 96);
    DrawTextW(dc, title, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    const auto drawButton = [&](RECT rect, UINT command) {
        const auto style = GetWindowLongPtrW(window, GWL_STYLE);
        if ((command == SC_MINIMIZE && !(style & WS_MINIMIZEBOX)) ||
            (command == SC_MAXIMIZE && !(style & WS_MAXIMIZEBOX))) {
            return;
        }
        const bool pressed =
            state.pressed == command || (command == SC_MAXIMIZE && state.pressed == SC_RESTORE);
        fill(dc, rect, pressed ? RGB(20, 20, 20) : RGB(255, 255, 255));
        ++rect.left;
        ++rect.top;
        fill(dc, rect, pressed ? RGB(255, 255, 255) : RGB(20, 20, 20));
        --rect.right;
        --rect.bottom;
        fill(dc, rect, colors.border);
        InflateRect(&rect, -2, -2);
        const int x = (rect.left + rect.right) / 2;
        const int y = (rect.top + rect.bottom) / 2;
        const int unit = std::max(1, MulDiv(2, dpi, 96));
        if (command == SC_CLOSE) {
            fill(dc, {x - 3 * unit, y - 3 * unit, x + 3 * unit, y + 3 * unit}, RGB(20, 20, 20));
        } else if (command == SC_MINIMIZE) {
            fill(dc, {x - 4 * unit, y + unit, x + 4 * unit, y + 3 * unit}, RGB(20, 20, 20));
        } else {
            RECT box{x - 4 * unit, y - 3 * unit, x + 4 * unit, y + 3 * unit};
            fill(dc, box, RGB(20, 20, 20));
            InflateRect(&box, -unit, -unit);
            fill(dc, box, colors.border);
            fill(dc, {x, y, x + 5 * unit, y + 4 * unit}, RGB(20, 20, 20));
        }
    };
    drawButton(frame.close, SC_CLOSE);
    drawButton(frame.minimize, SC_MINIMIZE);
    drawButton(frame.maximize, SC_MAXIMIZE);
    SelectObject(dc, previousFont);
    DeleteObject(font);
    ReleaseDC(window, dc);
}

LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR,
                           DWORD_PTR data) {
    auto& state = *reinterpret_cast<State*>(data);
    switch (message) {
        case WM_NCCALCSIZE:
            if (hasCaption(window)) {
                auto* rect = wParam ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam)->rgrc[0]
                                    : reinterpret_cast<RECT*>(lParam);
                const int border = MulDiv(4, GetDpiForWindow(window), 96);
                const int caption = MulDiv(28, GetDpiForWindow(window), 96);
                rect->left += border;
                rect->right -= border;
                rect->top += border + caption;
                if (GetMenu(window)) {
                    rect->top += MulDiv(28, GetDpiForWindow(window), 96);
                }
                if (GetWindowLongPtrW(window, GWL_STYLE) & WS_VSCROLL) {
                    rect->right -= GetSystemMetricsForDpi(SM_CXVSCROLL, GetDpiForWindow(window));
                }
                rect->bottom -= border;
                return 0;
            }
            break;
        case WM_NCPAINT:
        case WM_NCACTIVATE:
            if (hasCaption(window)) {
                paint(window, state);
                return TRUE;
            }
            break;
        case WM_NCHITTEST:
            if (hasCaption(window)) {
                const POINT point =
                    windowPoint(window, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
                if (buttonAt(window, point)) {
                    return HTBORDER;
                }
                const auto frame = layout(window);
                const int border = frame.title.left;
                if (!IsZoomed(window) && (GetWindowLongPtrW(window, GWL_STYLE) & WS_THICKFRAME)) {
                    const bool left = point.x < border;
                    const bool right = point.x >= frame.window.right - border;
                    const bool top = point.y < border;
                    const bool bottom = point.y >= frame.window.bottom - border;
                    if (top) {
                        return left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP;
                    }
                    if (bottom) {
                        return left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM;
                    }
                    if (left || right) {
                        return left ? HTLEFT : HTRIGHT;
                    }
                }
                if (PtInRect(&frame.title, point)) {
                    return HTCAPTION;
                }
                return HTCLIENT;
            }
            break;
        case WM_NCLBUTTONDOWN:
            if (hasCaption(window)) {
                const auto command = buttonAt(
                    window, windowPoint(window, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}));
                if (command) {
                    SetForegroundWindow(window);
                    state.pressed = command;
                    SetCapture(window);
                    paint(window, state);
                    return 0;
                }
            }
            break;
        case WM_LBUTTONUP:
            if (state.pressed) {
                const UINT command = state.pressed;
                state.pressed = 0;
                POINT cursor{};
                GetCursorPos(&cursor);
                ReleaseCapture();
                paint(window, state);
                if (buttonAt(window, windowPoint(window, cursor)) == command) {
                    PostMessageW(window, WM_SYSCOMMAND, command, 0);
                }
                return 0;
            }
            break;
        case WM_CANCELMODE:
            if (GetCapture() == window) {
                ReleaseCapture();
            }
            break;
        case WM_CAPTURECHANGED:
            state.pressed = 0;
            paint(window, state);
            break;
        case WM_SETTEXT:
        case WM_SIZE:
        case WM_DPICHANGED: {
            const auto result = DefSubclassProc(window, message, wParam, lParam);
            paint(window, state);
            return result;
        }
        case WM_GETMINMAXINFO: {
            const auto result = DefSubclassProc(window, message, wParam, lParam);
            MONITORINFO monitor{};
            monitor.cbSize = sizeof(monitor);
            if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
                auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
                limits->ptMaxPosition = {monitor.rcWork.left - monitor.rcMonitor.left,
                                         monitor.rcWork.top - monitor.rcMonitor.top};
                limits->ptMaxSize = {monitor.rcWork.right - monitor.rcWork.left,
                                     monitor.rcWork.bottom - monitor.rcWork.top};
            }
            return result;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(window, procedure, subclassId);
            RemovePropW(window, stateProperty);
            delete &state;
            return DefSubclassProc(window, message, wParam, lParam);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}
} // namespace

bool install(HWND window, Colors colors) {
    auto* existing = reinterpret_cast<State*>(GetPropW(window, stateProperty));
    if (existing) {
        reinterpret_cast<State*>(existing)->colors = colors;
        paint(window, *reinterpret_cast<State*>(existing));
        return true;
    }
    auto state = std::make_unique<State>();
    state->colors = colors;
    if (!SetPropW(window, stateProperty, state.get())) {
        return false;
    }
    if (!SetWindowSubclass(window, procedure, subclassId,
                           reinterpret_cast<DWORD_PTR>(state.get()))) {
        RemovePropW(window, stateProperty);
        return false;
    }
    state.release();
    // Turn off compositor caption rendering; Windows still manages move/resize and system commands.
    const DWMNCRENDERINGPOLICY policy = DWMNCRP_DISABLED;
    DwmSetWindowAttribute(window, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
    SetWindowPos(window, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    return true;
}
} // namespace retro_frame
