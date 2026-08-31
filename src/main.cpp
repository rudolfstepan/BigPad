#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr wchar_t kClassName[] = L"BigPadWindow";
constexpr UINT_PTR kNew = 99, kOpen = 100, kSave = 101, kSaveAs = 102, kExit = 103;
constexpr UINT_PTR kLineNumbers = 200, kWordWrap = 201;
constexpr UINT_PTR kLightMode = 202, kDarkMode = 203;
constexpr UINT_PTR kFontConsolas = 300, kFontCascadia = 301, kFontCourier = 302, kFontLucida = 303;
constexpr UINT_PTR kSize10 = 320, kSize12 = 321, kSize14 = 322, kSize16 = 323, kSize18 = 324,
                   kSize20 = 325, kSize24 = 326, kSize28 = 327;
constexpr UINT_PTR kAbout = 400;
constexpr UINT_PTR kSyntaxAuto = 500, kSyntaxNone = 501, kSyntaxCpp = 502, kSyntaxJson = 503,
                   kSyntaxPython = 504, kSyntaxXml = 505, kSyntaxLog = 506,
                   kSyntaxCSharp = 507, kSyntaxAsm = 508;
constexpr UINT_PTR kTextMode = 600, kHexMode = 601;
constexpr UINT_PTR kSaveSettings = 700, kAutoSaveSettings = 701, kResetSettings = 702;

enum class Syntax { None, Cpp, Json, Python, Xml, Log, CSharp, Asm };

std::wstring winError(DWORD code = GetLastError()) {
    wchar_t* p = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&p), 0, nullptr);
    std::wstring result = p ? p : L"Unbekannter Fehler";
    LocalFree(p);
    return result;
}

class Document {
public:
    enum class Source : uint8_t { Original, Added };
    struct Piece { Source source; uint64_t offset; uint64_t length; };

    ~Document() { close(); }
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    Document() = default;

    bool open(const std::wstring& path, std::wstring& error) {
        close();
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
        if (h == INVALID_HANDLE_VALUE) { error = winError(); return false; }
        LARGE_INTEGER n{};
        if (!GetFileSizeEx(h, &n)) { error = winError(); CloseHandle(h); return false; }
        file_ = h; open_ = true; path_ = path; originalSize_ = static_cast<uint64_t>(n.QuadPart);
        if (originalSize_) pieces_.push_back({Source::Original, 0, originalSize_});
        dirty_ = false;
        invalidateCache();
        return true;
    }

    void close() {
        if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE; open_ = false; path_.clear(); pieces_.clear(); added_.clear();
        originalSize_ = 0; dirty_ = false; invalidateCache();
    }

    uint64_t size() const {
        uint64_t n = 0; for (const auto& p : pieces_) n += p.length; return n;
    }
    void createNew() { close(); open_ = true; }
    bool isOpen() const { return open_; }
    bool dirty() const { return dirty_; }
    const std::wstring& path() const { return path_; }

    size_t read(uint64_t pos, char* out, size_t count) const {
        if (pos >= size() || count == 0) return 0;
        size_t written = 0; uint64_t logical = 0;
        for (const auto& piece : pieces_) {
            if (pos >= logical + piece.length) { logical += piece.length; continue; }
            uint64_t within = pos > logical ? pos - logical : 0;
            size_t take = static_cast<size_t>(std::min<uint64_t>(piece.length - within, count - written));
            if (piece.source == Source::Added) {
                memcpy(out + written, added_.data() + piece.offset + within, take);
            } else if (!readOriginal(piece.offset + within, out + written, take)) {
                break;
            }
            written += take; pos += take; logical += piece.length;
            if (written == count) break;
        }
        return written;
    }

    std::string read(uint64_t pos, size_t count) const {
        std::string s(count, '\0'); s.resize(read(pos, s.data(), count)); return s;
    }

    void insert(uint64_t pos, std::string_view bytes) {
        if (bytes.empty()) return;
        pos = std::min(pos, size());
        const uint64_t addOffset = added_.size();
        added_.insert(added_.end(), bytes.begin(), bytes.end());
        splitAt(pos);
        size_t index = pieceIndexAt(pos);
        pieces_.insert(pieces_.begin() + static_cast<ptrdiff_t>(index),
                       {Source::Added, addOffset, static_cast<uint64_t>(bytes.size())});
        coalesce(); dirty_ = true;
    }

    void erase(uint64_t pos, uint64_t count) {
        uint64_t n = size(); if (pos >= n || count == 0) return;
        count = std::min(count, n - pos);
        splitAt(pos); splitAt(pos + count);
        size_t first = pieceIndexAt(pos), last = pieceIndexAt(pos + count);
        pieces_.erase(pieces_.begin() + static_cast<ptrdiff_t>(first),
                      pieces_.begin() + static_cast<ptrdiff_t>(last));
        coalesce(); dirty_ = true;
    }

    bool saveAs(const std::wstring& target, std::wstring& error) {
        std::wstring temp = target + L".bigpad.tmp";
        HANDLE out = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
        if (out == INVALID_HANDLE_VALUE) { error = winError(); return false; }
        std::array<char, 1024 * 1024> buffer{};
        bool ok = true;
        for (const auto& p : pieces_) {
            uint64_t done = 0;
            while (done < p.length) {
                DWORD chunk = static_cast<DWORD>(std::min<uint64_t>(buffer.size(), p.length - done));
                if (p.source == Source::Added)
                    memcpy(buffer.data(), added_.data() + p.offset + done, chunk);
                else if (!readOriginal(p.offset + done, buffer.data(), chunk)) { ok = false; break; }
                DWORD written = 0;
                if (!WriteFile(out, buffer.data(), chunk, &written, nullptr) || written != chunk) {
                    error = winError(); ok = false; break;
                }
                done += chunk;
            }
            if (!ok) break;
        }
        if (ok && !FlushFileBuffers(out)) { error = winError(); ok = false; }
        CloseHandle(out);
        if (!ok) { DeleteFileW(temp.c_str()); return false; }
        if (!MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            error = winError(); DeleteFileW(temp.c_str()); return false;
        }
        return open(target, error);
    }

private:
    void invalidateCache() const { cacheStart_ = UINT64_MAX; cache_.clear(); }
    bool readOriginal(uint64_t pos, char* out, size_t count) const {
        size_t done = 0;
        while (done < count) {
            if (cacheStart_ == UINT64_MAX || pos < cacheStart_ || pos >= cacheStart_ + cache_.size()) {
                constexpr size_t window = 1024 * 1024;
                cacheStart_ = pos; cache_.resize(static_cast<size_t>(std::min<uint64_t>(window, originalSize_ - pos)));
                LARGE_INTEGER at{}; at.QuadPart = static_cast<LONGLONG>(pos);
                if (!SetFilePointerEx(file_, at, nullptr, FILE_BEGIN)) return false;
                DWORD got = 0;
                if (!ReadFile(file_, cache_.data(), static_cast<DWORD>(cache_.size()), &got, nullptr)) return false;
                cache_.resize(got); if (!got) return false;
            }
            size_t offset = static_cast<size_t>(pos - cacheStart_);
            size_t take = std::min(count - done, cache_.size() - offset);
            memcpy(out + done, cache_.data() + offset, take); done += take; pos += take;
        }
        return true;
    }

