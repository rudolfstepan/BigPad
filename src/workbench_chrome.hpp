#pragma once
#include <functional>
#include <windows.h>

bool installWorkbenchDocumentScrollbar(HWND window, std::function<void(int, int)> scroll);
bool installWorkbenchStatus(HWND status);
