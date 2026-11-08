#include "workbench/window_frame.hpp"
#include "workbench/workbench_menu.hpp"

#include <commctrl.h>

#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

int main() {
    try {
        SetProcessDPIAware();
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
        InitCommonControlsEx(&controls);
        WNDCLASSW type{};
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpfnWndProc = DefWindowProcW;
        type.lpszClassName = L"WorkbenchMenuTest";
        require(RegisterClassW(&type), "Register test window");
        HMENU bar = CreateMenu();
        HMENU file = CreatePopupMenu();
        AppendMenuW(file, MF_STRING, 100, L"Open");
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"File");
        HWND window = CreateWindowExW(0, type.lpszClassName, L"Test", WS_OVERLAPPEDWINDOW,
                                      100,100,700,500,nullptr,bar,type.hInstance,nullptr);
        require(window != nullptr, "Create hidden test window");
        require(retro_frame::install(window,{RGB(0,65,175),RGB(255,255,255),RGB(170,170,170),RGB(100,100,100)}), "Install frame");
        require(workbench::installMenu(window), "Install menu");
        const UINT dpi = GetDpiForWindow(window);
        const int border = MulDiv(4,dpi,96);
        const int height = MulDiv(28,dpi,96);
        RECT bounds{};
        GetWindowRect(window,&bounds);
        const LPARAM point = MAKELPARAM(bounds.left+border+10,bounds.top+border+height+height/2);
        require(SendMessageW(window,WM_NCHITTEST,0,point)==HTBORDER,
                "Custom menu must not activate native Windows menu tracking");
        require(SendMessageW(window,WM_NCMOUSEMOVE,HTBORDER,point)==0,
                "Hover stays inside custom menu handling");
        POINT client{};
        ClientToScreen(window,&client);
        require(client.x==bounds.left+border && client.y==bounds.top+border+height*2,
                "Painted menu and client origin share DPI-aware frame coordinates");
        DestroyWindow(window);
        std::cout << "PASS menu hover isolation and DPI-aware hit geometry\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