    size_t pieceIndexAt(uint64_t pos) const {
        uint64_t logical = 0;
        for (size_t i = 0; i < pieces_.size(); ++i) {
            if (logical == pos) return i;
            logical += pieces_[i].length;
        }
        return pieces_.size();
    }
    void splitAt(uint64_t pos) {
        uint64_t logical = 0;
        for (size_t i = 0; i < pieces_.size(); ++i) {
            auto p = pieces_[i];
            if (pos > logical && pos < logical + p.length) {
                uint64_t left = pos - logical;
                pieces_[i].length = left;
                pieces_.insert(pieces_.begin() + static_cast<ptrdiff_t>(i + 1),
                               {p.source, p.offset + left, p.length - left});
                return;
            }
            logical += p.length;
        }
    }
    void coalesce() {
        for (size_t i = 1; i < pieces_.size();) {
            auto& a = pieces_[i - 1]; const auto& b = pieces_[i];
            if (a.source == b.source && a.offset + a.length == b.offset) {
                a.length += b.length; pieces_.erase(pieces_.begin() + static_cast<ptrdiff_t>(i));
            } else ++i;
        }
    }

    HANDLE file_ = INVALID_HANDLE_VALUE;
    bool open_ = false;
    std::wstring path_;
    uint64_t originalSize_ = 0;
    std::vector<Piece> pieces_;
    std::vector<char> added_;
    bool dirty_ = false;
    mutable uint64_t cacheStart_ = UINT64_MAX;
    mutable std::vector<char> cache_;
};

struct Line {
    uint64_t start{}, contentEnd{}, next{}, number{};
    bool continuation{};
    std::wstring text;
};

