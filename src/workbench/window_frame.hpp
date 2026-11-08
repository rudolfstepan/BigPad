#pragma once

#include "platform.hpp"

namespace retro_frame {
struct Colors {
    COLORREF title;
    COLORREF text;
    COLORREF border;
    COLORREF inactive;
};

// Keeps the native client area and sizing semantics, paints the non-client frame.
// The subclass owns its state until WM_NCDESTROY. Call on the window's UI thread.
bool install(HWND window, Colors colors);
} // namespace retro_frame
