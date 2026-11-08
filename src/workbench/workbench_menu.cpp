#include "workbench_menu.hpp"
#include "workbench_dialogs.hpp"
#include "workbench_graphics.hpp"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <string>
#include <vector>

namespace workbench {
namespace {
std::wstring caption(HMENU menu, int index) {
    wchar_t buffer[256]{};
    GetMenuStringW(menu, index, buffer, 256, MF_BYPOSITION);
    std::wstring value = buffer;
    value.erase(std::remove(value.begin(), value.end(), L'&'), value.end());
    return value;
}

RECT bounds(HWND window) {
    const UINT dpi = GetDpiForWindow(window);
    RECT rect{};
    GetWindowRect(window, &rect);
    const int border = MulDiv(4, dpi, 96);
    const int top = border + MulDiv(28, dpi, 96);
    return {border, top, rect.right - rect.left - border, top + MulDiv(28, dpi, 96)};
}

int choose(HWND window, HMENU menu, const std::wstring& title) {
    std::vector<std::pair<int, std::wstring>> choices;
    for (int i = 0; i < GetMenuItemCount(menu); ++i) {
        const UINT state = GetMenuState(menu, i, MF_BYPOSITION);
        if (state & (MF_SEPARATOR | MF_DISABLED | MF_GRAYED)) {
            continue;
        }
        auto text = caption(menu, i);
        if (state & MF_CHECKED) {
            text = L"[X] " + text;
        }
        choices.emplace_back(10000 + i, text);
    }
    if (choices.empty()) {
        return 0;
    }
    const int selected = workbench::menu(window, title.c_str(), choices) - 10000;
    if (selected < 0 || selected >= GetMenuItemCount(menu)) {
        return 0;
    }
    HMENU child = GetSubMenu(menu, selected);
    return child ? choose(window, child, caption(menu, selected))
                 : static_cast<int>(GetMenuItemID(menu, selected));
}

void show(HWND window, int index) {
    HMENU bar = GetMenu(window);
    const int command = index < 0 ? choose(window, bar, L"BigPad")
                                  : choose(window, GetSubMenu(bar, index), caption(bar, index));
    if (command > 0) {
        SendMessageW(window, WM_COMMAND, command, 0);
    }
}

void paint(HWND window) {
    if (!GetMenu(window)) {
        return;
    }
    HDC dc = GetWindowDC(window);
    if (!dc) {
        return;
    }
    const auto rect = bounds(window);
    fill(dc, rect, RGB(170, 170, 170));
    bevel(dc, rect);
    HFONT font = CreateFontW(-MulDiv(16, GetDpiForWindow(window), 96), 0, 0, 0, FW_NORMAL, FALSE,
                             FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             NONANTIALIASED_QUALITY, FIXED_PITCH, L"Consolas");
    auto previous = SelectObject(dc, font);
    int x = rect.left + 4;
    for (int i = 0; i < GetMenuItemCount(GetMenu(window)); ++i) {
        const auto value = caption(GetMenu(window), i);
        SIZE size{};
        GetTextExtentPoint32W(dc, value.c_str(), static_cast<int>(value.size()), &size);
        label(dc, {x, rect.top, x + size.cx + 20, rect.bottom}, value, RGB(0, 0, 0),
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        x += size.cx + 20;
    }
    SelectObject(dc, previous);
    DeleteObject(font);
    ReleaseDC(window, dc);
}

LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                           DWORD_PTR) {
    if (message == WM_NCHITTEST || message == WM_NCLBUTTONDOWN || message == WM_NCLBUTTONDBLCLK ||
        message == WM_NCMOUSEMOVE) {
        RECT windowRect{};
        GetWindowRect(window, &windowRect);
        const POINT point{GET_X_LPARAM(lParam) - windowRect.left,
                          GET_Y_LPARAM(lParam) - windowRect.top};
        const auto rect = bounds(window);
        if (GetMenu(window) && PtInRect(&rect, point)) {
            // HTMENU would let Windows track and redraw its own smaller menu bar.
            if (message == WM_NCHITTEST) {
                return HTBORDER;
            }
            if (message == WM_NCMOUSEMOVE) {
                return 0;
            }
            HDC dc = GetWindowDC(window);
            HFONT font =
                CreateFontW(-MulDiv(16, GetDpiForWindow(window), 96), 0, 0, 0, FW_NORMAL, FALSE,
                            FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            NONANTIALIASED_QUALITY, FIXED_PITCH, L"Consolas");
            auto old = SelectObject(dc, font);
            int x = rect.left + 4;
            int selected = -1;
            for (int i = 0; i < GetMenuItemCount(GetMenu(window)); ++i) {
                const auto value = caption(GetMenu(window), i);
                SIZE size{};
                GetTextExtentPoint32W(dc, value.c_str(), static_cast<int>(value.size()), &size);
                if (point.x >= x && point.x < x + size.cx + 20) {
                    selected = i;
                    break;
                }
                x += size.cx + 20;
            }
            SelectObject(dc, old);
            DeleteObject(font);
            ReleaseDC(window, dc);
            if (selected >= 0) {
                show(window, selected);
            }
            return 0;
        }
    }
    if (message == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_KEYMENU) {
        show(window, -1);
        return 0;
    }
    if (message == WM_NCPAINT || message == WM_NCACTIVATE || message == WM_SIZE ||
        message == WM_SETTEXT) {
        const auto result = DefSubclassProc(window, message, wParam, lParam);
        paint(window);
        return result;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, procedure, id);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}
} // namespace

bool installMenu(HWND window) {
    return SetWindowSubclass(window, procedure, 3, 0) != FALSE;
}
} // namespace workbench