class Editor {
public:
    explicit Editor(HWND hwnd) : hwnd_(hwnd) {
        recreateFont();
        doc_.createNew();
        status_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                  0, 0, 0, 0, hwnd_, nullptr, GetModuleHandleW(nullptr), nullptr);
        loadSettings();
    }
    ~Editor() { DeleteObject(font_); }

    void paint() {
        PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd_, &ps);
        RECT rc{}; GetClientRect(hwnd_, &rc);
        rc.bottom = textBottom();
        IntersectClipRect(dc, rc.left, rc.top, rc.right, rc.bottom);
        HBRUSH background = CreateSolidBrush(darkMode_ ? RGB(30, 30, 30) : GetSysColor(COLOR_WINDOW));
        FillRect(dc, &rc, background); DeleteObject(background);
        SelectObject(dc, font_); SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, darkMode_ ? RGB(225, 225, 225) : GetSysColor(COLOR_WINDOWTEXT));
        if (hexMode_) { paintHex(dc); updateChrome(); EndPaint(hwnd_, &ps); return; }
        rebuildLines();
        updateChrome();
        const int gutter = gutterWidth();
        if (showLineNumbers_) {
            RECT gr{0, 0, gutter - 8, rc.bottom};
            HBRUSH gutterBrush = CreateSolidBrush(darkMode_ ? RGB(38, 38, 38) : GetSysColor(COLOR_BTNFACE));
            FillRect(dc, &gr, gutterBrush); DeleteObject(gutterBrush);
            HPEN pen = CreatePen(PS_SOLID, 1, darkMode_ ? RGB(70, 70, 70) : GetSysColor(COLOR_3DSHADOW));
            HPEN old = reinterpret_cast<HPEN>(SelectObject(dc, pen));
            MoveToEx(dc, gutter - 8, 0, nullptr); LineTo(dc, gutter - 8, rc.bottom);
            SelectObject(dc, old); DeleteObject(pen);
        }
        int y = 0;
        for (const auto& line : lines_) {
            if (showLineNumbers_ && !line.continuation) {
                std::wstring number = std::to_wstring(line.number);
                RECT nr{2, y, gutter - 12, y + lineHeight_};
                SetTextColor(dc, darkMode_ ? RGB(145, 145, 145) : GetSysColor(COLOR_GRAYTEXT));
                DrawTextW(dc, number.c_str(), static_cast<int>(number.size()), &nr,
                          DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
                SetTextColor(dc, darkMode_ ? RGB(225, 225, 225) : GetSysColor(COLOR_WINDOWTEXT));
            }
            paintHighlightedLine(dc, gutter, y, line.text); y += lineHeight_;
        }
        if (GetFocus() == hwnd_) {
            auto [row, col] = cursorScreenPosition();
            if (row >= 0) { SetCaretPos(gutter + col * charWidth_, row * lineHeight_); ShowCaret(hwnd_); }
        }
        EndPaint(hwnd_, &ps);
    }

    bool openDialog() {
        if (!confirmDiscard()) return false;
        wchar_t path[32768]{};
        OPENFILENAMEW ofn{sizeof(ofn)}; ofn.hwndOwner = hwnd_; ofn.lpstrFile = path;
        ofn.nMaxFile = ARRAYSIZE(path); ofn.lpstrFilter = L"Textdateien\0*.txt;*.log;*.csv;*.json;*.xml;*.md\0Alle Dateien\0*.*\0";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
        if (!GetOpenFileNameW(&ofn)) return false;
        return openPath(path, false);
    }

    bool openPath(const std::wstring& path, bool askToDiscard = true) {
        if (askToDiscard && !confirmDiscard()) return false;
        std::wstring error;
        if (!doc_.open(path, error)) { showError(error); return false; }
        top_ = cursor_ = 0; preferredCol_ = 0;
        if (syntaxAuto_) syntax_ = syntaxForPath(path);
        numberedPosition_ = UINT64_MAX; refresh(); return true;
    }

    void dropFiles(HDROP drop) {
        UINT length = DragQueryFileW(drop, 0, nullptr, 0);
        if (length != 0 && length != 0xFFFFFFFF) {
            std::wstring path(static_cast<size_t>(length) + 1, L'\0');
            DragQueryFileW(drop, 0, path.data(), length + 1);
            path.resize(length);
            openPath(path);
        }
        DragFinish(drop);
    }

    void newDocument() {
        if (!confirmDiscard()) return;
        doc_.createNew(); top_ = cursor_ = 0; preferredCol_ = 0; refresh();
    }

    bool save(bool choosePath) {
        if (!doc_.isOpen()) return false;
        std::wstring path = doc_.path();
        if (choosePath || path.empty()) {
            std::vector<wchar_t> buf(32768); wcsncpy_s(buf.data(), buf.size(), path.c_str(), _TRUNCATE);
            OPENFILENAMEW ofn{sizeof(ofn)}; ofn.hwndOwner = hwnd_; ofn.lpstrFile = buf.data();
            ofn.nMaxFile = static_cast<DWORD>(buf.size()); ofn.lpstrFilter = L"Textdateien\0*.txt\0Alle Dateien\0*.*\0";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
            if (!GetSaveFileNameW(&ofn)) return false; path = buf.data();
        }
        std::wstring error;
        if (!doc_.saveAs(path, error)) { showError(error); return false; }
        cursor_ = std::min(cursor_, doc_.size()); refresh(); return true;
    }

    bool confirmDiscard() {
        if (!doc_.dirty()) return true;
        int r = MessageBoxW(hwnd_, L"Aenderungen speichern?", L"BigPad", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (r == IDCANCEL) return false; if (r == IDYES) return save(false); return true;
    }

    void resize() { if (status_) SendMessageW(status_, WM_SIZE, 0, 0); measure(); refresh(); }
    void toggleLineNumbers() { showLineNumbers_ = !showLineNumbers_; settingChanged(); refreshMenu(); refresh(); }
    void toggleWordWrap() { wordWrap_ = !wordWrap_; settingChanged(); refreshMenu(); refresh(); }
    void setDarkMode(bool dark) {
        darkMode_ = dark;
        BOOL enabled = dark ? TRUE : FALSE;
        // DWMWA_USE_IMMERSIVE_DARK_MODE ist ab Windows 10 20H1 die ID 20.
        if (FAILED(DwmSetWindowAttribute(hwnd_, 20, &enabled, sizeof(enabled))))
            DwmSetWindowAttribute(hwnd_, 19, &enabled, sizeof(enabled));
        SetWindowTheme(hwnd_, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        settingChanged(); refreshMenu(); DrawMenuBar(hwnd_); refresh();
    }
    void selectFont(UINT_PTR command) {
        switch (command) {
        case kFontConsolas: fontName_ = L"Consolas"; break;
        case kFontCascadia: fontName_ = L"Cascadia Mono"; break;
        case kFontCourier: fontName_ = L"Courier New"; break;
        case kFontLucida: fontName_ = L"Lucida Console"; break;
        default: return;
        }
        fontCommand_ = command; recreateFont(); settingChanged(); refreshMenu(); refresh();
    }
    void selectFontSize(UINT_PTR command) {
        switch (command) {
        case kSize10: fontPoints_ = 10; break; case kSize12: fontPoints_ = 12; break;
        case kSize14: fontPoints_ = 14; break; case kSize16: fontPoints_ = 16; break;
        case kSize18: fontPoints_ = 18; break; case kSize20: fontPoints_ = 20; break;
        case kSize24: fontPoints_ = 24; break; case kSize28: fontPoints_ = 28; break;
        default: return;
        }
        sizeCommand_ = command; recreateFont(); settingChanged(); refreshMenu(); refresh();
    }
    void showAbout() {
        MessageBoxW(hwnd_,
            L"BigPad\n\nEin nativer Windows-Texteditor fuer sehr grosse Dateien.\n"
            L"Dateiinhalte werden fensterweise gelesen und Aenderungen in einer Piece-Table verwaltet.\n\n"
            L"Version 1.0",
            L"Ueber BigPad", MB_OK | MB_ICONINFORMATION);
    }
    void selectSyntax(UINT_PTR command) {
        syntaxAuto_ = command == kSyntaxAuto;
        switch (command) {
        case kSyntaxAuto: syntax_ = syntaxForPath(doc_.path()); break;
        case kSyntaxNone: syntax_ = Syntax::None; break; case kSyntaxCpp: syntax_ = Syntax::Cpp; break;
        case kSyntaxJson: syntax_ = Syntax::Json; break; case kSyntaxPython: syntax_ = Syntax::Python; break;
        case kSyntaxXml: syntax_ = Syntax::Xml; break; case kSyntaxLog: syntax_ = Syntax::Log; break;
        case kSyntaxCSharp: syntax_ = Syntax::CSharp; break; case kSyntaxAsm: syntax_ = Syntax::Asm; break;
        default: return;
        }
        syntaxCommand_ = command; settingChanged(); refreshMenu(); refresh();
    }
    void setHexMode(bool enabled) {
        hexMode_ = enabled; highNibble_ = true;
        top_ = enabled ? (cursor_ / 16) * 16 : lineStart(cursor_);
        settingChanged(); refreshMenu(); refresh();
    }
    void toggleAutoSaveSettings() { autoSaveSettings_ = !autoSaveSettings_; saveSettings(false); refreshMenu(); }
    void saveSettings(bool notify = true) {
        HKEY key{};
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\BigPad", 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
            if (notify) MessageBoxW(hwnd_, L"Die Einstellungen konnten nicht gespeichert werden.", L"BigPad", MB_OK | MB_ICONERROR);
            return;
        }
        auto write = [key](const wchar_t* name, DWORD value) { RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)); };
        write(L"DarkMode", darkMode_); write(L"LineNumbers", showLineNumbers_); write(L"WordWrap", wordWrap_);
        write(L"FontPoints", fontPoints_); write(L"FontCommand", static_cast<DWORD>(fontCommand_));
        write(L"SyntaxCommand", static_cast<DWORD>(syntaxCommand_)); write(L"HexMode", hexMode_);
        write(L"AutoSaveSettings", autoSaveSettings_);
        RegSetValueExW(key, L"FontName", 0, REG_SZ, reinterpret_cast<const BYTE*>(fontName_.c_str()), static_cast<DWORD>((fontName_.size()+1)*sizeof(wchar_t)));
        RegCloseKey(key);
        if (notify) MessageBoxW(hwnd_, L"Einstellungen wurden gespeichert.", L"BigPad", MB_OK | MB_ICONINFORMATION);
    }
    void resetSettings() {
        darkMode_=false; showLineNumbers_=true; wordWrap_=false; fontName_=L"Consolas"; fontPoints_=14;
        fontCommand_=kFontConsolas; sizeCommand_=kSize14; syntaxAuto_=true; syntax_=syntaxForPath(doc_.path()); syntaxCommand_=kSyntaxAuto;
        hexMode_=false; highNibble_=true; autoSaveSettings_=true; recreateFont();
        BOOL dark=FALSE; DwmSetWindowAttribute(hwnd_,20,&dark,sizeof(dark)); SetWindowTheme(hwnd_,L"Explorer",nullptr);
        saveSettings(false); refreshMenu(); DrawMenuBar(hwnd_); refresh();
    }
    void focus(bool in) { if (in) { CreateCaret(hwnd_, nullptr, 2, lineHeight_); ShowCaret(hwnd_); } else DestroyCaret(); }
    void wheel(int delta) {
        if (hexMode_) { int rows = -(delta / WHEEL_DELTA) * 3; if(rows<0) top_-=std::min<uint64_t>(top_,static_cast<uint64_t>(-rows)*16); else top_=std::min(doc_.size(),top_+static_cast<uint64_t>(rows)*16); refresh(); }
        else moveTopLines(-(delta / WHEEL_DELTA) * 3);
    }
    void vscroll(int code, int thumb) {
        if (!doc_.isOpen()) return;
        if (hexMode_) {
            int rows = code == SB_LINEUP ? -1 : code == SB_LINEDOWN ? 1 :
                       code == SB_PAGEUP ? -visibleRows() : code == SB_PAGEDOWN ? visibleRows() : 0;
            if (code == SB_THUMBTRACK || code == SB_THUMBPOSITION) {
                top_ = ((doc_.size() * static_cast<uint64_t>(thumb)) / 1000000ULL / 16) * 16; cursor_ = top_;
            } else if (rows < 0) top_ -= std::min<uint64_t>(top_, static_cast<uint64_t>(-rows) * 16);
            else top_ = std::min(doc_.size(), top_ + static_cast<uint64_t>(rows) * 16);
            refresh(); return;
        }
        switch (code) {
        case SB_LINEUP: moveTopLines(-1); break; case SB_LINEDOWN: moveTopLines(1); break;
        case SB_PAGEUP: moveTopLines(-visibleRows()); break; case SB_PAGEDOWN: moveTopLines(visibleRows()); break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: {
            uint64_t target = doc_.size() ? (doc_.size() * static_cast<uint64_t>(thumb)) / 1000000ULL : 0;
            top_ = lineStart(target); cursor_ = top_; refresh(); break;
        }}
    }

    void key(UINT key) {
        if (!doc_.isOpen()) return;
        if (hexMode_) { keyHex(key); return; }
        rebuildLines();
        switch (key) {
        case VK_LEFT: cursor_ = previousCodepoint(cursor_); preferredCol_ = columnAt(cursor_); break;
        case VK_RIGHT: cursor_ = nextCodepoint(cursor_); preferredCol_ = columnAt(cursor_); break;
        case VK_HOME: cursor_ = lineStart(cursor_); preferredCol_ = 0; break;
        case VK_END: cursor_ = lineEnd(cursor_); preferredCol_ = columnAt(cursor_); break;
        case VK_UP: moveCursorLines(-1); break; case VK_DOWN: moveCursorLines(1); break;
        case VK_PRIOR: moveCursorLines(-visibleRows()); break; case VK_NEXT: moveCursorLines(visibleRows()); break;
        case VK_BACK: if (cursor_) { uint64_t p = previousCodepoint(cursor_); doc_.erase(p, cursor_ - p); cursor_ = p; } break;
        case VK_DELETE: { uint64_t n = nextCodepoint(cursor_); doc_.erase(cursor_, n - cursor_); break; }
        default: return;
        }
        ensureCursorVisible(); refresh();
    }

    void character(wchar_t ch) {
        if (hexMode_) { characterHex(ch); return; }
        if (!doc_.isOpen() || ch < 32 && ch != L'\r' && ch != L'\t') return;
        std::wstring w(1, ch); if (ch == L'\r') w = L"\r\n";
        int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        std::string bytes(n, '\0'); WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), bytes.data(), n, nullptr, nullptr);
        doc_.insert(cursor_, bytes); cursor_ += bytes.size(); preferredCol_ = columnAt(cursor_);
        ensureCursorVisible(); refresh();
    }

    void click(int x, int y) {
        if (y < 0 || y >= textBottom()) return;
        if (hexMode_) {
            int column = (x - (6 + 18 * charWidth_)) / std::max(1, 3 * charWidth_);
            if (column >= 0 && column < 16) { cursor_ = std::min(doc_.size(), top_ + static_cast<uint64_t>(y / lineHeight_) * 16 + column); highNibble_ = true; SetFocus(hwnd_); refresh(); }
            return;
        }
        rebuildLines(); int row = std::clamp(y / lineHeight_, 0, static_cast<int>(lines_.size()) - 1);
        if (lines_.empty()) return; int wanted = std::max(0, (x - gutterWidth() + charWidth_ / 2) / charWidth_);
        cursor_ = byteAtColumn(lines_[row], wanted); preferredCol_ = wanted; SetFocus(hwnd_); refresh();
    }

    void updateTitle() {
        std::wstring name = doc_.path().empty() ? L"Ohne Titel" : doc_.path();
        std::wstring title = (doc_.dirty() ? L"* " : L"") + name + L" - BigPad";
        SetWindowTextW(hwnd_, title.c_str());
    }

