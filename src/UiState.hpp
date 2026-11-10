#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace audioduplo::ui {
// Choosing the endpoint in the other field swaps the route instead of rejecting the click.
inline void SelectEndpoint(std::wstring &source, std::wstring &destination, bool selectingSource,
                           const std::wstring &selected) {
    if (selected.empty())
        return;
    auto &current = selectingSource ? source : destination;
    auto &other = selectingSource ? destination : source;
    if (other == selected)
        std::swap(current, other);
    else
        current = selected;
}
inline int VolumeAt(float x, float width, float inset) {
    if (width <= 2 * inset)
        return 0;
    return std::clamp(static_cast<int>(std::lround((x - inset) * 100 / (width - 2 * inset))), 0, 100);
}
} // namespace audioduplo::ui
