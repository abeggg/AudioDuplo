#include "ControlSurface.hpp"

namespace audioduplo::ui {
std::optional<LRESULT> PaintControlSurface(HWND window, UINT message, WPARAM parameter,
                                           SurfacePainter painter, void *context) {
    switch (message) {
    case WM_ERASEBKGND:
        // The complete background is already in the frame. Never flash a native light brush.
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        if (dc)
            painter(window, dc, context);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_PRINTCLIENT:
        if (parameter)
            painter(window, reinterpret_cast<HDC>(parameter), context);
        return 0;
    default:
        return std::nullopt;
    }
}
void SetControlEnabled(HWND window, bool enabled) {
    // EnableWindow(FALSE) sends WM_CANCELMODE even when already disabled.
    if ((IsWindowEnabled(window) != FALSE) != enabled)
        EnableWindow(window, enabled);
}
} // namespace audioduplo::ui
