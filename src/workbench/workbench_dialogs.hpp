#pragma once
#include "window_frame.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace workbench {
struct DialogTheme {
    retro_frame::Colors frame{RGB(0, 65, 175), RGB(255, 255, 255), RGB(170, 170, 170),
                              RGB(100, 100, 100)};
    COLORREF background = RGB(170, 170, 170);
    COLORREF text = RGB(20, 20, 20);
    COLORREF selection = RGB(0, 65, 175);
};
void setDialogTheme(DialogTheme theme);
void setDialogLanguage(bool german);
int message(HWND owner, const wchar_t* text, const wchar_t* title, UINT flags = MB_OK);
int menu(HWND owner, const wchar_t* title,
         const std::vector<std::pair<int, std::wstring>>& actions);
enum class Request { OpenFile, SaveFile, Folder };
std::optional<std::filesystem::path> requestPath(HWND owner, const wchar_t* title, Request request,
                                                 const std::filesystem::path& initial = {});
std::optional<COLORREF> requestColor(HWND owner, COLORREF initial);
} // namespace workbench
