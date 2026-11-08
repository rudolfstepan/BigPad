#pragma once
#include "platform.hpp"

#include <string>

namespace workbench {
enum class Symbol { Disk, Drawer, Tool, Document, Preferences };
void fill(HDC dc, RECT rect, COLORREF color);
void bevel(HDC dc, RECT rect, bool pressed = false);
void label(HDC dc, RECT rect, const std::wstring& text, COLORREF color, UINT flags);
void icon(HDC dc, RECT tile, Symbol symbol, const std::wstring& name, bool selected,
          COLORREF selection);
} // namespace workbench