private:
    int textBottom() const {
        RECT client{}; GetClientRect(hwnd_, &client);
        if (status_) {
            RECT statusRect{}; GetWindowRect(status_, &statusRect);
            client.bottom -= statusRect.bottom - statusRect.top;
        }
        return std::max(0L, client.bottom);
    }
    int gutterWidth() const { return showLineNumbers_ ? 78 : 6; }
    int wrapColumns() const {
        RECT r{}; GetClientRect(hwnd_, &r);
        return std::max(1, static_cast<int>((r.right - gutterWidth() - 8) / std::max(1, charWidth_)));
    }
    void refreshMenu() {
        HMENU menu = GetMenu(hwnd_);
        CheckMenuItem(menu, kLineNumbers, MF_BYCOMMAND | (showLineNumbers_ ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(menu, kWordWrap, MF_BYCOMMAND | (wordWrap_ ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuRadioItem(menu, kLightMode, kDarkMode, darkMode_ ? kDarkMode : kLightMode, MF_BYCOMMAND);
        CheckMenuRadioItem(menu, kFontConsolas, kFontLucida, fontCommand_, MF_BYCOMMAND);
        CheckMenuRadioItem(menu, kSize10, kSize28, sizeCommand_, MF_BYCOMMAND);
        CheckMenuRadioItem(menu, kSyntaxAuto, kSyntaxAsm, syntaxCommand_, MF_BYCOMMAND);
        CheckMenuRadioItem(menu, kTextMode, kHexMode, hexMode_ ? kHexMode : kTextMode, MF_BYCOMMAND);
        CheckMenuItem(menu, kAutoSaveSettings, MF_BYCOMMAND | (autoSaveSettings_ ? MF_CHECKED : MF_UNCHECKED));
    }
    void settingChanged() { if (settingsLoaded_ && autoSaveSettings_) saveSettings(false); }
    void loadSettings() {
        HKEY key{}; if (RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\BigPad",0,KEY_READ,&key)!=ERROR_SUCCESS) { settingsLoaded_=true; return; }
        auto read = [key](const wchar_t* name, DWORD fallback) { DWORD value=fallback,size=sizeof(value),type=0; return RegQueryValueExW(key,name,nullptr,&type,reinterpret_cast<BYTE*>(&value),&size)==ERROR_SUCCESS&&type==REG_DWORD?value:fallback; };
        darkMode_=read(L"DarkMode",0)!=0; showLineNumbers_=read(L"LineNumbers",1)!=0; wordWrap_=read(L"WordWrap",0)!=0;
        fontPoints_=static_cast<int>(std::clamp<DWORD>(read(L"FontPoints",14),8,72)); fontCommand_=read(L"FontCommand",kFontConsolas);
        syntaxCommand_=read(L"SyntaxCommand",kSyntaxAuto); hexMode_=read(L"HexMode",0)!=0; autoSaveSettings_=read(L"AutoSaveSettings",1)!=0;
        wchar_t name[LF_FACESIZE]{}; DWORD bytes=sizeof(name),type=0; if(RegQueryValueExW(key,L"FontName",nullptr,&type,reinterpret_cast<BYTE*>(name),&bytes)==ERROR_SUCCESS&&type==REG_SZ) fontName_=name;
        RegCloseKey(key); syntaxAuto_=syntaxCommand_==kSyntaxAuto;
        selectSyntax(syntaxCommand_); recreateFont(); BOOL dark=darkMode_; DwmSetWindowAttribute(hwnd_,20,&dark,sizeof(dark)); SetWindowTheme(hwnd_,darkMode_?L"DarkMode_Explorer":L"Explorer",nullptr);
        settingsLoaded_=true; refreshMenu();
    }
    void paintHex(HDC dc) {
        const int hexX = 6 + 18 * charWidth_;
        for (int row = 0; row < visibleRows(); ++row) {
            uint64_t offset = top_ + static_cast<uint64_t>(row) * 16; if (offset > doc_.size()) break;
            std::array<char,16> data{}; size_t got = doc_.read(offset, data.data(), data.size());
            wchar_t address[24]{}; swprintf(address, ARRAYSIZE(address), L"%016llX", static_cast<unsigned long long>(offset));
            SetTextColor(dc, darkMode_ ? RGB(120,170,220) : RGB(60,80,140)); TextOutW(dc, 6, row*lineHeight_, address, 16);
            for (size_t i=0; i<got; ++i) {
                unsigned value = static_cast<unsigned char>(data[i]); wchar_t hex[3]{}; swprintf(hex, 3, L"%02X", value);
                SetTextColor(dc, textColor()); TextOutW(dc, hexX + static_cast<int>(i)*3*charWidth_, row*lineHeight_, hex, 2);
                wchar_t ascii = value >= 32 && value < 127 ? static_cast<wchar_t>(value) : L'.';
                TextOutW(dc, hexX + 50*charWidth_ + static_cast<int>(i)*charWidth_, row*lineHeight_, &ascii, 1);
            }
        }
        int row = static_cast<int>((cursor_ - top_) / 16), col = static_cast<int>(cursor_ % 16);
        if (GetFocus()==hwnd_ && row>=0 && row<visibleRows()) { SetCaretPos(hexX+col*3*charWidth_+(highNibble_?0:charWidth_), row*lineHeight_); ShowCaret(hwnd_); }
    }
    void ensureHexVisible() {
        if (cursor_ < top_) top_ = (cursor_/16)*16;
        uint64_t end = top_ + static_cast<uint64_t>(visibleRows())*16;
        if (cursor_ >= end) top_ = (cursor_/16 - static_cast<uint64_t>(visibleRows()-1))*16;
    }
    void keyHex(UINT key) {
        uint64_t page = static_cast<uint64_t>(visibleRows())*16;
        switch (key) {
        case VK_LEFT: if(cursor_) --cursor_; break; case VK_RIGHT: cursor_=std::min(doc_.size(),cursor_+1); break;
        case VK_UP: cursor_-=std::min<uint64_t>(cursor_,16); break; case VK_DOWN: cursor_=std::min(doc_.size(),cursor_+16); break;
        case VK_PRIOR: cursor_-=std::min(cursor_,page); break; case VK_NEXT: cursor_=std::min(doc_.size(),cursor_+page); break;
        case VK_HOME: cursor_=(cursor_/16)*16; break; case VK_END: cursor_=std::min(doc_.size(),(cursor_/16)*16+15); break;
        case VK_BACK: if(cursor_){doc_.erase(cursor_-1,1);--cursor_;} break; case VK_DELETE: doc_.erase(cursor_,1); break;
        default:return; }
        highNibble_=true; ensureHexVisible(); refresh();
    }
    void characterHex(wchar_t ch) {
        int nibble=ch>=L'0'&&ch<=L'9'?ch-L'0':ch>=L'a'&&ch<=L'f'?ch-L'a'+10:ch>=L'A'&&ch<=L'F'?ch-L'A'+10:-1;
        if(nibble<0||!doc_.isOpen()) return;
        unsigned char old=0; char byte{}; if(cursor_<doc_.size()&&doc_.read(cursor_,&byte,1)) old=static_cast<unsigned char>(byte);
        unsigned char value=highNibble_?static_cast<unsigned char>((nibble<<4)|(old&15)):static_cast<unsigned char>((old&240)|nibble);
        if(cursor_<doc_.size()) doc_.erase(cursor_,1); char replacement=static_cast<char>(value); doc_.insert(cursor_,std::string_view(&replacement,1));
        if(highNibble_) highNibble_=false; else {highNibble_=true;++cursor_;} ensureHexVisible(); refresh();
    }
    const wchar_t* syntaxName() const {
        switch (syntax_) {
        case Syntax::Cpp: return L"C/C++"; case Syntax::CSharp: return L"C#";
        case Syntax::Json: return L"JSON"; case Syntax::Python: return L"Python";
        case Syntax::Xml: return L"XML/HTML"; case Syntax::Log: return L"Log";
        case Syntax::Asm: return L"Assembler"; default: return L"Text";
        }
    }
    void updateChrome() {
        const uint64_t end = hexMode_ ? std::min(doc_.size(), top_ + static_cast<uint64_t>(visibleRows())*16) : (lines_.empty() ? top_ : lines_.back().next);
        const uint64_t visible = end > top_ ? end - top_ : 1;
        SCROLLINFO si{}; si.cbSize = sizeof(si); si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0; si.nMax = 1000000;
        si.nPage = std::max<UINT>(1, static_cast<UINT>(1000000ULL *
            std::min(visible, std::max<uint64_t>(1, doc_.size())) / std::max<uint64_t>(1, doc_.size())));
        si.nPos = doc_.size() ? static_cast<int>(1000000ULL * top_ / doc_.size()) : 0;
        SetScrollInfo(hwnd_, SB_VERT, &si, TRUE);

        uint64_t currentLine = lines_.empty() ? 1 : lines_.front().number;
        for (const auto& line : lines_) {
            if (cursor_ >= line.start && cursor_ <= line.contentEnd) { currentLine = line.number; break; }
        }
        std::wstring pos;
        if (hexMode_) { wchar_t value[32]{}; swprintf(value, ARRAYSIZE(value), L"Offset 0x%llX", static_cast<unsigned long long>(cursor_)); pos=value; }
        else pos = L"Zeile " + std::to_wstring(currentLine) + L", Spalte " + std::to_wstring(columnAt(cursor_) + 1);
        std::wstring bytes = L"Byte " + std::to_wstring(cursor_) + L" / " + std::to_wstring(doc_.size());
        std::wstring language = syntaxAuto_ ? std::wstring(L"Auto: ") + syntaxName() : syntaxName();
        int parts[] = {210, 475, 565, 710, -1};
        SendMessageW(status_, SB_SETPARTS, ARRAYSIZE(parts), reinterpret_cast<LPARAM>(parts));
        SendMessageW(status_, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(pos.c_str()));
        SendMessageW(status_, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(bytes.c_str()));
        SendMessageW(status_, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(hexMode_ ? L"HEX" : L"UTF-8"));
        SendMessageW(status_, SB_SETTEXTW, 3, reinterpret_cast<LPARAM>(language.c_str()));
        SendMessageW(status_, SB_SETTEXTW, 4, reinterpret_cast<LPARAM>(doc_.dirty() ? L"Geaendert" : L"Gespeichert"));
    }
    int visibleRows() const {
        return std::max(1, textBottom() / std::max(1, lineHeight_));
    }
    void measure() {
        HDC dc = GetDC(hwnd_); SelectObject(dc, font_); TEXTMETRICW tm{}; GetTextMetricsW(dc, &tm);
        lineHeight_ = tm.tmHeight + tm.tmExternalLeading; charWidth_ = tm.tmAveCharWidth; ReleaseDC(hwnd_, dc);
    }
    void recreateFont() {
        HDC dc = GetDC(hwnd_);
        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(hwnd_, dc);
        HFONT replacement = CreateFontW(-MulDiv(fontPoints_, dpi, 72), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, fontName_.c_str());
        if (replacement) {
            if (font_) DeleteObject(font_);
            font_ = replacement;
        }
        measure();
        if (GetFocus() == hwnd_) {
            DestroyCaret(); CreateCaret(hwnd_, nullptr, 2, lineHeight_); ShowCaret(hwnd_);
        }
    }
    COLORREF textColor() const { return darkMode_ ? RGB(225, 225, 225) : RGB(25, 25, 25); }
    COLORREF syntaxColor(int kind) const {
        if (darkMode_) {
            static constexpr COLORREF colors[] = {RGB(225,225,225), RGB(86,156,214), RGB(206,145,120),
                                                   RGB(106,153,85), RGB(181,206,168), RGB(197,134,192)};
            return colors[std::clamp(kind, 0, 5)];
        }
        static constexpr COLORREF colors[] = {RGB(25,25,25), RGB(0,0,200), RGB(163,21,21),
                                               RGB(0,128,0), RGB(9,134,88), RGB(128,0,128)};
        return colors[std::clamp(kind, 0, 5)];
    }
    static bool identifier(wchar_t c) { return iswalnum(c) || c == L'_'; }
    static bool keyword(std::wstring_view word, Syntax syntax) {
        static constexpr std::wstring_view cpp[] = {L"auto",L"bool",L"break",L"case",L"char",L"class",L"const",L"constexpr",L"continue",L"default",L"delete",L"do",L"double",L"else",L"enum",L"false",L"float",L"for",L"if",L"int",L"namespace",L"new",L"nullptr",L"private",L"protected",L"public",L"return",L"static",L"struct",L"switch",L"template",L"this",L"true",L"try",L"using",L"void",L"while"};
        static constexpr std::wstring_view python[] = {L"and",L"as",L"assert",L"async",L"await",L"break",L"class",L"continue",L"def",L"del",L"elif",L"else",L"except",L"False",L"finally",L"for",L"from",L"global",L"if",L"import",L"in",L"is",L"lambda",L"None",L"not",L"or",L"pass",L"raise",L"return",L"True",L"try",L"while",L"with",L"yield"};
        static constexpr std::wstring_view csharp[] = {L"abstract",L"as",L"async",L"await",L"base",L"bool",L"break",L"byte",L"case",L"catch",L"char",L"class",L"const",L"continue",L"decimal",L"default",L"delegate",L"do",L"double",L"else",L"enum",L"event",L"explicit",L"extern",L"false",L"finally",L"fixed",L"float",L"for",L"foreach",L"get",L"if",L"implicit",L"in",L"int",L"interface",L"internal",L"is",L"lock",L"long",L"namespace",L"new",L"null",L"object",L"operator",L"out",L"override",L"params",L"private",L"protected",L"public",L"readonly",L"record",L"ref",L"return",L"sbyte",L"sealed",L"set",L"short",L"sizeof",L"stackalloc",L"static",L"string",L"struct",L"switch",L"this",L"throw",L"true",L"try",L"typeof",L"uint",L"ulong",L"unchecked",L"unsafe",L"ushort",L"using",L"var",L"virtual",L"void",L"volatile",L"while",L"yield"};
        static constexpr std::wstring_view assembler[] = {L"mov",L"lea",L"push",L"pop",L"call",L"ret",L"jmp",L"je",L"jne",L"jz",L"jnz",L"ja",L"jb",L"jg",L"jl",L"cmp",L"test",L"add",L"sub",L"mul",L"imul",L"div",L"idiv",L"inc",L"dec",L"and",L"or",L"xor",L"not",L"shl",L"shr",L"rol",L"ror",L"nop",L"int",L"db",L"dw",L"dd",L"dq",L"byte",L"word",L"dword",L"qword",L"section",L"segment",L"global",L"extern",L"equ",L"org",L"proc",L"endp"};
        const auto match = [word](const auto& words) { return std::find(std::begin(words), std::end(words), word) != std::end(words); };
        if (syntax == Syntax::Cpp) return match(cpp);
        if (syntax == Syntax::Python) return match(python);
        if (syntax == Syntax::CSharp) return match(csharp);
        if (syntax == Syntax::Asm) {
            std::wstring lower(word); std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
            return match(assembler) || std::find(std::begin(assembler), std::end(assembler), lower) != std::end(assembler);
        }
        return false;
    }
    void paintHighlightedLine(HDC dc, int x, int y, const std::wstring& text) const {
        if (text.empty()) return;
        std::vector<uint8_t> kinds(text.size(), 0);
        if (syntax_ == Syntax::Log) {
            int kind = text.find(L"ERROR") != std::wstring::npos || text.find(L"FATAL") != std::wstring::npos ? 2 :
                       text.find(L"WARN") != std::wstring::npos ? 4 : text.find(L"INFO") != std::wstring::npos ? 1 : 0;
            std::fill(kinds.begin(), kinds.end(), static_cast<uint8_t>(kind));
        } else if (syntax_ == Syntax::Xml) {
            bool tag = false, string = false; wchar_t quote = 0;
            for (size_t i = 0; i < text.size(); ++i) {
                if (!string && text[i] == L'<') tag = true;
                if (tag && (text[i] == L'\'' || text[i] == L'\"')) { if (!string) { string = true; quote = text[i]; } else if (text[i] == quote) string = false; }
                kinds[i] = string ? 2 : tag ? 1 : 0;
                if (!string && text[i] == L'>') tag = false;
            }
        } else if (syntax_ != Syntax::None) {
            for (size_t i = 0; i < text.size();) {
                if (((syntax_ == Syntax::Cpp || syntax_ == Syntax::CSharp) && i + 1 < text.size() && text[i] == L'/' && text[i+1] == L'/') ||
                    (syntax_ == Syntax::Python && text[i] == L'#') || (syntax_ == Syntax::Asm && text[i] == L';')) {
                    std::fill(kinds.begin() + static_cast<ptrdiff_t>(i), kinds.end(), 3); break;
                }
                if (text[i] == L'\"' || text[i] == L'\'') {
                    wchar_t quote = text[i]; size_t begin = i++;
                    while (i < text.size()) { if (text[i] == L'\\') i += std::min<size_t>(2, text.size()-i); else if (text[i++] == quote) break; else {} }
                    std::fill(kinds.begin()+static_cast<ptrdiff_t>(begin), kinds.begin()+static_cast<ptrdiff_t>(i), 2); continue;
                }
                if (iswdigit(text[i])) { size_t begin=i++; while (i<text.size() && (iswalnum(text[i]) || text[i]==L'.')) ++i;
                    std::fill(kinds.begin()+static_cast<ptrdiff_t>(begin), kinds.begin()+static_cast<ptrdiff_t>(i), 4); continue; }
                if (identifier(text[i])) { size_t begin=i++; while (i<text.size() && identifier(text[i])) ++i;
                    std::wstring_view word(text.data()+begin, i-begin);
                    bool special = keyword(word, syntax_) || (syntax_ == Syntax::Json && (word==L"true" || word==L"false" || word==L"null"));
                    if (special) std::fill(kinds.begin()+static_cast<ptrdiff_t>(begin), kinds.begin()+static_cast<ptrdiff_t>(i), 1); continue; }
                ++i;
            }
        }
        size_t begin = 0;
        while (begin < text.size()) {
            size_t end = begin + 1; while (end < text.size() && kinds[end] == kinds[begin]) ++end;
            SetTextColor(dc, syntaxColor(kinds[begin]));
            TextOutW(dc, x + static_cast<int>(begin) * charWidth_, y, text.data() + begin, static_cast<int>(end - begin));
            begin = end;
        }
        SetTextColor(dc, textColor());
    }
    static Syntax syntaxForPath(const std::wstring& path) {
        size_t dot = path.find_last_of(L'.'); std::wstring ext = dot == std::wstring::npos ? L"" : path.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), towlower);
        if (ext==L".cpp" || ext==L".c" || ext==L".h" || ext==L".hpp" || ext==L".cc") return Syntax::Cpp;
        if (ext==L".json") return Syntax::Json; if (ext==L".py" || ext==L".pyw") return Syntax::Python;
        if (ext==L".cs") return Syntax::CSharp;
        if (ext==L".asm" || ext==L".s" || ext==L".inc") return Syntax::Asm;
        if (ext==L".xml" || ext==L".html" || ext==L".htm" || ext==L".svg") return Syntax::Xml;
        if (ext==L".log") return Syntax::Log; return Syntax::None;
    }
    void rebuildLines() {
        lines_.clear(); if (!doc_.isOpen()) return;
        uint64_t pos = std::min(top_, doc_.size());
        uint64_t lineNumber = lineNumberAt(pos);
        while (static_cast<int>(lines_.size()) < visibleRows() + 1 && pos <= doc_.size()) {
            const uint64_t physicalStart = pos; std::string raw;
            while (pos < doc_.size() && raw.size() < 1024 * 1024) {
                std::string chunk = doc_.read(pos, static_cast<size_t>(std::min<uint64_t>(8192, doc_.size() - pos)));
                if (chunk.empty()) break;
                size_t nl = chunk.find('\n'); size_t take = nl == std::string::npos ? chunk.size() : nl;
                raw.append(chunk.data(), take); pos += take;
                if (nl != std::string::npos) { if (!raw.empty() && raw.back() == '\r') raw.pop_back(); ++pos; break; }
            }
            const uint64_t contentEnd = physicalStart + raw.size();
            int n = MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(raw.size()), nullptr, 0);
            std::wstring wide;
            if (n > 0) { wide.resize(n); MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(raw.size()), wide.data(), n); }
            for (auto& c : wide) if (c == L'\t') c = L' ';
            const size_t columns = wordWrap_ ? static_cast<size_t>(wrapColumns()) : std::max<size_t>(1, wide.size());
            size_t wcharOffset = 0; uint64_t byteOffset = 0; bool first = true;
            do {
                size_t take = std::min(columns, wide.size() - wcharOffset);
                std::wstring_view part(wide.data() + wcharOffset, take);
                int bytes = WideCharToMultiByte(CP_UTF8, 0, part.data(), static_cast<int>(part.size()), nullptr, 0, nullptr, nullptr);
                Line line{}; line.start = physicalStart + byteOffset;
                line.contentEnd = line.start + static_cast<uint64_t>(std::max(0, bytes));
                line.next = line.contentEnd; line.number = lineNumber; line.continuation = !first;
                line.text.assign(part); lines_.push_back(std::move(line));
                wcharOffset += take; byteOffset += static_cast<uint64_t>(std::max(0, bytes)); first = false;
                if (!wordWrap_) break;
            } while (wcharOffset < wide.size() && static_cast<int>(lines_.size()) < visibleRows() + 1);
            if (!lines_.empty()) lines_.back().next = pos;
            ++lineNumber;
            if (pos >= doc_.size()) break;
        }
    }
    uint64_t lineNumberAt(uint64_t position) const {
        if (position == numberedPosition_) return numberedLine_;
        uint64_t number = 1, pos = 0;
        std::array<char, 1024 * 1024> buffer{};
        while (pos < position) {
            size_t take = static_cast<size_t>(std::min<uint64_t>(buffer.size(), position - pos));
            size_t got = doc_.read(pos, buffer.data(), take); if (!got) break;
            number += static_cast<uint64_t>(std::count(buffer.begin(), buffer.begin() + static_cast<ptrdiff_t>(got), '\n'));
            pos += got;
        }
        numberedPosition_ = position; numberedLine_ = number;
        return number;
    }
    uint64_t lineStart(uint64_t pos) const {
        pos = std::min(pos, doc_.size()); if (!pos) return 0;
        std::array<char, 65536> b{}; uint64_t cursor = pos;
        while (cursor) {
            uint64_t begin = cursor > b.size() ? cursor - b.size() : 0;
            size_t got = doc_.read(begin, b.data(), static_cast<size_t>(cursor - begin));
            for (size_t i = got; i > 0; --i) if (b[i - 1] == '\n') return begin + i;
            cursor = begin;
        }
        return 0;
    }
    uint64_t lineEnd(uint64_t pos) const {
        pos = lineStart(pos);
        while (pos < doc_.size()) { char c{}; if (!doc_.read(pos, &c, 1) || c == '\n') break; ++pos; }
        if (pos && pos <= doc_.size()) { char c{}; doc_.read(pos - 1, &c, 1); if (c == '\r') --pos; }
        return pos;
    }
    uint64_t nextLine(uint64_t pos) const { uint64_t e = lineEnd(pos); char c{}; while (e < doc_.size() && doc_.read(e, &c, 1) && (c == '\r' || c == '\n')) { ++e; if (c == '\n') break; } return e; }
    uint64_t previousLine(uint64_t pos) const { uint64_t here = lineStart(pos); return here ? lineStart(here - 1) : 0; }
    uint64_t previousCodepoint(uint64_t pos) const {
        if (!pos) return 0; --pos; char c{};
        while (pos && doc_.read(pos, &c, 1) && (static_cast<unsigned char>(c) & 0xC0) == 0x80) --pos;
        return pos;
    }
    uint64_t nextCodepoint(uint64_t pos) const {
        if (pos >= doc_.size()) return doc_.size(); char c{}; doc_.read(pos++, &c, 1);
        while (pos < doc_.size() && doc_.read(pos, &c, 1) && (static_cast<unsigned char>(c) & 0xC0) == 0x80) ++pos;
        return pos;
    }
    int columnAt(uint64_t pos) const {
        uint64_t s = lineStart(pos); std::string bytes = doc_.read(s, static_cast<size_t>(pos - s));
        return MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    }
    uint64_t byteAtColumn(const Line& l, int col) const {
        uint64_t p = l.start; for (int i = 0; i < col && p < l.contentEnd; ++i) p = nextCodepoint(p); return p;
    }
    void moveCursorLines(int delta) {
        if (!preferredCol_) preferredCol_ = columnAt(cursor_);
        uint64_t p = cursor_; int count = abs(delta);
        while (count--) p = delta < 0 ? previousLine(p) : nextLine(p);
        Line l{p, lineEnd(p), 0, {}}; cursor_ = byteAtColumn(l, preferredCol_);
    }
    void moveTopLines(int delta) {
        uint64_t p = top_; int count = abs(delta);
        while (count--) p = delta < 0 ? previousLine(p) : nextLine(p);
        top_ = p; refresh();
    }
    void ensureCursorVisible() {
        if (cursor_ < top_) top_ = lineStart(cursor_);
        rebuildLines();
        if (!lines_.empty() && cursor_ >= lines_.back().next) {
            top_ = lineStart(cursor_); int n = visibleRows() - 1; while (n--) top_ = previousLine(top_);
        }
    }
    std::pair<int,int> cursorScreenPosition() const {
        for (size_t i = 0; i < lines_.size(); ++i)
            if (cursor_ >= lines_[i].start && cursor_ <= lines_[i].contentEnd)
                return {static_cast<int>(i), columnAt(cursor_) - columnAt(lines_[i].start)};
        return {-1, 0};
    }
    void refresh() {
        updateTitle(); InvalidateRect(hwnd_, nullptr, FALSE);
    }
    void showError(const std::wstring& e) { MessageBoxW(hwnd_, e.c_str(), L"BigPad - Fehler", MB_OK | MB_ICONERROR); }

    HWND hwnd_; HWND status_{}; Document doc_; HFONT font_{}; int lineHeight_ = 20, charWidth_ = 10;
    uint64_t top_ = 0, cursor_ = 0; int preferredCol_ = 0; std::vector<Line> lines_;
    bool showLineNumbers_ = true, wordWrap_ = false;
    bool darkMode_ = false;
    bool hexMode_ = false, highNibble_ = true;
    bool autoSaveSettings_ = true, settingsLoaded_ = false;
    bool syntaxAuto_ = true;
    Syntax syntax_ = Syntax::None;
    UINT_PTR syntaxCommand_ = kSyntaxAuto;
    std::wstring fontName_ = L"Consolas";
    int fontPoints_ = 14;
    UINT_PTR fontCommand_ = kFontConsolas, sizeCommand_ = kSize14;
    mutable uint64_t numberedPosition_ = UINT64_MAX, numberedLine_ = 1;
};

