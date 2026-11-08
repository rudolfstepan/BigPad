#include "workbench_dialogs.hpp"
#include "scrollbar.hpp"
#include "workbench_graphics.hpp"

#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <cwchar>
#include <memory>

namespace workbench {
namespace {
namespace fs = std::filesystem;
DialogTheme theme;
bool german = true;
const wchar_t* tr(const wchar_t* de, const wchar_t* en) {
    return german ? de : en;
}
enum class Mode { Text, Path, Color, Menu };
struct Dialog {
    Mode mode = Mode::Text;
    DialogTheme theme;
    std::wstring content;
    std::wstring error;
    std::vector<std::pair<int, std::wstring>> actions;
    Request request = Request::OpenFile;
    fs::path directory;
    fs::path chosenPath;
    std::wstring defaultExtension;
    COLORREF chosenColor = 0;
    std::vector<fs::path> entries;
    std::vector<bool> directories;
    HWND edit = nullptr;
    HFONT font = nullptr;
    HBRUSH brush = nullptr;
    int position = 0;
    int extent = 0;
    int result = IDCANCEL;
    int focus = 0;
    bool dragging = false;
    int grab = 0;
};

std::wstring editText(HWND edit) {
    std::wstring value(GetWindowTextLengthW(edit) + 1, L'\0');
    GetWindowTextW(edit, value.data(), static_cast<int>(value.size()));
    value.resize(wcslen(value.c_str()));
    return value;
}

RECT body(HWND window, const Dialog& dialog) {
    RECT rect{};
    GetClientRect(window, &rect);
    return {14,
            dialog.mode == Mode::Path    ? 84L
            : dialog.mode == Mode::Color ? 66L
                                         : 14L,
            rect.right - 38, rect.bottom - 78};
}

Scrollbar bar(HWND window, const Dialog& dialog) {
    const auto rect = body(window, dialog);
    const int page = dialog.mode == Mode::Path ? std::max(1L, (rect.bottom - rect.top) / 26)
                                               : std::max(1L, rect.bottom - rect.top);
    return verticalScrollbar({rect.right + 4, rect.top, rect.right + 26, rect.bottom},
                             dialog.extent, page, dialog.position);
}

RECT actionRect(HWND window, int index, int count, bool vertical = false) {
    RECT rect{};
    GetClientRect(window, &rect);
    if (vertical) {
        return {14, 50 + index * 38, rect.right - 14, 84 + index * 38};
    }
    const int width = (rect.right - 28) / std::max(1, count);
    return {14 + index * width, rect.bottom - 48, 8 + (index + 1) * width, rect.bottom - 12};
}

void navigate(HWND window, Dialog& dialog, const fs::path& path) {
    std::error_code error;
    std::vector<fs::path> entries;
    fs::directory_iterator iterator(path, error);
    while (!error && iterator != fs::directory_iterator{}) {
        const bool directory = iterator->is_directory(error);
        if (!error && (directory || dialog.request != Request::Folder)) {
            entries.push_back(iterator->path());
        }
        iterator.increment(error);
    }
    if (error) {
        dialog.error = tr(L"Verzeichnis kann nicht gelesen werden.", L"Directory cannot be read.");
    } else {
        std::sort(entries.begin(), entries.end());
        dialog.directory = path;
        dialog.entries = std::move(entries);
        dialog.directories.clear();
        for (const auto& entry : dialog.entries) {
            dialog.directories.push_back(fs::is_directory(entry, error));
        }
        dialog.extent = static_cast<int>(dialog.entries.size());
        dialog.position = 0;
        dialog.error.clear();
        SetWindowTextW(dialog.edit, path.c_str());
    }
    InvalidateRect(window, nullptr, FALSE);
}

void activate(HWND window, Dialog& dialog, int index) {
    const int result = dialog.actions[index].first;
    if (result == IDOK && dialog.mode == Mode::Path) {
        fs::path path(editText(dialog.edit));
        if (path.empty()) {
            dialog.error = tr(L"Bitte einen Pfad eingeben.", L"Enter a path.");
        } else {
            if (path.is_relative()) {
                path = dialog.directory / path;
            }
            if (dialog.request == Request::SaveFile && path.extension().empty()) {
                path += dialog.defaultExtension;
            }
            std::error_code error;
            const bool directory = fs::is_directory(path, error);
            if (directory && dialog.request != Request::Folder) {
                navigate(window, dialog, path);
                return;
            }
            if (dialog.request == Request::Folder ? directory
                : dialog.request == Request::OpenFile
                    ? fs::is_regular_file(path, error)
                    : fs::is_directory(path.parent_path(), error)) {
                if (dialog.request == Request::SaveFile && fs::exists(path, error) &&
                    message(window, tr(L"Vorhandene Datei ersetzen?", L"Replace existing file?"),
                            tr(L"Speichern", L"Save"), MB_YESNO) != IDYES) {
                    return;
                }
                dialog.chosenPath = path;
                dialog.result = IDOK;
                DestroyWindow(window);
                return;
            }
            dialog.error =
                tr(L"Pfad ist ungültig oder nicht erreichbar.", L"Path is invalid or unavailable.");
        }
        InvalidateRect(window, nullptr, FALSE);
        return;
    }
    if (result == IDOK && dialog.mode == Mode::Color) {
        auto value = editText(dialog.edit);
        if (!value.empty() && value.front() == L'#') {
            value.erase(0, 1);
        }
        wchar_t* end = nullptr;
        const auto color = wcstoul(value.c_str(), &end, 16);
        if (value.size() != 6 || !end || *end || color > 0xFFFFFF) {
            dialog.error = tr(L"Farbe als #RRGGBB eingeben.", L"Enter a color as #RRGGBB.");
            InvalidateRect(window, nullptr, FALSE);
            return;
        }
        dialog.chosenColor = RGB((color >> 16) & 255, (color >> 8) & 255, color & 255);
    }
    dialog.result = result;
    DestroyWindow(window);
}

void paint(HWND window, Dialog& dialog) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(window, &ps);
    RECT client{};
    GetClientRect(window, &client);
    fill(dc, client, dialog.theme.background);
    auto oldFont = SelectObject(dc, dialog.font);
    const auto rect = body(window, dialog);
    if (dialog.mode == Mode::Text) {
        RECT measure{rect.left, 0, rect.right, 0};
        DrawTextW(dc, dialog.content.c_str(), -1, &measure,
                  DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX | DT_EXPANDTABS);
        dialog.extent = measure.bottom;
    }
    dialog.position = std::clamp(dialog.position, 0, bar(window, dialog).maximum);
    const int saved = SaveDC(dc);
    IntersectClipRect(dc, rect.left, rect.top, rect.right, rect.bottom);
    if (dialog.mode == Mode::Text) {
        label(dc,
              {rect.left, rect.top - dialog.position, rect.right,
               rect.top - dialog.position + dialog.extent},
              dialog.content, dialog.theme.text, DT_WORDBREAK | DT_EXPANDTABS);
    } else if (dialog.mode == Mode::Path) {
        const int page = std::max(1L, (rect.bottom - rect.top) / 26);
        for (int row = 0;
             row <= page && dialog.position + row < static_cast<int>(dialog.entries.size());
             ++row) {
            const int index = dialog.position + row;
            const auto name = (dialog.directories[index] ? L"[+] " : L"    ") +
                              dialog.entries[index].filename().wstring();
            label(dc, {rect.left + 4, rect.top + row * 26, rect.right, rect.top + (row + 1) * 26},
                  name, dialog.theme.text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
    } else if (dialog.mode == Mode::Color) {
        for (int i = 0; i < 32; ++i) {
            const COLORREF color = i < 16 ? RGB((i & 1) ? 255 : 0, (i & 2) ? 255 : 0,
                                                (i & 4)   ? 255
                                                : (i & 8) ? 128
                                                          : 0)
                                          : RGB((i - 16) * 17, (i - 16) * 17, (i - 16) * 17);
            const int x = rect.left + (i % 8) * 64;
            const int y = rect.top + (i / 8) * 56;
            fill(dc, {x, y, x + 56, y + 48}, color);
            bevel(dc, {x, y, x + 56, y + 48});
        }
    } else {
        label(dc, rect, dialog.content, dialog.theme.text, DT_WORDBREAK);
    }
    RestoreDC(dc, saved);
    if (dialog.mode == Mode::Text || dialog.mode == Mode::Path) {
        const auto scroll = bar(window, dialog);
        fill(dc, scroll.track, RGB(100, 100, 100));
        bevel(dc, scroll.track, true);
        fill(dc, scroll.thumb, dialog.theme.background);
        bevel(dc, scroll.thumb, dialog.dragging);
        bevel(dc, scroll.up);
        bevel(dc, scroll.down);
        label(dc, scroll.up, L"↑", dialog.theme.text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        label(dc, scroll.down, L"↓", dialog.theme.text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (dialog.edit) {
        RECT edit{14, 12, client.right - 14, 50};
        bevel(dc, edit, true);
    }
    if (dialog.mode == Mode::Path) {
        bevel(dc, {14, 52, 220, 80});
        label(dc, {14, 52, 220, 80}, tr(L"Übergeordnet", L"Parent"), dialog.theme.text,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (size_t i = 0; i < dialog.actions.size(); ++i) {
        const auto button =
            actionRect(window, static_cast<int>(i), static_cast<int>(dialog.actions.size()),
                       dialog.mode == Mode::Menu);
        bevel(dc, button, static_cast<int>(i) == dialog.focus);
        label(dc, button, dialog.actions[i].second, dialog.theme.text,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    label(dc, {14, client.bottom - 76, client.right - 14, client.bottom - 50}, dialog.error,
          RGB(140, 0, 0), DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, oldFont);
    EndPaint(window, &ps);
}

LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* dialog = reinterpret_cast<Dialog*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        dialog = static_cast<Dialog*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
    }
    if (!dialog) {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    switch (message) {
        case WM_PAINT:
            paint(window, *dialog);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_CTLCOLOREDIT:
            SetTextColor(reinterpret_cast<HDC>(wParam), dialog->theme.text);
            SetBkColor(reinterpret_cast<HDC>(wParam), dialog->theme.background);
            return reinterpret_cast<LRESULT>(dialog->brush);
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_MOUSEWHEEL:
            dialog->position -= GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA *
                                (dialog->mode == Mode::Path ? 3 : 48);
            dialog->position = std::clamp(dialog->position, 0, bar(window, *dialog).maximum);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_MOUSEMOVE:
            if (dialog->dragging) {
                dialog->position =
                    rowFromThumb(bar(window, *dialog), GET_Y_LPARAM(lParam), dialog->grab);
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
        case WM_CANCELMODE:
            if (GetCapture() == window) {
                ReleaseCapture();
            }
            return 0;
        case WM_CAPTURECHANGED:
            dialog->dragging = false;
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            for (size_t i = 0; i < dialog->actions.size(); ++i) {
                const auto rect = actionRect(window, static_cast<int>(i),
                                             static_cast<int>(dialog->actions.size()),
                                             dialog->mode == Mode::Menu);
                if (PtInRect(&rect, point)) {
                    activate(window, *dialog, static_cast<int>(i));
                    return 0;
                }
            }
            const auto rect = body(window, *dialog);
            if (dialog->mode == Mode::Path && point.y >= 52 && point.y < 80 && point.x < 220) {
                navigate(window, *dialog, dialog->directory.parent_path());
                return 0;
            }
            if (PtInRect(&rect, point) && dialog->mode == Mode::Path) {
                const int index = dialog->position + (point.y - rect.top) / 26;
                if (index < static_cast<int>(dialog->entries.size())) {
                    SetWindowTextW(dialog->edit, dialog->entries[index].c_str());
                    if (message == WM_LBUTTONDBLCLK) {
                        if (dialog->directories[index]) {
                            navigate(window, *dialog, dialog->entries[index]);
                        } else {
                            activate(window, *dialog, 0);
                        }
                    }
                }
            } else if (PtInRect(&rect, point) && dialog->mode == Mode::Color) {
                const int column = (point.x - rect.left) / 64;
                const int row = (point.y - rect.top) / 56;
                const int i = row * 8 + column;
                if (column < 8 && row < 4) {
                    const COLORREF color = i < 16
                                               ? RGB((i & 1) ? 255 : 0, (i & 2) ? 255 : 0,
                                                     (i & 4)   ? 255
                                                     : (i & 8) ? 128
                                                               : 0)
                                               : RGB((i - 16) * 17, (i - 16) * 17, (i - 16) * 17);
                    wchar_t value[16]{};
                    swprintf_s(value, L"#%02X%02X%02X", GetRValue(color), GetGValue(color),
                               GetBValue(color));
                    SetWindowTextW(dialog->edit, value);
                }
            } else if (dialog->mode == Mode::Text || dialog->mode == Mode::Path) {
                const auto scroll = bar(window, *dialog);
                const int step = dialog->mode == Mode::Path ? 1 : 24;
                if (scroll.maximum && PtInRect(&scroll.thumb, point)) {
                    dialog->dragging = true;
                    dialog->grab = point.y - scroll.thumb.top;
                    SetCapture(window);
                } else if (PtInRect(&scroll.up, point)) {
                    dialog->position -= step;
                } else if (PtInRect(&scroll.down, point)) {
                    dialog->position += step;
                } else if (PtInRect(&scroll.track, point)) {
                    dialog->position += point.y < scroll.thumb.top ? -step * 8 : step * 8;
                }
                dialog->position = std::clamp(dialog->position, 0, scroll.maximum);
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_NCDESTROY:
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int run(HWND owner, const wchar_t* title, Dialog& dialog) {
    dialog.theme = theme;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW type{};
    type.hInstance = instance;
    type.lpfnWndProc = procedure;
    type.style = CS_DBLCLKS;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.lpszClassName = L"WorkbenchRequester";
    if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return IDCANCEL;
    }
    dialog.font = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                              FIXED_PITCH, L"Consolas");
    dialog.brush = CreateSolidBrush(theme.background);
    HWND window = CreateWindowExW(
        WS_EX_DLGMODALFRAME, type.lpszClassName, title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 680,
        dialog.mode == Mode::Menu ? 114 + static_cast<int>(dialog.actions.size()) * 38 : 540, owner,
        nullptr, instance, &dialog);
    if (!window || !retro_frame::install(window, theme.frame)) {
        if (window) {
            DestroyWindow(window);
        }
        DeleteObject(dialog.font);
        DeleteObject(dialog.brush);
        return IDCANCEL;
    }
    if (dialog.mode == Mode::Path || dialog.mode == Mode::Color) {
        dialog.edit =
            CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                            20, 18, 624, 26, window, nullptr, instance, nullptr);
        SendMessageW(dialog.edit, WM_SETFONT, reinterpret_cast<WPARAM>(dialog.font), TRUE);
        SendMessageW(dialog.edit, EM_SETLIMITTEXT, 32767, 0);
        SetWindowTheme(dialog.edit, L"", L"");
        if (dialog.mode == Mode::Path) {
            navigate(window, dialog, dialog.directory);
        }
        SetWindowTextW(dialog.edit, dialog.content.c_str());
    }
    const bool disableOwner = owner && IsWindowEnabled(owner);
    if (disableOwner) {
        EnableWindow(owner, FALSE);
    }
    ShowWindow(window, SW_SHOW);
    SetFocus(dialog.edit ? dialog.edit : window);
    MSG event{};
    while (IsWindow(window)) {
        const int result = GetMessageW(&event, nullptr, 0, 0);
        if (result <= 0) {
            if (result == 0) {
                PostQuitMessage(static_cast<int>(event.wParam));
            }
            DestroyWindow(window);
            break;
        }
        if ((event.hwnd == window || IsChild(window, event.hwnd)) && event.message == WM_KEYDOWN) {
            if (event.wParam == VK_ESCAPE) {
                DestroyWindow(window);
                continue;
            }
            if (event.wParam == VK_RETURN) {
                activate(window, dialog, dialog.focus);
                continue;
            }
            if (event.wParam == VK_TAB) {
                dialog.focus = (dialog.focus + 1) % static_cast<int>(dialog.actions.size());
                SetFocus(dialog.edit && GetFocus() != dialog.edit ? dialog.edit : window);
                InvalidateRect(window, nullptr, FALSE);
                continue;
            }
            if (event.hwnd == window && (event.wParam == VK_UP || event.wParam == VK_DOWN ||
                                         event.wParam == VK_PRIOR || event.wParam == VK_NEXT)) {
                const int step = dialog.mode == Mode::Path ? 1 : 24;
                dialog.position += (event.wParam == VK_UP || event.wParam == VK_PRIOR ? -1 : 1) *
                                   step *
                                   (event.wParam == VK_PRIOR || event.wParam == VK_NEXT ? 8 : 1);
                InvalidateRect(window, nullptr, FALSE);
                continue;
            }
        }
        TranslateMessage(&event);
        DispatchMessageW(&event);
    }
    if (disableOwner && IsWindow(owner)) {
        EnableWindow(owner, TRUE);
        SetActiveWindow(owner);
    }
    DeleteObject(dialog.font);
    DeleteObject(dialog.brush);
    return dialog.result;
}
} // namespace

void setDialogLanguage(bool value) {
    german = value;
}

void setDialogTheme(DialogTheme value) {
    theme = value;
}
int message(HWND owner, const wchar_t* text, const wchar_t* title, UINT flags) {
    Dialog dialog;
    dialog.content = text;
    const UINT kind = flags & MB_TYPEMASK;
    if (kind == MB_YESNO || kind == MB_YESNOCANCEL) {
        dialog.actions = {{IDYES, tr(L"Ja", L"Yes")}, {IDNO, tr(L"Nein", L"No")}};
        if (kind == MB_YESNOCANCEL) {
            dialog.actions.push_back({IDCANCEL, tr(L"Abbrechen", L"Cancel")});
        }
        dialog.focus = static_cast<int>(dialog.actions.size()) - 1;
    } else {
        dialog.actions = {{IDOK, tr(L"OK", L"OK")}};
    }
    return run(owner, title, dialog);
}
int menu(HWND owner, const wchar_t* title,
         const std::vector<std::pair<int, std::wstring>>& actions) {
    Dialog dialog;
    dialog.mode = Mode::Menu;
    dialog.actions = actions;
    dialog.content = tr(L"Aktion wählen:", L"Choose an action:");
    return run(owner, title, dialog);
}
std::optional<fs::path> requestPath(HWND owner, const wchar_t* title, Request request,
                                    const fs::path& initial) {
    Dialog dialog;
    dialog.mode = Mode::Path;
    dialog.request = request;
    dialog.defaultExtension = initial.extension().wstring();
    std::error_code error;
    auto path = initial.empty() ? fs::current_path(error) : fs::absolute(initial, error);
    if (error) {
        return std::nullopt;
    }
    dialog.directory = fs::is_directory(path, error) ? path : path.parent_path();
    dialog.content = path.wstring();
    dialog.actions = {{IDOK, request == Request::SaveFile ? tr(L"Speichern", L"Save")
                             : request == Request::Folder ? tr(L"Ordner wählen", L"Select folder")
                                                          : tr(L"Öffnen", L"Open")},
                      {IDCANCEL, tr(L"Abbrechen", L"Cancel")}};
    return run(owner, title, dialog) == IDOK ? std::optional<fs::path>(dialog.chosenPath)
                                             : std::nullopt;
}
std::optional<COLORREF> requestColor(HWND owner, COLORREF initial) {
    Dialog dialog;
    dialog.mode = Mode::Color;
    wchar_t value[16]{};
    swprintf_s(value, L"#%02X%02X%02X", GetRValue(initial), GetGValue(initial), GetBValue(initial));
    dialog.content = value;
    dialog.actions = {{IDOK, tr(L"Farbe übernehmen", L"Use color")},
                      {IDCANCEL, tr(L"Abbrechen", L"Cancel")}};
    return run(owner, tr(L"Farbe wählen – #RRGGBB", L"Choose color – #RRGGBB"), dialog) == IDOK
               ? std::optional<COLORREF>(dialog.chosenColor)
               : std::nullopt;
}
} // namespace workbench
