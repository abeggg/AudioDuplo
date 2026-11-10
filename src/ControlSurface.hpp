#pragma once
#include <windows.h>
#include <optional>

namespace audioduplo::ui {
using SurfacePainter = void (*)(HWND window, HDC dc, void *context);

// Keep native input/accessibility, but give every control the same buffered paint path.
std::optional<LRESULT> PaintControlSurface(HWND window, UINT message, WPARAM parameter,
                                           SurfacePainter painter, void *context);
void SetControlEnabled(HWND window, bool enabled);
} // namespace audioduplo::ui
