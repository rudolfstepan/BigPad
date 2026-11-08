#pragma once
#include "platform.hpp"

#include <algorithm>

namespace workbench {
struct Scrollbar {
    RECT track;
    RECT thumb;
    RECT up;
    RECT down;
    int maximum;
};

inline Scrollbar verticalScrollbar(RECT bounds, int total, int page, int position) {
    const int arrow = std::min(22L, std::max(0L, (bounds.bottom - bounds.top) / 3));
    Scrollbar result{};
    result.maximum = std::max(0, total - std::max(1, page));
    result.down = {bounds.left, bounds.bottom - arrow, bounds.right, bounds.bottom};
    result.up = {bounds.left, result.down.top - arrow, bounds.right, result.down.top};
    result.track = {bounds.left, bounds.top, bounds.right, result.up.top};
    const int height = result.track.bottom - result.track.top;
    const int thumbHeight =
        total <= page
            ? height
            : std::min(height, std::max(20, MulDiv(height, std::max(1, page), std::max(1, total))));
    const int offset =
        result.maximum
            ? MulDiv(height - thumbHeight, std::clamp(position, 0, result.maximum), result.maximum)
            : 0;
    result.thumb = {bounds.left, bounds.top + offset, bounds.right,
                    bounds.top + offset + thumbHeight};
    return result;
}

inline int rowFromThumb(const Scrollbar& bar, int cursor, int grabOffset) {
    const int travel = (bar.track.bottom - bar.track.top) - (bar.thumb.bottom - bar.thumb.top);
    return travel > 0 ? MulDiv(std::clamp<int>(cursor - grabOffset - bar.track.top, 0, travel),
                               bar.maximum, travel)
                      : 0;
}
} // namespace workbench