Editor* editor(HWND h) { return reinterpret_cast<Editor*>(GetWindowLongPtrW(h, GWLP_USERDATA)); }

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(new Editor(hwnd)));
        DragAcceptFiles(hwnd, TRUE);
        return 0;
    case WM_DESTROY: delete editor(hwnd); PostQuitMessage(0); return 0;
    case WM_CLOSE: if (editor(hwnd)->confirmDiscard()) DestroyWindow(hwnd); return 0;
    case WM_PAINT: editor(hwnd)->paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: editor(hwnd)->resize(); return 0;
    case WM_SETFOCUS: editor(hwnd)->focus(true); return 0;
    case WM_KILLFOCUS: editor(hwnd)->focus(false); return 0;
    case WM_MOUSEWHEEL: editor(hwnd)->wheel(GET_WHEEL_DELTA_WPARAM(wp)); return 0;
    case WM_DROPFILES: editor(hwnd)->dropFiles(reinterpret_cast<HDROP>(wp)); return 0;
    case WM_VSCROLL: { SCROLLINFO si{sizeof(si), SIF_TRACKPOS}; GetScrollInfo(hwnd, SB_VERT, &si); editor(hwnd)->vscroll(LOWORD(wp), si.nTrackPos); return 0; }
    case WM_LBUTTONDOWN: editor(hwnd)->click(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0;
    case WM_KEYDOWN:
        if (GetKeyState(VK_CONTROL) < 0) {
            if (wp == 'N') editor(hwnd)->newDocument();
            else if (wp == 'O') editor(hwnd)->openDialog();
            else if (wp == 'S') editor(hwnd)->save(GetKeyState(VK_SHIFT) < 0);
            return 0;
        }
        editor(hwnd)->key(static_cast<UINT>(wp)); return 0;
    case WM_CHAR: editor(hwnd)->character(static_cast<wchar_t>(wp)); return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) { case kNew: editor(hwnd)->newDocument(); break; case kOpen: editor(hwnd)->openDialog(); break; case kSave: editor(hwnd)->save(false); break;
        case kSaveAs: editor(hwnd)->save(true); break; case kExit: SendMessageW(hwnd, WM_CLOSE, 0, 0); break;
        case kLineNumbers: editor(hwnd)->toggleLineNumbers(); break; case kWordWrap: editor(hwnd)->toggleWordWrap(); break; }
        if (LOWORD(wp) == kLightMode) editor(hwnd)->setDarkMode(false);
        else if (LOWORD(wp) == kDarkMode) editor(hwnd)->setDarkMode(true);
        else if (LOWORD(wp) >= kFontConsolas && LOWORD(wp) <= kFontLucida) editor(hwnd)->selectFont(LOWORD(wp));
        else if (LOWORD(wp) >= kSize10 && LOWORD(wp) <= kSize28) editor(hwnd)->selectFontSize(LOWORD(wp));
        else if (LOWORD(wp) == kAbout) editor(hwnd)->showAbout();
        else if (LOWORD(wp) >= kSyntaxAuto && LOWORD(wp) <= kSyntaxAsm) editor(hwnd)->selectSyntax(LOWORD(wp));
        else if (LOWORD(wp) == kTextMode) editor(hwnd)->setHexMode(false);
        else if (LOWORD(wp) == kHexMode) editor(hwnd)->setHexMode(true);
        else if (LOWORD(wp) == kSaveSettings) editor(hwnd)->saveSettings();
        else if (LOWORD(wp) == kAutoSaveSettings) editor(hwnd)->toggleAutoSaveSettings();
        else if (LOWORD(wp) == kResetSettings) editor(hwnd)->resetSettings();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

