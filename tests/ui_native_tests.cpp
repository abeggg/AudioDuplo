#include "ControlSurface.hpp"
#include "UiState.hpp"
#include <commctrl.h>
#include <array>
#include <iostream>
#include <stdexcept>
#ifdef _MSC_VER
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' "       \
                        "version='6.0.0.0' processorArchitecture='*' "                                       \
                        "publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

namespace ui = audioduplo::ui;
namespace {
int checks = 0;
void Check(bool condition, const char *label) {
    ++checks;
    if (!condition)
        throw std::runtime_error(label);
}
struct Probe {
    int paints{}, cancellations{}, enableChanges{};
};
void Paint(HWND window, HDC dc, void *context) {
    ++static_cast<Probe *>(context)->paints;
    RECT bounds{};
    GetClientRect(window, &bounds);
    HBRUSH fill = CreateSolidBrush(RGB(23, 26, 31));
    FillRect(dc, &bounds, fill);
    DeleteObject(fill);
}
LRESULT CALLBACK ControlProc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR ref) {
    auto &probe = *reinterpret_cast<Probe *>(ref);
    if (message == WM_CANCELMODE)
        ++probe.cancellations;
    if (message == WM_ENABLE)
        ++probe.enableChanges;
    if (auto value = ui::PaintControlSurface(window, message, wp, Paint, &probe))
        return *value;
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(window, ControlProc, id);
    return DefSubclassProc(window, message, wp, lp);
}
void NativeSurfaceTest(const wchar_t *className, DWORD style, bool slider) {
    // Message-only parent: no desktop window, activation, global input, preferences or audio.
    HWND parent = CreateWindowExW(0, L"STATIC", L"AudioDuplo isolated test", 0, 0, 0, 120, 40, HWND_MESSAGE,
                                  nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(parent != nullptr, "message-only parent");
    HWND child = CreateWindowExW(0, className, L"Control", WS_CHILD | style, 0, 0, 120, 40, parent, nullptr,
                                 GetModuleHandleW(nullptr), nullptr);
    Check(child != nullptr, "native control");
    Check(!IsWindowVisible(parent) && !IsWindowVisible(child), "test surfaces stay invisible");
    Probe probe;
    Check(SetWindowSubclass(child, ControlProc, 1, reinterpret_cast<DWORD_PTR>(&probe)) != FALSE,
          "install paint adapter");
    HDC memory = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 120;
    info.bmiHeader.biHeight = -40;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *pixels{};
    HBITMAP bitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    Check(bitmap && pixels, "memory paint surface");
    auto previous = SelectObject(memory, bitmap);
    PatBlt(memory, 0, 0, 120, 40, WHITENESS);
    // Without the adapter, native controls can paint their own background. With it, erasure is inert.
    Check(SendMessageW(child, WM_ERASEBKGND, reinterpret_cast<WPARAM>(memory), 0) == 1,
          "background erase handled");
    Check(GetPixel(memory, 10, 10) == RGB(255, 255, 255) && probe.paints == 0,
          "erase does not paint an intermediate frame");
    for (int iteration = 0; iteration < 12; ++iteration) {
        SendMessageW(child, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(memory), PRF_CLIENT);
        Check(GetPixel(memory, 0, 0) == RGB(23, 26, 31) && GetPixel(memory, 119, 39) == RGB(23, 26, 31),
              "control paints a complete dark frame through the shared adapter");
    }
    Check(probe.paints == 12, "exactly one paint per request");
    InvalidateRect(child, nullptr, TRUE);
    SendMessageW(child, WM_PAINT, 0, 0);
    Check(probe.paints == 13, "native paint message uses the same surface adapter");
    Check(!GetUpdateRect(child, nullptr, FALSE), "paint validates the region without a repaint loop");
    ui::SetControlEnabled(child, false);
    probe.cancellations = probe.enableChanges = 0;
    for (int iteration = 0; iteration < 40; ++iteration)
        ui::SetControlEnabled(child, false);
    Check(probe.cancellations == 0 && probe.enableChanges == 0,
          "idle disabled controls receive no cancellation or enable messages");
    ui::SetControlEnabled(child, true);
    Check(IsWindowEnabled(child) && probe.enableChanges == 1, "one state change enables control");
    if (slider) {
        SendMessageW(child, TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
        SendMessageW(child, TBM_SETPOS, FALSE, 73);
        Check(SendMessageW(child, TBM_GETPOS, 0, 0) == 73, "native volume value retained");
        SendMessageW(child, WM_KEYDOWN, VK_RIGHT, 0);
        Check(SendMessageW(child, TBM_GETPOS, 0, 0) == 74, "native volume keyboard behavior retained");
    } else {
        SendMessageW(child, BM_SETSTATE, TRUE, 0);
        Check((SendMessageW(child, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0,
              "native button press state retained");
        SendMessageW(child, BM_SETSTATE, FALSE, 0);
        Check((SendMessageW(child, BM_GETSTATE, 0, 0) & BST_PUSHED) == 0,
              "native button release state retained");
    }
    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    DestroyWindow(parent);
}
void StateTest() {
    std::wstring source = L"usb", destination = L"bluetooth";
    ui::SelectEndpoint(source, destination, true, L"bluetooth");
    Check(source == L"bluetooth" && destination == L"usb", "selecting other device swaps source");
    ui::SelectEndpoint(source, destination, false, L"bluetooth");
    Check(source == L"usb" && destination == L"bluetooth", "selecting other device swaps destination");
    ui::SelectEndpoint(source, destination, false, L"hdmi");
    Check(source == L"usb" && destination == L"hdmi", "selecting a third device preserves source");
    ui::SelectEndpoint(source, destination, true, L"");
    Check(source == L"usb" && destination == L"hdmi", "empty choice preserves route");
    for (float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
        Check(ui::VolumeAt(9 * scale, 556 * scale, 9 * scale) == 0, "volume left endpoint");
        Check(ui::VolumeAt(547 * scale, 556 * scale, 9 * scale) == 100, "volume right endpoint");
        Check(ui::VolumeAt(278 * scale, 556 * scale, 9 * scale) == 50, "volume midpoint");
        Check(ui::VolumeAt(-100 * scale, 556 * scale, 9 * scale) == 0, "drag beyond left clamped");
        Check(ui::VolumeAt(700 * scale, 556 * scale, 9 * scale) == 100, "drag beyond right clamped");
    }
}
} // namespace
int main() {
    try {
        INITCOMMONCONTROLSEX classes{sizeof(classes), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES};
        Check(InitCommonControlsEx(&classes) != FALSE, "native controls initialized");
        NativeSurfaceTest(L"BUTTON", BS_PUSHBUTTON, false);
        NativeSurfaceTest(TRACKBAR_CLASSW, TBS_NOTICKS, true);
        StateTest();
        std::cout << "PASS: " << checks
                  << " paint, idle, native control, route and volume checks; "
                     "no visible window or audio device access.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
