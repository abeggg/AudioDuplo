#include "UiRenderer.hpp"
#include <algorithm>
#include <cmath>
#include <memory>

namespace audioduplo::ui {
using namespace Gdiplus;
namespace {
std::unique_ptr<PrivateFontCollection> fonts;
std::vector<HANDLE> fontHandles;
Color Blend(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color(255, static_cast<BYTE>(a.GetR() + (b.GetR() - a.GetR()) * t),
                 static_cast<BYTE>(a.GetG() + (b.GetG() - a.GetG()) * t),
                 static_cast<BYTE>(a.GetB() + (b.GetB() - a.GetB()) * t));
}
void Line(Graphics &g, float x1, float y1, float x2, float y2, Color c, float width = 1) {
    Pen p(c, width);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    g.DrawLine(&p, x1, y1, x2, y2);
}
void Headphones(Graphics &g, float x, float y, Color c) {
    Pen p(c, 1.8f);
    p.SetStartCap(LineCapRound);
    p.SetEndCap(LineCapRound);
    g.DrawArc(&p, x + 4, y + 2, 20.0f, 22.0f, 180.0f, 180.0f);
    g.DrawLine(&p, x + 4, y + 13, x + 4, y + 21);
    g.DrawLine(&p, x + 24, y + 13, x + 24, y + 21);
    Rounded(g, {x + 2, y + 14, 6, 11}, 2, Surface(), c);
    Rounded(g, {x + 20, y + 14, 6, 11}, 2, Surface(), c);
}
void Chevron(Graphics &g, float x, float y, Color c, bool right = false) {
    if (right) {
        Line(g, x, y, x + 4, y + 4, c, 1.5f);
        Line(g, x + 4, y + 4, x, y + 8, c, 1.5f);
    } else {
        Line(g, x, y, x + 4, y + 4, c, 1.5f);
        Line(g, x + 4, y + 4, x + 8, y, c, 1.5f);
    }
}
void Gear(Graphics &g, float x, float y, Color c) {
    Pen p(c, 1.5f);
    g.DrawEllipse(&p, x + 4, y + 4, 12.0f, 12.0f);
    g.DrawEllipse(&p, x + 8, y + 8, 4.0f, 4.0f);
    for (int i = 0; i < 8; ++i) {
        float a = static_cast<float>(i) * 3.14159265f / 4;
        Line(g, x + 10 + 6 * std::cos(a), y + 10 + 6 * std::sin(a), x + 10 + 9 * std::cos(a),
             y + 10 + 9 * std::sin(a), c, 1.5f);
    }
}
void Wave(Graphics &g, float x, float y, Color c) {
    const float heights[] = {9, 19, 27, 19, 9};
    for (int i = 0; i < 5; ++i)
        Line(g, x + i * 5, y + (27 - heights[i]) / 2, x + i * 5, y + (27 + heights[i]) / 2, c, 2.4f);
}
} // namespace
PrivateFontCollection *Fonts() {
    if (!fonts) {
        fonts = std::make_unique<PrivateFontCollection>();
        for (int id : {1001, 1002}) {
            HMODULE module = GetModuleHandleW(nullptr);
            HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
            if (resource) {
                auto bytes = LoadResource(module, resource);
                DWORD count = 0;
                HANDLE handle = AddFontMemResourceEx(LockResource(bytes), SizeofResource(module, resource), nullptr, &count);
                if (handle) fontHandles.push_back(handle);
                fonts->AddMemoryFont(LockResource(bytes), SizeofResource(module, resource));
            }
        }
    }
    return fonts.get();
}
void ReleaseFonts() { fonts.reset(); for (auto handle : fontHandles) RemoveFontMemResourceEx(handle); fontHandles.clear(); }
float Ease(float t) {
    if (t <= 0)
        return 0;
    if (t >= 1)
        return 1;
    // cubic-bezier(0.23, 1, 0.32, 1), solved by bisection.
    t = std::clamp(t, 0.0f, 1.0f);
    float lo = 0, hi = 1, u = t;
    for (int i = 0; i < 14; ++i) {
        u = (lo + hi) / 2;
        float x = 3 * (1 - u) * (1 - u) * u * 0.23f + 3 * (1 - u) * u * u * 0.32f + u * u * u;
        if (x < t)
            lo = u;
        else
            hi = u;
    }
    return 1 - std::pow(1 - u, 3.0f);
}
std::vector<Item> Items(const View &v) {
    const float w = static_cast<float>(v.width), body = w - 64;
    if (v.settings)
        return {{Back, {26, 24, 100, 40}},
                {Stability, {32, 184, body, 64}},
                {WindowsSound, {32, 264, body, 58}},
                {Diagnostics, {32, 322, body, 58}},
                {Tray, {32, 380, body, 58}}};
    return {{Settings, {w - 130, 24, 104, 40}},
            {Source, {32, 196, body, 86}},
            {Destination, {32, 282, body, 86}},
            {Volume, {32, 469, body, 40}},
            {Start, {32, 540, body, 54}}};
}
void Text(Graphics &g, const std::wstring &s, RectF r, float size, Color color, bool bold,
          StringAlignment alignment, bool wrap) {
    FontFamily family(L"Nunito", Fonts());
    Font font(&family, size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    SolidBrush brush(color);
    StringFormat format;
    format.SetAlignment(alignment);
    format.SetLineAlignment(StringAlignmentCenter);
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    format.SetFormatFlags(wrap ? 0 : StringFormatFlagsNoWrap);
    g.DrawString(s.c_str(), static_cast<INT>(s.size()), &font, r, &format, &brush);
}
void Rounded(Graphics &g, RectF r, float radius, Color fill, Color edge) {
    float d = radius * 2;
    GraphicsPath p;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    p.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    p.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    p.CloseFigure();
    SolidBrush b(fill);
    g.FillPath(&b, &p);
    if (edge.GetA()) {
        Pen pen(edge, 1);
        g.DrawPath(&pen, &p);
    }
}
void DrawControl(Graphics &g, const View &v, int id, RectF r, float hover, float pressed, bool focused) {
    const Color edge(56, 65, 76), hovered(42, 49, 58);
    const bool disabled = (id == Start && !v.canStart && !v.running) ||
                          ((id == Source || id == Destination || id == Stability) && v.running);
    Color color = disabled ? Color(139, 149, 164) : Ink();
    if (disabled) { hover = 0; pressed = 0; }
    if (id == Start) {
        float inset = r.Width * 0.01f * pressed;
        r = RectF(r.X + inset, r.Y + pressed, r.Width - 2 * inset, r.Height - 2 * pressed);
    }
    if (id == Source || id == Destination) {
        // One joined route surface; each native button owns its row and keyboard semantics.
        GraphicsState saved = g.Save();
        g.SetClip(r, CombineModeIntersect);
        RectF whole(r.X, id == Source ? r.Y : r.Y - 86, r.Width, 172);
        float expanded = v.expandedControl == id ? v.expansion : 0;
        Rounded(g, whole, 16, Blend(Surface(), hovered, std::max(hover, expanded * 0.65f)),
                Blend(edge, Accent(), expanded * 0.7f));
        if (id == Source)
            Line(g, r.X + 22, r.GetBottom() - 0.5f, r.GetRight() - 22, r.GetBottom() - 0.5f, edge);
        Rounded(g, {r.X + 20, r.Y + 22, 42, 42}, 12, Color(39, 46, 55), Color(0, 0, 0, 0));
        Headphones(g, r.X + 27, r.Y + 28, Accent());
        const auto &name = id == Source ? v.source : v.destination;
        Text(g, id == Source ? L"Som do PC" : L"Compartilhar com", {r.X + 80, r.Y + 14, r.Width - 126, 22},
             12, Muted());
        Text(g, name.empty() ? L"Selecionar dispositivo" : name, {r.X + 80, r.Y + 37, r.Width - 126, 30}, 17,
             name.empty() ? Muted() : color);
        float open = v.expandedControl == id ? v.expansion : 0;
        auto arrow = g.Save();
        g.TranslateTransform(r.GetRight() - 26, r.Y + 43);
        g.RotateTransform(180 * open);
        Chevron(g, -4, -2, open > 0 ? Accent() : Muted());
        g.Restore(arrow);
        g.Restore(saved);
    } else if (id == Start) {
        Color fill = disabled ? Color(47, 55, 66) : v.running ? Color(44, 57, 73) : Color(232, 239, 250);
        if (hover > 0 && !disabled)
            fill = Blend(fill, v.running ? Color(54, 72, 94) : Color(255, 255, 255), hover);
        if (pressed && !disabled)
            fill = Blend(fill, Color(130, 151, 184), 0.25f * pressed);
        Rounded(g, {r.X + 1, r.Y + 1, r.Width - 2, r.Height - 2}, 12, fill,
                v.running ? Color(85, 110, 143) : Color(0, 0, 0, 0));
        if (hover > 0 && !disabled) {
            auto sweep = g.Save();
            g.SetClip(RectF(r.X, r.Y, r.Width * hover, r.Height), CombineModeIntersect);
            Rounded(g, {r.X + 1, r.Y + 1, r.Width - 2, r.Height - 2}, 12,
                    v.running ? Color(54, 76, 105) : Blend(Color(196, 218, 250), Color(151, 184, 230), pressed),
                    Color(0, 0, 0, 0));
            g.Restore(sweep);
        }
        const std::wstring label =
            v.running ? (v.ready ? L"Parar compartilhamento" : L"Cancelar conexão") : L"Compartilhar áudio";
        Color fg = disabled ? Color(148, 159, 175) : v.running ? Ink() : Color(27, 35, 47);
        Text(g, label, {r.X + 20, r.Y, r.Width - 40, r.Height}, 15, fg, true, StringAlignmentCenter);
    } else if (id == Volume) {
        Line(g, r.X + 9, r.Y + 20, r.GetRight() - 9, r.Y + 20, Color(61, 70, 82), 4);
        float x = r.X + 9 + (r.Width - 18) * static_cast<float>(v.volume) / 100;
        if (v.volume > 0)
            Line(g, r.X + 9, r.Y + 20, x, r.Y + 20, Accent(), 4);
        SolidBrush b(Ink());
        g.FillEllipse(&b, x - 6, r.Y + 14, 12.0f, 12.0f);
        if (focused) {
            Pen p(Accent(), 1);
            g.DrawEllipse(&p, x - 10, r.Y + 10, 20.0f, 20.0f);
        }
    } else if (id == Settings || id == Back) {
        if (hover > 0 || pressed)
            Rounded(g, {r.X + 1, r.Y + 1, r.Width - 2, r.Height - 2}, 9,
                    Blend(Shell(), hovered, std::max(pressed, hover)), Color(0, 0, 0, 0));
        if (id == Settings) {
            Gear(g, r.X + 11, r.Y + 10, Muted());
            Text(g, L"Ajustes", {r.X + 38, r.Y, r.Width - 42, r.Height}, 13, Ink());
        } else {
            Line(g, r.X + 18, r.Y + 20, r.X + 26, r.Y + 20, Muted(), 1.5f);
            Line(g, r.X + 18, r.Y + 20, r.X + 22, r.Y + 16, Muted(), 1.5f);
            Line(g, r.X + 18, r.Y + 20, r.X + 22, r.Y + 24, Muted(), 1.5f);
            Text(g, L"Voltar", {r.X + 36, r.Y, r.Width - 38, r.Height}, 13, Ink());
        }
    } else {
        Rounded(g, {r.X + 1, r.Y + 1, r.Width - 2, r.Height - 2}, 10,
                Blend(Surface(), hovered, pressed ? 1 : hover), Color(0, 0, 0, 0));
        std::wstring title = id == Stability      ? L"Estabilidade do áudio"
                             : id == WindowsSound ? L"Som do Windows"
                             : id == Diagnostics  ? L"Copiar diagnóstico"
                                                  : L"Continuar na bandeja";
        if (id == Stability) {
            Text(g, title, {r.X + 18, r.Y + 8, r.Width - 55, 26}, 14, color);
            const wchar_t *choices[] = {L"Automática", L"20 ms", L"30 ms",  L"40 ms",
                                        L"60 ms",      L"80 ms", L"120 ms", L"160 ms"};
            Text(g, choices[std::clamp(v.bufferIndex, 0, 7)], {r.X + 18, r.Y + 34, r.Width - 55, 22}, 12,
                 disabled ? Muted() : Accent());
        } else
            Text(g, title, {r.X + 18, r.Y, r.Width - 55, r.Height}, 14, color);
        Chevron(g, r.GetRight() - 27, r.Y + r.Height / 2 - 4, Muted(), id != Stability);
    }
    if (focused && id != Volume) {
        Pen pen(Accent(), 2);
        GraphicsPath p;
        RectF f(r.X + 4, r.Y + 4, r.Width - 8, r.Height - 8);
        const float d = 14;
        p.AddArc(f.X, f.Y, d, d, 180, 90);
        p.AddArc(f.GetRight() - d, f.Y, d, d, 270, 90);
        p.AddArc(f.GetRight() - d, f.GetBottom() - d, d, d, 0, 90);
        p.AddArc(f.X, f.GetBottom() - d, d, d, 90, 90);
        p.CloseFigure();
        g.DrawPath(&pen, &p);
    }
}
void Render(Graphics &g, const View &v, bool withControls) {
    g.Clear(Shell());
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    const float w = static_cast<float>(v.width), body = w - 64;
    if (v.settings) {
        Text(g, L"Ajustes", {32, 92, body, 44}, 28, Ink(), true);
        Text(g, L"O essencial fica automático. Ajuste quando precisar.", {32, 137, body, 40}, 13, Muted(),
             false, StringAlignmentNear, true);
        Text(g, L"Como usar", {32, 465, body, 27}, 14, Ink(), true);
        Text(g,
             L"Escolha a saída que já recebe o som do seu PC e um segundo fone. Reproduza música, vídeo ou "
             L"jogo e compartilhe.",
             {32, 500, body, 58}, 14, Muted(), false, StringAlignmentNear, true);
        Text(g, L"AudioDuplo 1.1.4", {32, 601, body, 23}, 12, Muted());
        if (!v.message.empty())
            Text(g, v.message, {32, 564, body, 32}, 12, v.error ? Color(244, 174, 161) : Accent(), false,
                 StringAlignmentNear, true);
    } else {
        Wave(g, 34, 30, Accent());
        Text(g, L"AudioDuplo", {69, 27, 210, 34}, 18, Ink(), true);
        Text(g, L"Ouça simultaneamente", {32, 97, body, 47}, HeadingSize(v.width), Ink(), true);
        Text(g, L"O mesmo som em dois fones.", {33, 148, body, 25}, 15, Muted());
        std::wstring message = v.message.empty()
                                   ? (v.ready ? L"Compartilhando áudio. Para trocar os fones, pare primeiro."
                                      : v.running ? L"Conectando os dispositivos…"
                                                  : L"Reproduza algo no primeiro dispositivo e compartilhe.")
                                   : v.message;
        Text(g, message, {33, 385, body - 2, 40}, 13, v.error ? Color(244, 174, 161) : Muted(), false,
             StringAlignmentNear, true);
        Text(g, L"Volume do segundo fone", {32, 438, body - 70, 27}, 13, Ink());
        Text(g, std::to_wstring(v.volume) + L"%", {w - 100, 438, 68, 27}, 13, Muted(), false,
             StringAlignmentFar);
        Text(g, L"Sua saída padrão continua a mesma.", {32, 608, body, 22}, 12, Muted(), false,
             StringAlignmentCenter);
    }
    if (withControls)
        for (const auto &item : Items(v))
            DrawControl(g, v, item.id, item.rect);
}
} // namespace audioduplo::ui