HMENU makeMenu() {
    HMENU bar = CreateMenu(), file = CreatePopupMenu(), view = CreatePopupMenu();
    HMENU font = CreatePopupMenu(), faces = CreatePopupMenu(), sizes = CreatePopupMenu(), help = CreatePopupMenu(), syntax = CreatePopupMenu(), mode = CreatePopupMenu(), settings = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, kNew, L"&Neu\tStrg+N"); AppendMenuW(file, MF_STRING, kOpen, L"&Oeffnen...\tStrg+O");
    AppendMenuW(file, MF_STRING, kSave, L"&Speichern\tStrg+S");
    AppendMenuW(file, MF_STRING, kSaveAs, L"Speichern &unter...\tStrg+Umschalt+S"); AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, kExit, L"&Beenden"); AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&Datei");
    AppendMenuW(view, MF_STRING | MF_CHECKED, kLineNumbers, L"&Zeilennummern");
    AppendMenuW(view, MF_STRING, kWordWrap, L"&Virtueller Zeilenumbruch");
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view, MF_STRING | MF_CHECKED, kLightMode, L"&Heller Modus");
    AppendMenuW(view, MF_STRING, kDarkMode, L"&Dunkler Modus");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"&Ansicht");
    AppendMenuW(faces, MF_STRING | MF_CHECKED, kFontConsolas, L"Consolas");
    AppendMenuW(faces, MF_STRING, kFontCascadia, L"Cascadia Mono");
    AppendMenuW(faces, MF_STRING, kFontCourier, L"Courier New");
    AppendMenuW(faces, MF_STRING, kFontLucida, L"Lucida Console");
    AppendMenuW(sizes, MF_STRING, kSize10, L"10 pt"); AppendMenuW(sizes, MF_STRING, kSize12, L"12 pt");
    AppendMenuW(sizes, MF_STRING | MF_CHECKED, kSize14, L"14 pt"); AppendMenuW(sizes, MF_STRING, kSize16, L"16 pt");
    AppendMenuW(sizes, MF_STRING, kSize18, L"18 pt"); AppendMenuW(sizes, MF_STRING, kSize20, L"20 pt");
    AppendMenuW(sizes, MF_STRING, kSize24, L"24 pt"); AppendMenuW(sizes, MF_STRING, kSize28, L"28 pt");
    AppendMenuW(font, MF_POPUP, reinterpret_cast<UINT_PTR>(faces), L"&Schriftart");
    AppendMenuW(font, MF_POPUP, reinterpret_cast<UINT_PTR>(sizes), L"Schrift&groesse");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(font), L"&Schrift");
    AppendMenuW(syntax, MF_STRING | MF_CHECKED, kSyntaxAuto, L"&Automatisch");
    AppendMenuW(syntax, MF_STRING, kSyntaxNone, L"&Aus"); AppendMenuW(syntax, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(syntax, MF_STRING, kSyntaxCpp, L"C / C++"); AppendMenuW(syntax, MF_STRING, kSyntaxJson, L"JSON");
    AppendMenuW(syntax, MF_STRING, kSyntaxPython, L"Python"); AppendMenuW(syntax, MF_STRING, kSyntaxXml, L"XML / HTML");
    AppendMenuW(syntax, MF_STRING, kSyntaxLog, L"Logdatei");
    AppendMenuW(syntax, MF_STRING, kSyntaxCSharp, L"C#"); AppendMenuW(syntax, MF_STRING, kSyntaxAsm, L"Assembler");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(syntax), L"S&yntax");
    AppendMenuW(mode, MF_STRING | MF_CHECKED, kTextMode, L"&Texteditor");
    AppendMenuW(mode, MF_STRING, kHexMode, L"&Hex-Editor");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(mode), L"&Modus");
    AppendMenuW(settings, MF_STRING, kSaveSettings, L"Einstellungen jetzt &speichern");
    AppendMenuW(settings, MF_STRING | MF_CHECKED, kAutoSaveSettings, L"&Automatisch speichern");
    AppendMenuW(settings, MF_SEPARATOR, 0, nullptr); AppendMenuW(settings, MF_STRING, kResetSettings, L"&Standard wiederherstellen");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(settings), L"&Einstellungen");
    AppendMenuW(help, MF_STRING, kAbout, L"&Ueber BigPad...");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"&Hilfe"); return bar;
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int show) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc = wndProc; wc.hInstance = instance; wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_IBEAM); wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&wc)) return 1;
    HWND hwnd = CreateWindowExW(0, kClassName, L"BigPad", WS_OVERLAPPEDWINDOW | WS_VSCROLL,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1100, 750, nullptr, makeMenu(), instance, nullptr);
    if (!hwnd) return 1; ShowWindow(hwnd, show); UpdateWindow(hwnd);
    if (commandLine && *commandLine) {
        // Drag/drop and command-line opening can be added without changing the document model.
    }
    MSG msg{}; while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return static_cast<int>(msg.wParam);
}
