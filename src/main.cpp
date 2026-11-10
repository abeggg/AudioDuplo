#include "AudioEngine.hpp"
#include "ControlSurface.hpp"
#include "Preferences.hpp"
#include "UiRenderer.hpp"
#include "UiState.hpp"
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <sstream>

namespace {
using namespace audioduplo;
namespace design = audioduplo::ui;
using Gdiplus::Bitmap;
using Gdiplus::Graphics;
constexpr wchar_t ClassName[] = L"AudioDuplo.Desktop.v1.1.4";
constexpr UINT TrayMessage = WM_APP + 10, RevealMessage = WM_APP + 11;
const LatencyChoice Latencies[] = {LatencyChoice::Automatic, LatencyChoice::Ms20, LatencyChoice::Ms30,
                                   LatencyChoice::Ms40,      LatencyChoice::Ms60, LatencyChoice::Ms80,
                                   LatencyChoice::Ms120,     LatencyChoice::Ms160};
struct App;
struct Control {
    App *app{};
    HWND hwnd{};
    int id{};
    float hover{}, from{}, target{};
    float press{}, pressFrom{}, pressTarget{};
    ULONGLONG pressStart{};
    ULONGLONG started{};
    bool tracking{}, dragging{};
    std::wstring label;
};
struct MenuItem {
    std::wstring label;
    bool selected{}, disabled{};
};
struct App {
    HWND window{};
    UINT dpi{96};
    AudioEngine engine;
    Preferences prefs;
    design::View view;
    std::vector<DeviceInfo> devices;
    std::array<Control, 10> controls{};
    std::unique_ptr<Bitmap> frame, previous;
    ULONGLONG transitionStart{}, noticeUntil{};
    bool reduced{}, keyboard{}, dirty{true}, picking{}, inTray{}, layoutBusy{};
    int pixelWidth{620}, pixelHeight{650}, offsetX{}, scrollY{}, ticks{};
    std::wstring notice;
    float expansionFrom{}, expansionTarget{};
    ULONGLONG expansionStart{};
    int px(int n) const {
        return MulDiv(n, static_cast<int>(dpi), 96);
    }
    HWND control(int id) {
        for (auto &c : controls)
            if (c.id == id)
                return c.hwnd;
        return nullptr;
    }
    float scale() const {
        return static_cast<float>(dpi) / 96;
    }
};
void Dirty(App &a) {
    a.dirty = true;
    RedrawWindow(a.window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
}
void Save(App &a) {
    a.prefs.volume = a.view.volume;
    a.prefs.latencyIndex = a.view.bufferIndex;
    if (!a.prefs.save()) {
        a.notice = L"Não foi possível salvar os ajustes. Tente novamente ao fechar.";
        a.noticeUntil = GetTickCount64() + 5000;
    }
}
void Label(App &a, int id, const std::wstring &text) {
    auto &c = a.controls[static_cast<size_t>(id - design::Source)];
    if (c.label != text) {
        c.label = text;
        a.dirty = true;
        SetWindowTextW(c.hwnd, text.c_str());
    }
}
const DeviceInfo *Device(const App &a, const std::wstring &id) {
    auto i = std::find_if(a.devices.begin(), a.devices.end(), [&](const auto &d) { return d.id == id; });
    return i == a.devices.end() ? nullptr : &*i;
}
void MotionPreference(App &a) {
    BOOL enabled = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0);
    a.reduced = !enabled;
}
void Layout(App &a) {
    if (a.layoutBusy || !a.controls[0].hwnd)
        return;
    a.layoutBusy = true;
    RECT client{};
    GetClientRect(a.window, &client);
    a.pixelWidth = std::max(1L, client.right);
    a.pixelHeight = std::max(1L, client.bottom);
    int width = MulDiv(a.pixelWidth, 96, static_cast<int>(a.dpi));
    int height = MulDiv(a.pixelHeight, 96, static_cast<int>(a.dpi));
    a.view.width = std::clamp(width, 420, 620);
    a.offsetX = std::max(0, (width - a.view.width) / 2);
    SCROLLINFO si{sizeof(si),
                  SIF_RANGE | SIF_PAGE | SIF_POS,
                  0,
                  design::ContentHeight - 1,
                  static_cast<UINT>(height),
                  a.scrollY,
                  0};
    SetScrollInfo(a.window, SB_VERT, &si, TRUE);
    a.scrollY = GetScrollPos(a.window, SB_VERT);
    // A scrollbar can change the client width while WM_SIZE is reentrant.
    GetClientRect(a.window, &client);
    a.pixelWidth = std::max(1L, client.right);
    a.pixelHeight = std::max(1L, client.bottom);
    width = MulDiv(a.pixelWidth, 96, static_cast<int>(a.dpi));
    a.view.width = std::clamp(width, 420, 620);
    a.offsetX = std::max(0, (width - a.view.width) / 2);
    const auto items = design::Items(a.view);
    a.previous.reset();
    a.frame.reset();
    a.dirty = true;
    for (auto &c : a.controls)
        if (std::none_of(items.begin(), items.end(), [&](const auto &item) { return item.id == c.id; }))
            SetWindowPos(c.hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW |
                             SWP_NOREDRAW);
    for (auto item : items) {
        auto h = a.control(item.id);
        auto r = item.rect;
        SetWindowPos(h, nullptr, a.px(static_cast<int>(r.X) + a.offsetX),
                     a.px(static_cast<int>(r.Y) - a.scrollY), a.px(static_cast<int>(r.Width)),
                     a.px(static_cast<int>(r.Height)),
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOREDRAW);
    }
    a.previous.reset();
    a.frame.reset();
    a.layoutBusy = false;
    Dirty(a);
}
void Frame(App &a) {
    if (!a.dirty && a.frame)
        return;
    auto fresh = std::make_unique<Bitmap>(a.pixelWidth, a.pixelHeight, PixelFormat32bppPARGB);
    {
        Graphics g(fresh.get());
        g.ScaleTransform(a.scale(), a.scale());
        g.TranslateTransform(static_cast<float>(a.offsetX), static_cast<float>(-a.scrollY));
        design::Render(g, a.view, false);
        for (auto item : design::Items(a.view)) {
            auto &c = a.controls[static_cast<size_t>(item.id - design::Source)];
            float pressed = a.keyboard ? ((SendMessageW(c.hwnd, BM_GETSTATE, 0, 0) & BST_PUSHED) ? 1.0f : 0.0f) : c.press;
            design::DrawControl(g, a.view, item.id, item.rect, c.hover, pressed,
                                GetFocus() == c.hwnd && a.keyboard);
        }
    }
    if (a.previous) {
        float p = static_cast<float>(GetTickCount64() - a.transitionStart) / 180.0f;
        if (p >= 1 || a.reduced)
            a.previous.reset();
        else {
            auto mixed = std::make_unique<Bitmap>(a.pixelWidth, a.pixelHeight, PixelFormat32bppPARGB);
            Graphics g(mixed.get());
            g.DrawImage(a.previous.get(), Gdiplus::Rect(0, 0, a.pixelWidth, a.pixelHeight), 0, 0,
                        a.pixelWidth, a.pixelHeight, Gdiplus::UnitPixel);
            Gdiplus::ColorMatrix matrix = {
                1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, design::Ease(p), 0, 0, 0, 0, 0, 1};
            Gdiplus::ImageAttributes attributes;
            attributes.SetColorMatrix(&matrix);
            int slide = static_cast<int>(a.px(12) * (1 - design::Ease(p))) * (a.view.settings ? 1 : -1);
            g.DrawImage(fresh.get(), Gdiplus::Rect(slide, 0, a.pixelWidth, a.pixelHeight), 0, 0, a.pixelWidth,
                        a.pixelHeight, Gdiplus::UnitPixel, &attributes);
            fresh = std::move(mixed);
        }
    }
    a.frame = std::move(fresh);
    a.dirty = false;
}
void Navigate(App &a, bool settings) {
    if (a.view.settings == settings)
        return;
    Frame(a);
    auto before = std::move(a.frame);
    for (auto &c : a.controls) {
        c.hover = c.from = c.target = 0;
        c.tracking = false;
    }
    a.view.settings = settings;
    a.scrollY = 0;
    Layout(a);
    if (!a.keyboard && !a.reduced) {
        a.previous = std::move(before);
        a.transitionStart = GetTickCount64();
        SetTimer(a.window, 2, 16, nullptr);
    }
    SetFocus(a.control(settings ? design::Back : design::Settings));
    Dirty(a);
}
void Hover(Control &c, float target) {
    auto &a = *c.app;
    if (c.target == target && c.hover == target)
        return;
    c.from = c.hover;
    c.target = target;
    c.started = GetTickCount64();
    if (a.reduced)
        c.hover = target;
    else
        SetTimer(a.window, 2, 16, nullptr);
    Dirty(a);
}
void SliderPoint(Control &c, int x) {
    auto &a = *c.app;
    RECT r;
    GetClientRect(c.hwnd, &r);
    int value =
        design::VolumeAt(static_cast<float>(x), static_cast<float>(r.right), static_cast<float>(a.px(9)));
    // Change the model before requesting a paint; native immediate redraw used the old value.
    SendMessageW(c.hwnd, TBM_SETPOS, FALSE, value);
    SendMessageW(a.window, WM_HSCROLL, MAKEWPARAM(TB_THUMBTRACK, value), reinterpret_cast<LPARAM>(c.hwnd));
}
void PaintChild(HWND window, HDC dc, void *context) {
    auto &a = *static_cast<App *>(context);
    Frame(a);
    POINT origin{};
    MapWindowPoints(window, a.window, &origin, 1);
    RECT bounds{};
    GetClientRect(window, &bounds);
    Graphics graphics(dc);
    graphics.DrawImage(a.frame.get(), Gdiplus::Rect(0, 0, bounds.right, bounds.bottom), origin.x, origin.y,
                       bounds.right, bounds.bottom, Gdiplus::UnitPixel);
}
LRESULT CALLBACK Subclass(HWND w, UINT m, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data) {
    auto &c = *reinterpret_cast<Control *>(data);
    auto &a = *c.app;
    if (auto result = design::PaintControlSurface(w, m, wp, PaintChild, &a))
        return *result;
    if (m == WM_GETDLGCODE) {
        auto flags = DefSubclassProc(w, m, wp, lp);
        if (wp == VK_RETURN && c.id != design::Volume)
            flags |= DLGC_WANTMESSAGE;
        return flags;
    }
    if (m == WM_MOUSEMOVE && !c.tracking) {
        c.tracking = true;
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, w, 0};
        TrackMouseEvent(&track);
        Hover(c, 1);
    }
    if (m == WM_MOUSELEAVE) {
        c.tracking = false;
        Hover(c, 0);
    }
    if ((m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_CAPTURECHANGED || m == WM_CANCELMODE) && c.id != design::Volume) {
        c.pressFrom = c.press;
        c.pressTarget = m == WM_LBUTTONDOWN ? 1.0f : 0.0f;
        c.pressStart = GetTickCount64();
        if (a.reduced) c.press = c.pressTarget;
        else SetTimer(a.window, 2, 16, nullptr);
        Dirty(a);
    }
    if (m == WM_LBUTTONDOWN) {
        a.keyboard = false;
        if (c.id == design::Volume) {
            SetFocus(w);
            SetCapture(w);
            c.dragging = true;
            SliderPoint(c, static_cast<short>(LOWORD(lp)));
            return 0;
        }
    }
    if (m == WM_MOUSEMOVE && c.dragging) {
        SliderPoint(c, static_cast<short>(LOWORD(lp)));
        return 0;
    }
    if (m == WM_LBUTTONUP && c.dragging) {
        c.dragging = false;
        ReleaseCapture();
        Save(a);
        return 0;
    }
    if (m == WM_CAPTURECHANGED || m == WM_CANCELMODE)
        c.dragging = false;
    if (m == WM_KEYDOWN) {
        a.keyboard = true;
        if (wp == VK_ESCAPE && a.view.settings) {
            Navigate(a, false);
            return 0;
        }
        if (wp == VK_RETURN && c.id != design::Volume) {
            if (!(lp & (1LL << 30)) && IsWindowEnabled(w))
                SendMessageW(a.window, WM_COMMAND, MAKEWPARAM(c.id, BN_CLICKED), reinterpret_cast<LPARAM>(w));
            return 0;
        }
    }
    if (m == WM_KEYUP && wp == VK_RETURN && c.id != design::Volume)
        return 0;
    if (m == WM_SETFOCUS) {
        PostMessageW(a.window, RevealMessage, reinterpret_cast<WPARAM>(w), 0);
    }
    if (m == WM_NCDESTROY)
        RemoveWindowSubclass(w, Subclass, id);
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_SETFOCUS || m == WM_KILLFOCUS ||
        m == WM_KEYDOWN || m == WM_KEYUP || m == BM_SETSTATE || m == WM_ENABLE)
        a.dirty = true;
    const auto result = DefSubclassProc(w, m, wp, lp);
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_SETFOCUS || m == WM_KILLFOCUS ||
        m == WM_KEYDOWN || m == WM_KEYUP)
        Dirty(a);
    return result;
}
void Sync(App &a) {
    const auto before = a.view;
    const auto stats = a.engine.snapshot();
    a.view.running = stats.running;
    a.view.ready = stats.ready;
    const auto s = Device(a, a.prefs.sourceId), d = Device(a, a.prefs.destinationId);
    a.view.source = s ? s->name : L"";
    a.view.destination = d ? d->name : L"";
    a.view.canStart = s && d && s->id != d->id;
    a.view.message.clear();
    a.view.error = false;
    if (!stats.lastError.empty()) {
        a.view.message = L"Não foi possível conectar. Confira os dispositivos e tente novamente.";
        a.view.error = true;
    } else if (!s || !d)
        a.view.message = L"Conecte dois dispositivos ao Windows e selecione acima.";
    else if (s->id == d->id)
        a.view.message = L"Selecione um segundo dispositivo diferente.";
    else if (s->looksHandsFree || d->looksHandsFree)
        a.view.message = L"Modo de chamadas detectado pelo nome. Prefira a saída estéreo.";
    if (GetTickCount64() < a.noticeUntil)
        a.view.message = a.notice;
    for (auto id : {design::Source, design::Destination, design::Stability})
        design::SetControlEnabled(a.control(id), !stats.running);
    design::SetControlEnabled(a.control(design::Start), a.view.canStart || stats.running);
    const std::wstring sourceLabel = L"Som do PC: " + (s ? s->name : L"Selecionar dispositivo");
    const std::wstring destLabel = L"Compartilhar com: " + (d ? d->name : L"Selecionar dispositivo");
    Label(a, design::Source, sourceLabel);
    Label(a, design::Destination, destLabel);
    Label(a, design::Start,
          stats.running ? (stats.ready ? L"Parar compartilhamento" : L"Cancelar conexão")
                        : L"Compartilhar áudio");
    if (before != a.view) {
        NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, a.window, OBJID_CLIENT, CHILDID_SELF);
        Dirty(a);
    }
}
void RefreshDevices(App &a, bool force = false) {
    if (a.picking)
        return;
    auto fresh = EnumerateRenderDevices();
    bool same = fresh.size() == a.devices.size() &&
                std::equal(fresh.begin(), fresh.end(), a.devices.begin(), [](const auto &x, const auto &y) {
                    return x.id == y.id && x.name == y.name && x.isDefault == y.isDefault;
                });
    if (same && !force)
        return;
    a.devices = std::move(fresh);
    if (a.view.running && (!Device(a, a.prefs.sourceId) || !Device(a, a.prefs.destinationId))) {
        a.engine.stop();
        a.notice = L"Um dispositivo foi desconectado. Reconecte e selecione novamente.";
        a.noticeUntil = GetTickCount64() + 6000;
    }
    if (a.prefs.sourceId.empty()) {
        auto i = std::find_if(a.devices.begin(), a.devices.end(), [](const auto &d) { return d.isDefault; });
        if (i != a.devices.end())
            a.prefs.sourceId = i->id;
        else if (!a.devices.empty())
            a.prefs.sourceId = a.devices.front().id;
    }
    if (a.prefs.destinationId.empty()) {
        auto i = std::find_if(a.devices.begin(), a.devices.end(),
                              [&](const auto &d) { return d.id != a.prefs.sourceId && !d.looksHandsFree; });
        if (i != a.devices.end())
            a.prefs.destinationId = i->id;
    }
    Sync(a);
    Dirty(a);
}
thread_local HWND menuAnchor{};
LRESULT CALLBACK MenuToggleFilter(int code, WPARAM wp, LPARAM lp) {
    if (code == MSGF_MENU && menuAnchor) {
        const auto &message = *reinterpret_cast<MSG *>(lp);
        RECT bounds{};
        GetWindowRect(menuAnchor, &bounds);
        if (message.message == WM_LBUTTONDOWN && PtInRect(&bounds, message.pt)) {
            EndMenu();
            return 1; // Consume the second down so it cannot reopen the menu.
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}
void Expand(App &a, int control, float target) {
    a.view.expandedControl = control;
    a.expansionFrom = a.view.expansion;
    a.expansionTarget = target;
    a.expansionStart = GetTickCount64();
    if (a.reduced || a.keyboard) a.view.expansion = target;
    else SetTimer(a.window, 2, 16, nullptr);
    Dirty(a);
}
int Menu(App &a, HWND anchor, std::vector<MenuItem> &entries) {
    HMENU menu = CreatePopupMenu();
    HBRUSH background = CreateSolidBrush(RGB(32, 37, 44));
    MENUINFO menuInfo{sizeof(menuInfo)};
    menuInfo.fMask = MIM_BACKGROUND;
    menuInfo.hbrBack = background;
    SetMenuInfo(menu, &menuInfo);
    for (size_t i = 0; i < entries.size(); ++i) {
        MENUITEMINFOW item{sizeof(item)};
        item.fMask = MIIM_FTYPE | MIIM_ID | MIIM_DATA | MIIM_STATE | MIIM_STRING;
        item.dwTypeData = entries[i].label.data();
        item.fType = MFT_OWNERDRAW;
        item.wID = static_cast<UINT>(i + 1);
        item.dwItemData = reinterpret_cast<ULONG_PTR>(&entries[i]);
        item.fState = entries[i].disabled ? MFS_DISABLED : MFS_ENABLED;
        InsertMenuItemW(menu, static_cast<UINT>(i), TRUE, &item);
    }
    RECT r;
    GetWindowRect(anchor, &r);
    SetForegroundWindow(a.window);
    a.picking = true;
    menuAnchor = anchor;
    HHOOK filter = SetWindowsHookExW(WH_MSGFILTER, MenuToggleFilter, nullptr, GetCurrentThreadId());
    Expand(a, GetDlgCtrlID(anchor), 1);
    TPMPARAMS placement{sizeof(placement), r};
    int command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_WORKAREA | ((a.reduced || a.keyboard) ? TPM_NOANIMATION : TPM_VERPOSANIMATION), r.left,
                                   r.bottom + a.px(4), a.window, &placement);
    if (filter) UnhookWindowsHookEx(filter);
    menuAnchor = nullptr;
    a.picking = false;
    Expand(a, GetDlgCtrlID(anchor), 0);
    DestroyMenu(menu);
    DeleteObject(background);
    SetFocus(anchor);
    return command - 1;
}
void PickDevice(App &a, bool source) {
    if (a.view.running)
        return;
    RefreshDevices(a, true);
    const auto devices = a.devices;
    std::vector<MenuItem> entries;
    entries.reserve(devices.size() + 2);
    for (const auto &d : devices)
        entries.push_back(
            {d.name + (d.id == (source ? a.prefs.destinationId : a.prefs.sourceId) ? L"  (trocar)"
                       : d.isDefault                                               ? L"  (padrão)"
                                                                                   : L""),
             d.id == (source ? a.prefs.sourceId : a.prefs.destinationId), false});
    entries.push_back({L"Atualizar dispositivos", false, false});
    entries.push_back({L"Abrir som do Windows", false, false});
    int choice = Menu(a, a.control(source ? design::Source : design::Destination), entries);
    if (choice >= 0 && choice < static_cast<int>(devices.size()) &&
        !entries[static_cast<size_t>(choice)].disabled) {
        design::SelectEndpoint(a.prefs.sourceId, a.prefs.destinationId, source,
                               devices[static_cast<size_t>(choice)].id);
        a.noticeUntil = 0;
        Save(a);
        Sync(a);
        Dirty(a);
    } else if (choice == static_cast<int>(devices.size()))
        RefreshDevices(a, true);
    else if (choice == static_cast<int>(devices.size() + 1))
        ShellExecuteW(a.window, L"open", L"ms-settings:sound", nullptr, nullptr, SW_SHOWNORMAL);
}
void PickBuffer(App &a) {
    if (a.view.running)
        return;
    std::vector<MenuItem> entries;
    int i = 0;
    for (auto text : {L"Automática (recomendado)", L"20 ms", L"30 ms", L"40 ms", L"60 ms", L"80 ms",
                      L"120 ms", L"160 ms"}) {
        entries.push_back({text, i++ == a.view.bufferIndex, false});
    }
    int choice = Menu(a, a.control(design::Stability), entries);
    if (choice >= 0 && choice < 8) {
        a.view.bufferIndex = choice;
        Save(a);
        Dirty(a);
    }
}
void ToggleAudio(App &a) {
    a.noticeUntil = 0;
    if (a.engine.snapshot().running)
        a.engine.stop();
    else if (a.view.canStart) {
        Save(a);
        a.engine.setVolume(static_cast<float>(a.view.volume) / 100);
        a.engine.start(a.prefs.sourceId, a.prefs.destinationId, Latencies[a.view.bufferIndex]);
    }
    Sync(a);
    Dirty(a);
}
void CopyDiagnostics(App &a) {
    const auto s = a.engine.snapshot();
    std::wostringstream out;
    out << L"AudioDuplo 1.1.4\r\nOrigem: " << a.view.source << L"\r\nDestino: " << a.view.destination
        << L"\r\nAtivo: " << s.ready << L"\r\nBuffer atual/alvo: " << s.bufferMs << L" / "
        << s.targetLatencyMs << L" ms\r\nInterrupções: " << s.underruns << L"\r\nExcesso: " << s.overruns
        << L"\r\nDescontinuidades: " << s.discontinuities << L"\r\nCorreção: " << s.driftPpm
        << L" ppm\r\nCaptura: " << s.sourceStatus << L"\r\nSaída: " << s.destinationStatus << L"\r\nErro: "
        << s.lastError;
    const auto value = out.str();
    SIZE_T size = (value.size() + 1) * sizeof(wchar_t);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!data)
        return;
    auto memory = GlobalLock(data);
    if (!memory) {
        GlobalFree(data);
        return;
    }
    memcpy(memory, value.c_str(), size);
    GlobalUnlock(data);
    bool copied = false;
    if (OpenClipboard(a.window)) {
        EmptyClipboard();
        copied = SetClipboardData(CF_UNICODETEXT, data) != nullptr;
        CloseClipboard();
    }
    if (!copied)
        GlobalFree(data);
    a.notice = copied ? L"Diagnóstico copiado. Revise os nomes antes de compartilhar."
                      : L"Não foi possível copiar. Tente novamente.";
    a.noticeUntil = GetTickCount64() + 5000;
    Sync(a);
    Dirty(a);
}
void TrayIcon(App &a, bool add) {
    NOTIFYICONDATAW n{sizeof(n)};
    n.hWnd = a.window;
    n.uID = 1;
    n.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    n.uCallbackMessage = TrayMessage;
    n.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
    wcscpy_s(n.szTip, L"AudioDuplo · clique para abrir");
    if (Shell_NotifyIconW(add ? NIM_ADD : NIM_DELETE, &n) || !add)
        a.inTray = add;
}
void Create(App &a) {
    const wchar_t *names[] = {L"Som do PC",
                              L"Compartilhar com",
                              L"Volume do segundo fone",
                              L"Compartilhar áudio",
                              L"Ajustes",
                              L"Voltar",
                              L"Estabilidade do áudio",
                              L"Som do Windows",
                              L"Copiar diagnóstico",
                              L"Continuar na bandeja"};
    for (size_t i = 0; i < a.controls.size(); ++i) {
        auto &c = a.controls[i];
        c.app = &a;
        c.id = design::Source + static_cast<int>(i);
        bool slider = c.id == design::Volume;
        c.hwnd = CreateWindowExW(0, slider ? TRACKBAR_CLASSW : L"BUTTON", names[i],
                                 WS_CHILD | WS_TABSTOP | (slider ? TBS_NOTICKS : BS_PUSHBUTTON), 0, 0, 1, 1,
                                 a.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(c.id)),
                                 GetModuleHandleW(nullptr), nullptr);
        SetWindowSubclass(c.hwnd, Subclass, 1, reinterpret_cast<DWORD_PTR>(&c));
    }
    a.prefs.load();
    a.view.volume = a.prefs.volume;
    a.view.bufferIndex = a.prefs.latencyIndex;
    a.engine.setVolume(static_cast<float>(a.view.volume) / 100);
    SendMessageW(a.control(design::Volume), TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessageW(a.control(design::Volume), TBM_SETPOS, TRUE, a.view.volume);
    SendMessageW(a.control(design::Volume), TBM_SETPAGESIZE, 0, 5);
    MotionPreference(a);
    RefreshDevices(a, true);
    Layout(a);
    SetTimer(a.window, 1, 250, nullptr);
}
void Scroll(App &a, int command) {
    SCROLLINFO s{sizeof(s), SIF_ALL};
    GetScrollInfo(a.window, SB_VERT, &s);
    int pos = s.nPos;
    switch (command) {
    case SB_LINEUP:
        pos -= 32;
        break;
    case SB_LINEDOWN:
        pos += 32;
        break;
    case SB_PAGEUP:
        pos -= static_cast<int>(s.nPage);
        break;
    case SB_PAGEDOWN:
        pos += static_cast<int>(s.nPage);
        break;
    case SB_THUMBTRACK:
        pos = s.nTrackPos;
        break;
    default:
        return;
    }
    s.fMask = SIF_POS;
    s.nPos = pos;
    SetScrollInfo(a.window, SB_VERT, &s, TRUE);
    a.scrollY = GetScrollPos(a.window, SB_VERT);
    Layout(a);
}
LRESULT CALLBACK WndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    auto *a = reinterpret_cast<App *>(GetWindowLongPtrW(w, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        a = static_cast<App *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
        a->window = w;
        a->dpi = GetDpiForWindow(w);
        SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(a));
    }
    if (!a)
        return DefWindowProcW(w, m, wp, lp);
    switch (m) {
    case WM_CREATE:
        Create(*a);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT p;
        HDC dc = BeginPaint(w, &p);
        Frame(*a);
        {
            Graphics g(dc);
            g.DrawImage(a->frame.get(), Gdiplus::Rect(0, 0, a->pixelWidth, a->pixelHeight), 0, 0,
                        a->pixelWidth, a->pixelHeight, Gdiplus::UnitPixel);
        }
        EndPaint(w, &p);
        return 0;
    }
    case WM_DRAWITEM: {
        auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(lp);
        Graphics g(item.hDC);
        if (item.CtlType == ODT_MENU) {
            auto &entry = *reinterpret_cast<MenuItem *>(item.itemData);
            Gdiplus::RectF r(0, 0, static_cast<float>(item.rcItem.right - item.rcItem.left),
                             static_cast<float>(item.rcItem.bottom - item.rcItem.top));
            g.TranslateTransform(static_cast<float>(item.rcItem.left), static_cast<float>(item.rcItem.top));
            Gdiplus::SolidBrush background(design::Surface());
            g.FillRectangle(&background, r);
            if (item.itemState & ODS_SELECTED)
                design::Rounded(g, {3, 2, r.Width - 6, r.Height - 4}, 6, Gdiplus::Color(45, 56, 72),
                                Gdiplus::Color(0, 0, 0, 0));
            if (entry.selected)
                design::Text(g, L"✓", {8, 0, 24, r.Height}, static_cast<float>(a->px(14)), design::Accent());
            design::Text(g, entry.label, {static_cast<float>(a->px(30)), 0, r.Width - a->px(46), r.Height},
                         static_cast<float>(a->px(14)),
                         entry.disabled ? Gdiplus::Color(125, 135, 150) : design::Ink());
            return TRUE;
        }
        return FALSE;
    }
    case WM_MEASUREITEM: {
        auto &item = *reinterpret_cast<MEASUREITEMSTRUCT *>(lp);
        if (item.CtlType == ODT_MENU) {
            item.itemWidth = a->px(std::max(310, a->view.width - 88));
            item.itemHeight = a->px(42);
            return TRUE;
        }
        break;
    }
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lp) == a->control(design::Volume)) {
            a->view.volume = static_cast<int>(SendMessageW(a->control(design::Volume), TBM_GETPOS, 0, 0));
            a->engine.setVolume(static_cast<float>(a->view.volume) / 100);
            if (LOWORD(wp) == TB_ENDTRACK)
                Save(*a);
            Dirty(*a);
        }
        return 0;
    case WM_COMMAND:
        if (lp && (HIWORD(wp) != BN_CLICKED || !IsWindowEnabled(reinterpret_cast<HWND>(lp))))
            return 0;
        switch (LOWORD(wp)) {
        case design::Source:
            PickDevice(*a, true);
            break;
        case design::Destination:
            PickDevice(*a, false);
            break;
        case design::Start:
            ToggleAudio(*a);
            break;
        case design::Settings:
            Navigate(*a, true);
            break;
        case design::Back:
            Navigate(*a, false);
            break;
        case design::Stability:
            PickBuffer(*a);
            break;
        case design::Diagnostics:
            CopyDiagnostics(*a);
            break;
        case design::WindowsSound:
            ShellExecuteW(w, L"open", L"ms-settings:sound", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case design::Tray:
            TrayIcon(*a, true);
            if (a->inTray)
                ShowWindow(w, SW_HIDE);
            break;
        case 9001:
            RefreshDevices(*a, true);
            break;
        case 9002:
            Navigate(*a, true);
            break;
        }
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            if (++a->ticks % 8 == 0)
                RefreshDevices(*a);
            Sync(*a);
        } else {
            if (a->previous && GetTickCount64() - a->transitionStart >= 180)
                a->previous.reset();
            bool animate = a->previous != nullptr;
            float expansionTime = std::min(1.0f, static_cast<float>(GetTickCount64() - a->expansionStart) / 180.0f);
            if (a->reduced || a->keyboard) expansionTime = 1;
            a->view.expansion = a->expansionFrom + (a->expansionTarget - a->expansionFrom) * design::Ease(expansionTime);
            if (expansionTime < 1 && a->expansionFrom != a->expansionTarget) animate = true;
            for (auto &c : a->controls) {
                float pressTime = std::min(1.0f, static_cast<float>(GetTickCount64() - c.pressStart) / 120.0f);
                if (a->reduced) pressTime = 1;
                c.press = c.pressFrom + (c.pressTarget - c.pressFrom) * design::Ease(pressTime);
                if (pressTime < 1 && c.pressFrom != c.pressTarget) animate = true;
                float p = std::min(1.0f, static_cast<float>(GetTickCount64() - c.started) / (c.id == design::Start ? 180.0f : 120.0f));
                c.hover = c.from + (c.target - c.from) * design::Ease(p);
                if (p < 1 && c.from != c.target)
                    animate = true;
            }
            Dirty(*a);
            if (!animate)
                KillTimer(w, 2);
        }
        return 0;
    case WM_SIZE:
        Layout(*a);
        return 0;
    case WM_VSCROLL:
        Scroll(*a, LOWORD(wp));
        return 0;
    case WM_MOUSEWHEEL:
        Scroll(*a, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? SB_LINEUP : SB_LINEDOWN);
        return 0;
    case WM_GETMINMAXINFO: {
        auto *info = reinterpret_cast<MINMAXINFO *>(lp);
        info->ptMinTrackSize = {a->px(460), a->px(420)};
        return 0;
    }
    case RevealMessage: {
        RECT r;
        GetClientRect(w, &r);
        int height = MulDiv(r.bottom, 96, static_cast<int>(a->dpi));
        for (auto item : design::Items(a->view))
            if (a->control(item.id) == reinterpret_cast<HWND>(wp)) {
                int y = static_cast<int>(item.rect.Y), bottom = static_cast<int>(item.rect.GetBottom());
                int prior = a->scrollY;
                if (y < a->scrollY)
                    a->scrollY = std::max(0, y - 16);
                else if (bottom > a->scrollY + height)
                    a->scrollY = bottom - height + 16;
                if (prior != a->scrollY)
                    Layout(*a);
            }
        return 0;
    }
    case WM_DPICHANGED: {
        a->dpi = HIWORD(wp);
        auto *r = reinterpret_cast<RECT *>(lp);
        SetWindowPos(w, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        Layout(*a);
        return 0;
    }
    case WM_SETTINGCHANGE:
        MotionPreference(*a);
        if (a->reduced) {
            a->previous.reset();
            for (auto &c : a->controls)
                c.hover = c.target;
        }
        Dirty(*a);
        return 0;
    case TrayMessage:
        if (lp == WM_LBUTTONUP || lp == WM_LBUTTONDBLCLK) {
            ShowWindow(w, SW_RESTORE);
            SetForegroundWindow(w);
            TrayIcon(*a, false);
        } else if (lp == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 1, L"Abrir AudioDuplo");
            AppendMenuW(menu, MF_STRING, 2, L"Encerrar");
            POINT p;
            GetCursorPos(&p);
            SetForegroundWindow(w);
            int choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, w, nullptr);
            DestroyMenu(menu);
            if (choice == 1) {
                ShowWindow(w, SW_RESTORE);
                TrayIcon(*a, false);
            }
            if (choice == 2)
                SendMessageW(w, WM_CLOSE, 0, 0);
        }
        return 0;
    case WM_CLOSE:
        Save(*a);
        DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        KillTimer(w, 1);
        KillTimer(w, 2);
        a->engine.stop();
        if (a->inTray)
            TrayIcon(*a, false);
        PostQuitMessage(0);
        return 0;
    default:
        if (m == RegisterWindowMessageW(L"TaskbarCreated") && a->inTray)
            TrayIcon(*a, true);
        break;
    }
    return DefWindowProcW(w, m, wp, lp);
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\AudioDuplo.Desktop.v1.1.4");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND other = FindWindowW(ClassName, nullptr)) {
            ShowWindow(other, SW_RESTORE);
            SetForegroundWindow(other);
        }
        CloseHandle(mutex);
        return 0;
    }
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(co)) {
        if (mutex)
            CloseHandle(mutex);
        return 1;
    }
    INITCOMMONCONTROLSEX cc{sizeof(cc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&cc);
    ULONG_PTR token{};
    Gdiplus::GdiplusStartupInput startup;
    if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok) {
        CoUninitialize();
        if (mutex)
            CloseHandle(mutex);
        return 1;
    }
    int result = 1;
    {
        App a;
        a.dpi = GetDpiForSystem();
        WNDCLASSEXW wc{sizeof(wc)};
        wc.hInstance = instance;
        wc.lpfnWndProc = WndProc;
        wc.lpszClassName = ClassName;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        DWORD style = WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_CLIPCHILDREN;
        RECT r{0, 0, a.px(620), a.px(design::ContentHeight)};
        AdjustWindowRectExForDpi(&r, style, FALSE, 0, a.dpi);
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        int width = std::min(r.right - r.left, work.right - work.left),
            height = std::min(r.bottom - r.top, work.bottom - work.top);
        HWND w = CreateWindowExW(WS_EX_CONTROLPARENT | WS_EX_COMPOSITED, ClassName, L"AudioDuplo", style,
                                 work.left + (work.right - work.left - width) / 2,
                                 work.top + (work.bottom - work.top - height) / 2, width, height, nullptr,
                                 nullptr, instance, &a);
        if (w) {
            BOOL dark = TRUE;
            COLORREF caption = RGB(23, 26, 31);
            DwmSetWindowAttribute(w, 20, &dark, sizeof(dark));
            DwmSetWindowAttribute(w, 35, &caption, sizeof(caption));
            ShowWindow(w, show);
            UpdateWindow(w);
            ACCEL keys[] = {{FVIRTKEY, VK_F5, 9001}, {FVIRTKEY, VK_F1, 9002}};
            HACCEL accel = CreateAcceleratorTableW(keys, 2);
            MSG msg{};
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                if (msg.message == WM_KEYDOWN) {
                    a.keyboard = true;
                    if (msg.wParam == VK_ESCAPE && a.view.settings) {
                        Navigate(a, false);
                        continue;
                    }
                }
                if (!TranslateAcceleratorW(w, accel, &msg) && !IsDialogMessageW(w, &msg)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
            DestroyAcceleratorTable(accel);
            result = static_cast<int>(msg.wParam);
        }
    }
    design::ReleaseFonts();
    Gdiplus::GdiplusShutdown(token);
    CoUninitialize();
    if (mutex)
        CloseHandle(mutex);
    return result;
}
