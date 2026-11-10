#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <vector>

namespace audioduplo::ui {
enum ControlId {
    Source = 101,
    Destination,
    Volume,
    Start,
    Settings,
    Back,
    Stability,
    WindowsSound,
    Diagnostics,
    Tray
};
void ReleaseFonts();
Gdiplus::PrivateFontCollection *Fonts();
struct View {
    int expandedControl{};
    float expansion{};
    int width{620}, volume{100}, bufferIndex{0};
    bool settings{false}, running{false}, ready{false}, canStart{false}, error{false};
    std::wstring source, destination, message;
    bool operator==(const View &) const = default;
};
struct Item {
    int id;
    Gdiplus::RectF rect;
};
constexpr int ContentHeight = 650;
inline float HeadingSize(int width) {
    return width >= 580 ? 30.0f : width >= 480 ? 26.0f : 22.0f;
}
inline Gdiplus::Color Shell() {
    return Gdiplus::Color(23, 26, 31);
}
inline Gdiplus::Color Surface() {
    return Gdiplus::Color(32, 37, 44);
}
inline Gdiplus::Color Ink() {
    return Gdiplus::Color(242, 245, 247);
}
inline Gdiplus::Color Muted() {
    return Gdiplus::Color(166, 175, 188);
}
inline Gdiplus::Color Accent() {
    return Gdiplus::Color(171, 201, 248);
}
std::vector<Item> Items(const View &view);
void Text(Gdiplus::Graphics &g, const std::wstring &text, Gdiplus::RectF r, float size, Gdiplus::Color color,
          bool bold = false, Gdiplus::StringAlignment alignment = Gdiplus::StringAlignmentNear,
          bool wrap = false);
void Rounded(Gdiplus::Graphics &g, Gdiplus::RectF r, float radius, Gdiplus::Color fill, Gdiplus::Color edge);
void DrawControl(Gdiplus::Graphics &g, const View &view, int id, Gdiplus::RectF rect, float hover = 0,
                 float pressed = 0, bool focused = false);
void Render(Gdiplus::Graphics &g, const View &view, bool withControls = true);
float Ease(float progress);
} // namespace audioduplo::ui
