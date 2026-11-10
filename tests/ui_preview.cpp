#include "UiRenderer.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace ui = audioduplo::ui;
void Check(bool ok, const char *name) {
    if (!ok)
        throw std::runtime_error(name);
}
void Test() {
    Check(ui::Ease(0) == 0 && ui::Ease(1) == 1, "animation endpoints");
    float last = 0;
    for (int i = 0; i <= 1000; ++i) {
        float now = ui::Ease(static_cast<float>(i) / 1000);
        Check(now >= last && now <= 1, "monotonic animation");
        last = now;
    }
    for (int width : {420, 480, 620})
        for (bool settings : {false, true}) {
            ui::View view;
            view.width = width;
            view.settings = settings;
            auto items = ui::Items(view);
            Check(items.size() == 5u, "control count");
            for (size_t i = 0; i < items.size(); ++i) {
                auto r = items[i].rect;
                Check(r.X >= 0 && r.Y >= 0 && r.GetRight() <= width && r.GetBottom() <= ui::ContentHeight,
                      "layout bounds");
                Check(r.Height >= 40, "minimum target size");
                for (size_t j = i + 1; j < items.size(); ++j) {
                    Gdiplus::RectF overlap;
                    Gdiplus::RectF::Intersect(overlap, r, items[j].rect);
                    Check(overlap.Width <= 0 || overlap.Height <= 0, "controls must not overlap");
                }
            }
        }
    std::cout << "PASS: layout widths, control counts, hit targets, nonoverlap and easing\n";
}
int Encoder(CLSID &id) {
    UINT count = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&count, &size);
    std::vector<BYTE> data(size);
    auto codecs = reinterpret_cast<Gdiplus::ImageCodecInfo *>(data.data());
    Gdiplus::GetImageEncoders(count, size, codecs);
    for (UINT i = 0; i < count; ++i)
        if (wcscmp(codecs[i].MimeType, L"image/png") == 0) {
            id = codecs[i].Clsid;
            return 0;
        }
    return 1;
}
void TextFitTest() {
    Gdiplus::Bitmap bitmap(620, ui::ContentHeight);
    Gdiplus::Graphics graphics(&bitmap);
    Gdiplus::FontFamily family(L"Nunito", ui::Fonts());
    for (int width : {420, 480, 620}) {
        Gdiplus::Font font(&family, ui::HeadingSize(width), Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::RectF measured;
        Check(graphics.MeasureString(L"Ouça simultaneamente", -1, &font, Gdiplus::PointF(0, 0), &measured) ==
                  Gdiplus::Ok,
              "measure heading");
        Check(measured.Width <= width - 64 && measured.Height <= 47, "heading fits at supported widths");
    }
}
int wmain(int argc, wchar_t **argv) {
    try {
        Test();
        ULONG_PTR token{};
        Gdiplus::GdiplusStartupInput startup;
        Check(Gdiplus::GdiplusStartup(&token, &startup, nullptr) == Gdiplus::Ok, "GDI+ startup");
        TextFitTest();
        if (argc < 2 || std::wstring(argv[1]) == L"--test") {
            ui::ReleaseFonts();
            Gdiplus::GdiplusShutdown(token);
            return 0;
        }
        {
            std::filesystem::path directory(argv[1]);
            std::filesystem::create_directories(directory);
            CLSID encoder{};
            Check(!Encoder(encoder), "PNG encoder");
            for (int scene = 0; scene < 6; ++scene) {
                ui::View view;
                view.source = L"Fone principal";
                view.destination = L"Fone Bluetooth";
                view.canStart = true;
                view.volume = 80;
                std::wstring name = L"main";
                float scale = 1.5f;
                if (scene == 1) {
                    view.running = true;
                    view.ready = true;
                    name = L"sharing";
                }
                if (scene == 2) {
                    view.settings = true;
                    name = L"settings";
                }
                if (scene == 3) {
                    view.source.clear();
                    view.destination.clear();
                    view.canStart = false;
                    view.width = 420;
                    view.message = L"Conecte dois dispositivos ao Windows e selecione acima.";
                    name = L"empty-compact";
                }
                if (scene == 4) {
                    view.source = L"Fones de ouvido USB com nome muito longo para testar truncamento";
                    view.destination = L"Outro dispositivo com identificação muito longa";
                    view.width = 480;
                    view.error = true;
                    view.message = L"Não foi possível conectar. Confira os dispositivos e tente novamente.";
                    name = L"error-long-names";
                    scale = 2;
                }
                if (scene == 5) {
                    view.settings = true;
                    view.width = 420;
                    name = L"settings-compact";
                    scale = 1.25f;
                }
                Gdiplus::Bitmap image(static_cast<INT>(view.width * scale),
                                      static_cast<INT>(ui::ContentHeight * scale), PixelFormat32bppARGB);
                {
                    Gdiplus::Graphics graphics(&image);
                    graphics.ScaleTransform(scale, scale);
                    ui::Render(graphics, view);
                }
                const auto file = directory / (name + L".png");
                Check(image.Save(file.c_str(), &encoder, nullptr) == Gdiplus::Ok, "save preview");
            }
        }
        ui::ReleaseFonts();
        Gdiplus::GdiplusShutdown(token);
        std::cout << "Rendered six views without creating a window or accessing audio devices.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
