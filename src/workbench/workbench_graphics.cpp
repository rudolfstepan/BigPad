#include "workbench_graphics.hpp"

namespace workbench {
void fill(HDC dc, RECT rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void bevel(HDC dc, RECT rect, bool pressed) {
    fill(dc, {rect.left, rect.top, rect.right, rect.top + 2},
         pressed ? RGB(25, 25, 25) : RGB(255, 255, 255));
    fill(dc, {rect.left, rect.top, rect.left + 2, rect.bottom},
         pressed ? RGB(25, 25, 25) : RGB(255, 255, 255));
    fill(dc, {rect.left, rect.bottom - 2, rect.right, rect.bottom},
         pressed ? RGB(255, 255, 255) : RGB(25, 25, 25));
    fill(dc, {rect.right - 2, rect.top, rect.right, rect.bottom},
         pressed ? RGB(255, 255, 255) : RGB(25, 25, 25));
}

void label(HDC dc, RECT rect, const std::wstring& value, COLORREF color, UINT flags) {
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, value.c_str(), -1, &rect, flags | DT_NOPREFIX);
}

void icon(HDC dc, RECT tile, Symbol symbol, const std::wstring& name, bool selected,
          COLORREF selection) {
    const int x = (tile.left + tile.right) / 2 - 30;
    const int y = tile.top + 8;
    const COLORREF ink = RGB(25, 25, 30);
    const COLORREF paper = selected ? selection : RGB(210, 210, 210);
    if (symbol == Symbol::Drawer || symbol == Symbol::Preferences) {
        fill(dc, {x, y + 12, x + 62, y + 50}, ink);
        fill(dc, {x + 2, y + 14, x + 60, y + 48}, paper);
        bevel(dc, {x + 2, y + 14, x + 60, y + 48});
        fill(dc, {x + 5, y + 1, x + 27, y + 14}, ink);
        fill(dc, {x + 7, y + 3, x + 25, y + 14}, paper);
        fill(dc, {x + 21, y + 26, x + 41, y + 34}, ink);
        fill(dc, {x + 23, y + 28, x + 39, y + 32}, RGB(255, 255, 255));
        if (symbol == Symbol::Preferences) {
            fill(dc, {x + 43, y + 37, x + 60, y + 53}, RGB(0, 65, 175));
            fill(dc, {x + 48, y + 32, x + 54, y + 58}, ink);
        }
    } else if (symbol == Symbol::Disk) {
        fill(dc, {x, y, x + 62, y + 53}, ink);
        fill(dc, {x + 2, y + 2, x + 60, y + 51}, paper);
        bevel(dc, {x + 2, y + 2, x + 60, y + 51});
        fill(dc, {x + 12, y + 4, x + 50, y + 22}, ink);
        fill(dc, {x + 15, y + 5, x + 47, y + 20}, RGB(155, 160, 170));
        fill(dc, {x + 36, y + 7, x + 43, y + 18}, ink);
        fill(dc, {x + 10, y + 30, x + 52, y + 49}, RGB(245, 245, 235));
        fill(dc, {x + 15, y + 35, x + 47, y + 37}, RGB(0, 65, 175));
        fill(dc, {x + 15, y + 41, x + 40, y + 43}, ink);
    } else {
        fill(dc, {x + 8, y, x + 53, y + 54}, ink);
        fill(dc, {x + 10, y + 2, x + 51, y + 52}, paper);
        if (symbol == Symbol::Tool) {
            fill(dc, {x + 15, y + 8, x + 46, y + 32}, RGB(0, 65, 175));
            fill(dc, {x + 20, y + 13, x + 25, y + 17}, RGB(255, 255, 255));
            fill(dc, {x + 25, y + 17, x + 30, y + 21}, RGB(255, 255, 255));
            fill(dc, {x + 20, y + 21, x + 25, y + 25}, RGB(255, 255, 255));
        } else {
            for (int line = 0; line < 5; ++line) {
                fill(dc, {x + 17, y + 10 + line * 7, x + 43, y + 12 + line * 7}, ink);
            }
        }
    }
    RECT caption{tile.left + 2, tile.top + 66, tile.right - 2, tile.top + 92};
    if (selected) {
        fill(dc, caption, RGB(0, 65, 175));
    }
    label(dc, caption, name, selected ? RGB(255, 255, 255) : ink,
          DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}
} // namespace workbench
