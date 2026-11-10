#include "AudioEngine.hpp"
#include "AudioFormat.hpp"

#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cwctype>
#include <sstream>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace audioduplo {
namespace {

std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

bool ContainsAny(const std::wstring &text, std::initializer_list<const wchar_t *> needles) {
    const auto low = ToLower(text);
    for (const auto *n : needles) {
        if (low.find(ToLower(n)) != std::wstring::npos)
            return true;
    }
    return false;
}

std::wstring HResultText(HRESULT hr) {
    wchar_t *msg = nullptr;
    const DWORD flags =
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD n =
        FormatMessageW(flags, nullptr, static_cast<DWORD>(hr), 0, reinterpret_cast<LPWSTR>(&msg), 0, nullptr);
    std::wstringstream ss;
    ss << L"HRESULT 0x" << std::hex << static_cast<unsigned long>(hr);
    if (n && msg) {
        std::wstring s(msg, n);
        while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n' || s.back() == L' '))
            s.pop_back();
        ss << L" — " << s;
    }
    if (msg)
        LocalFree(msg);
    return ss.str();
}

std::wstring FriendlyName(IMMDevice *dev) {
    if (!dev)
        return L"";
    ComPtr<IPropertyStore> props;
    if (FAILED(dev->OpenPropertyStore(STGM_READ, &props)))
        return L"";
    PROPVARIANT pv;
    PropVariantInit(&pv);
    std::wstring result;
    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal) {
        result = pv.pwszVal;
    }
    PropVariantClear(&pv);
    return result;
}

ComPtr<IMMDevice> DeviceById(const std::wstring &id) {
    ComPtr<IMMDeviceEnumerator> en;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en))))
        return {};
    ComPtr<IMMDevice> dev;
    if (FAILED(en->GetDevice(id.c_str(), &dev)))
        return {};
    return dev;
}

struct ComApartment {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComApartment() {
        if (SUCCEEDED(result))
            CoUninitialize();
    }
};

struct MmcssGuard {
    HANDLE handle{nullptr};
    DWORD taskIndex{0};
    MmcssGuard() {
        handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
        if (!handle)
            handle = AvSetMmThreadCharacteristicsW(L"Audio", &taskIndex);
        if (handle)
            AvSetMmThreadPriority(handle, AVRT_PRIORITY_HIGH);
    }
    ~MmcssGuard() {
        if (handle)
            AvRevertMmThreadCharacteristics(handle);
    }
};

struct EventHandle {
    HANDLE h{nullptr};
    explicit EventHandle(HANDLE value = nullptr) : h(value) {}
    ~EventHandle() {
        if (h)
            CloseHandle(h);
    }
    EventHandle(const EventHandle &) = delete;
    EventHandle &operator=(const EventHandle &) = delete;
};

bool InitRenderClient(IMMDevice *device, const WAVEFORMATEX *internalFormat, ComPtr<IAudioClient> &audio,
                      UINT32 &bufferFrames, HANDLE eventHandle, std::wstring &detail) {
    if (!device || !internalFormat || !eventHandle)
        return false;

    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void **>(audio.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        detail = L"Falha ao ativar IAudioClient: " + HResultText(hr);
        return false;
    }

    WAVEFORMATEX *closest = nullptr;
    const HRESULT nativeSupport =
        audio->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, internalFormat, &closest);
    if (closest)
        CoTaskMemFree(closest);

    // Best path: native 48k stereo float + IAudioClient3 minimum shared period.
    if (nativeSupport == S_OK) {
        ComPtr<IAudioClient3> c3;
        if (SUCCEEDED(audio.As(&c3))) {
            UINT32 defPeriod = 0, fundamental = 0, minPeriod = 0, maxPeriod = 0;
            hr = c3->GetSharedModeEnginePeriod(internalFormat, &defPeriod, &fundamental, &minPeriod,
                                               &maxPeriod);
            if (SUCCEEDED(hr) && minPeriod > 0) {
                hr = c3->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, minPeriod,
                                                     internalFormat, nullptr);
                if (SUCCEEDED(hr)) {
                    if (FAILED(audio->SetEventHandle(eventHandle))) {
                        detail = L"Falha em SetEventHandle da saída.";
                        return false;
                    }
                    if (FAILED(audio->GetBufferSize(&bufferFrames))) {
                        detail = L"Falha em GetBufferSize da saída.";
                        return false;
                    }
                    detail = L"IAudioClient3 / menor periodo compartilhado";
                    return true;
                }
            }
        }
    }

    // Re-activate because a failed initialization attempt should never contaminate fallback state.
    audio.Reset();
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void **>(audio.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        detail = L"Falha ao reativar IAudioClient: " + HResultText(hr);
        return false;
    }

    const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY | AUDCLNT_STREAMFLAGS_NOPERSIST;

    // 20 ms is only the WASAPI client buffer capacity. The application ring target is separate.
    REFERENCE_TIME clientBuffer = 20 * 10000LL;
    hr = audio->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, clientBuffer, 0, internalFormat, nullptr);
    if (FAILED(hr)) {
        // Some Bluetooth stacks/drivers want more client-side headroom. 40 ms fallback.
        audio.Reset();
        if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                    reinterpret_cast<void **>(audio.ReleaseAndGetAddressOf())))) {
            detail = L"Falha ao reativar renderizador no fallback.";
            return false;
        }
        clientBuffer = 40 * 10000LL;
        hr = audio->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, clientBuffer, 0, internalFormat, nullptr);
    }
    if (FAILED(hr)) {
        detail = L"Falha ao inicializar renderizacao WASAPI: " + HResultText(hr);
        return false;
    }

    hr = audio->SetEventHandle(eventHandle);
    if (FAILED(hr)) {
        detail = L"Falha em SetEventHandle: " + HResultText(hr);
        return false;
    }
    hr = audio->GetBufferSize(&bufferFrames);
    if (FAILED(hr)) {
        detail = L"Falha em GetBufferSize: " + HResultText(hr);
        return false;
    }
    detail = L"IAudioClient shared + AUTOCONVERTPCM/SRC";
    return true;
}

} // namespace

bool LooksHandsFreeName(const std::wstring &name) {
    return ContainsAny(name,
                       {L"hands-free", L"hands free", L"ag audio", L"hfp", L"mãos livres", L"maos livres"});
}

std::vector<DeviceInfo> EnumerateRenderDevices() {
    std::vector<DeviceInfo> out;
    ComPtr<IMMDeviceEnumerator> en;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en));
    if (FAILED(hr))
        return out;

    std::wstring defaultId;
    ComPtr<IMMDevice> def;
    if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eMultimedia, &def))) {
        LPWSTR id = nullptr;
        if (SUCCEEDED(def->GetId(&id)) && id) {
            defaultId = id;
            CoTaskMemFree(id);
        }
    }

    ComPtr<IMMDeviceCollection> col;
    if (FAILED(en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col)))
        return out;
    UINT count = 0;
    if (FAILED(col->GetCount(&count)))
        return out;

    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> dev;
        if (FAILED(col->Item(i, &dev)))
            continue;
        LPWSTR id = nullptr;
        if (FAILED(dev->GetId(&id)) || !id)
            continue;
        DeviceInfo d;
        d.id = id;
        CoTaskMemFree(id);
        d.name = FriendlyName(dev.Get());
        if (d.name.empty())
            d.name = L"Dispositivo de audio";
        d.isDefault = (d.id == defaultId);
        d.looksHandsFree = LooksHandsFreeName(d.name);
        out.push_back(std::move(d));
    }
    return out;
}

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::start(const std::wstring &sourceId, const std::wstring &destinationId,
                        LatencyChoice choice) {
    stop();
    if (sourceId.empty() || destinationId.empty()) {
        setError(L"Selecione o primeiro dispositivo e o segundo dispositivo.");
        return false;
    }
    if (sourceId == destinationId) {
        setError(L"Origem e destino precisam ser endpoints diferentes.");
        return false;
    }

    switch (choice) {
    case LatencyChoice::Automatic:
    case LatencyChoice::Ms20:
    case LatencyChoice::Ms30:
    case LatencyChoice::Ms40:
    case LatencyChoice::Ms60:
    case LatencyChoice::Ms80:
    case LatencyChoice::Ms120:
    case LatencyChoice::Ms160:
        break;
    default:
        setError(L"Buffer inválido.");
        return false;
    }
    sourceId_ = sourceId;
    destinationId_ = destinationId;
    latencyChoice_ = choice;
    targetLatencyMs_.store(choice == LatencyChoice::Automatic ? 20 : static_cast<int>(choice),
                           std::memory_order_release);
    captureReady_.store(false);
    renderReady_.store(false);
    capturedFrames_.store(0);
    renderedFrames_.store(0);
    peak_.store(0);
    underruns_.store(0);
    overruns_.store(0);
    discontinuities_.store(0);
    driftPpm_.store(0.0);
    lastAutoUnderruns_ = 0;
    autoWindowStartMs_ = GetTickCount64();
    ring_.reset();
    ring_.setTargetFrames(static_cast<std::size_t>(targetLatencyMs_.load()) * 48);

    {
        std::lock_guard<std::mutex> lock(textMutex_);
        sourceStatus_ = L"inicializando...";
        destinationStatus_ = L"inicializando...";
        lastError_.clear();
    }

    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent_) {
        setError(L"Nao foi possivel criar evento de parada.");
        return false;
    }

    running_.store(true, std::memory_order_release);
    try {
        captureThread_ = std::thread([this] {
            try {
                captureThreadMain();
            } catch (...) {
                setError(L"Falha inesperada na captura.");
                running_.store(false);
                SetEvent(stopEvent_);
            }
        });
        renderThread_ = std::thread([this] {
            try {
                renderThreadMain();
            } catch (...) {
                setError(L"Falha inesperada na saída.");
                running_.store(false);
                SetEvent(stopEvent_);
            }
        });
    } catch (...) {
        running_.store(false, std::memory_order_release);
        SetEvent(stopEvent_);
        if (captureThread_.joinable())
            captureThread_.join();
        if (renderThread_.joinable())
            renderThread_.join();
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
        setError(L"Falha ao criar threads de audio.");
        return false;
    }
    return true;
}

void AudioEngine::stop() {
    if (stopEvent_)
        SetEvent(stopEvent_);
    running_.store(false, std::memory_order_release);
    if (captureThread_.joinable())
        captureThread_.join();
    if (renderThread_.joinable())
        renderThread_.join();
    captureReady_.store(false);
    renderReady_.store(false);
    peak_.store(0);
    if (stopEvent_) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
}

void AudioEngine::setError(const std::wstring &text) {
    {
        std::lock_guard<std::mutex> lock(textMutex_);
        lastError_ = text;
    }
}

int AudioEngine::configuredTargetMs() const {
    return targetLatencyMs_.load(std::memory_order_acquire);
}

void AudioEngine::maybeIncreaseAutomaticLatency(std::uint64_t currentUnderruns) {
    if (latencyChoice_ != LatencyChoice::Automatic)
        return;
    const ULONGLONG now = GetTickCount64();
    if (now - autoWindowStartMs_ > 5000) {
        autoWindowStartMs_ = now;
        lastAutoUnderruns_ = currentUnderruns;
        return;
    }

    if (currentUnderruns >= lastAutoUnderruns_ + 2) {
        int current = targetLatencyMs_.load();
        int next = current;
        if (current < 30)
            next = 30;
        else if (current < 40)
            next = 40;
        else if (current < 60)
            next = 60;
        else if (current < 80)
            next = 80;
        if (next != current) {
            targetLatencyMs_.store(next, std::memory_order_release);
            ring_.setTargetFrames(static_cast<std::size_t>(next) * 48);
        }
        lastAutoUnderruns_ = currentUnderruns;
        autoWindowStartMs_ = now;
    }
}

EngineStatsSnapshot AudioEngine::snapshot() const {
    EngineStatsSnapshot s;
    s.running = running_.load(std::memory_order_acquire);
    s.ready = s.running && captureReady_.load() && renderReady_.load();
    s.peak = peak_.load();
    s.capturedFrames = capturedFrames_.load();
    s.renderedFrames = renderedFrames_.load();
    s.bufferMs = static_cast<double>(ring_.availableFrames()) * 1000.0 / static_cast<double>(kSampleRate);
    s.underruns = underruns_.load(std::memory_order_acquire);
    s.overruns = overruns_.load(std::memory_order_acquire);
    s.discontinuities = discontinuities_.load(std::memory_order_acquire);
    s.targetLatencyMs = targetLatencyMs_.load(std::memory_order_acquire);
    s.driftPpm = driftPpm_.load(std::memory_order_acquire);
    {
        std::lock_guard<std::mutex> lock(textMutex_);
        s.sourceStatus = sourceStatus_;
        s.destinationStatus = destinationStatus_;
        s.lastError = lastError_;
    }
    return s;
}

void AudioEngine::captureThreadMain() {
    ComApartment apartment;
    if (FAILED(apartment.result)) {
        setError(L"Não foi possível iniciar o áudio do Windows.");
        running_.store(false);
        SetEvent(stopEvent_);
        return;
    }
    MmcssGuard mmcss;

    auto fail = [&](const std::wstring &msg) {
        setError(msg);
        running_.store(false, std::memory_order_release);
        if (stopEvent_)
            SetEvent(stopEvent_);
    };

    ComPtr<IMMDevice> source = DeviceById(sourceId_);
    if (!source) {
        fail(L"Primeiro dispositivo não encontrado ou desconectado.");
        return;
    }
    const auto sourceName = FriendlyName(source.Get());

    ComPtr<IAudioClient> audio;
    HRESULT hr = source->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void **>(audio.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
        fail(L"Falha ao ativar loopback da origem: " + HResultText(hr));
        return;
    }

    WAVEFORMATEX *mix = nullptr;
    hr = audio->GetMixFormat(&mix);
    const std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> mixOwner(mix, CoTaskMemFree);
    if (FAILED(hr) || !mix) {
        fail(L"Falha ao obter formato da origem: " + HResultText(hr));
        return;
    }

    if (!IsSupportedCaptureFormat(mix)) {
        fail(L"Formato da origem não suportado. Configure uma saída PCM estéreo no Windows.");
        return;
    }

    EventHandle captureEvent(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    if (!captureEvent.h) {
        fail(L"Falha ao criar evento de captura.");
        return;
    }

    const DWORD flags = AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    hr = audio->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, mix, nullptr);
    if (FAILED(hr)) {
        fail(L"Falha ao inicializar WASAPI loopback: " + HResultText(hr));
        return;
    }
    hr = audio->SetEventHandle(captureEvent.h);
    if (FAILED(hr)) {
        fail(L"Falha ao configurar evento do loopback: " + HResultText(hr));
        return;
    }

    ComPtr<IAudioCaptureClient> capture;
    hr = audio->GetService(IID_PPV_ARGS(&capture));
    if (FAILED(hr)) {
        fail(L"Falha ao obter IAudioCaptureClient: " + HResultText(hr));
        return;
    }

    StreamingStereoResampler resampler;
    resampler.reset(static_cast<double>(mix->nSamplesPerSec), static_cast<double>(kSampleRate));
    std::vector<float> decoded;
    decoded.reserve(8192 * 2);

    hr = audio->Start();
    if (FAILED(hr)) {
        fail(L"Falha ao iniciar captura loopback: " + HResultText(hr));
        return;
    }

    {
        std::lock_guard<std::mutex> lock(textMutex_);
        sourceStatus_ =
            sourceName.empty() ? L"Primeiro dispositivo ativo" : sourceName + L" — loopback ativo";
    }

    captureReady_.store(true);
    HANDLE waits[2] = {stopEvent_, captureEvent.h};
    while (running_.load(std::memory_order_acquire)) {
        const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, 2000);
        if (wait == WAIT_OBJECT_0)
            break;
        if (wait == WAIT_TIMEOUT) {
            peak_.store(0);
            DWORD state = 0;
            if (FAILED(source->GetState(&state)) || state != DEVICE_STATE_ACTIVE) {
                fail(L"O primeiro dispositivo foi desconectado. Reconecte e inicie novamente.");
                break;
            }
            continue;
        }
        if (wait != WAIT_OBJECT_0 + 1) {
            fail(L"Erro aguardando evento de captura.");
            break;
        }

        UINT32 packet = 0;
        hr = capture->GetNextPacketSize(&packet);
        if (FAILED(hr)) {
            fail(L"Falha em GetNextPacketSize: " + HResultText(hr));
            break;
        }
        while (packet > 0) {
            BYTE *data = nullptr;
            UINT32 frames = 0;
            DWORD bufferFlags = 0;
            UINT64 devicePos = 0, qpcPos = 0;
            hr = capture->GetBuffer(&data, &frames, &bufferFlags, &devicePos, &qpcPos);
            if (FAILED(hr)) {
                fail(L"Falha em IAudioCaptureClient::GetBuffer: " + HResultText(hr));
                break;
            }

            if (bufferFlags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {
                discontinuities_.fetch_add(1, std::memory_order_relaxed);
            }
            const bool silent = (bufferFlags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
            if (!DecodeToStereoFloat(data, frames, mix, silent, decoded)) {
                capture->ReleaseBuffer(frames);
                fail(L"Formato de origem nao suportado. Use PCM/float compartilhado do Windows.");
                break;
            }

            float peak = 0;
            for (float sample : decoded)
                peak = std::max(peak, std::abs(sample));
            peak_.store(peak);
            capturedFrames_.fetch_add(frames);
            const auto &converted = resampler.process(decoded.data(), frames);
            const std::size_t outFrames = converted.size() / 2;
            if (outFrames > 0) {
                const std::size_t written = ring_.write(converted.data(), outFrames);
                if (written < outFrames) {
                    overruns_.fetch_add(1, std::memory_order_relaxed);
                    // Catastrophic only (ring has 2 s capacity): request click-free fade/re-prime.
                    ring_.requestReprime();
                }
            }

            capture->ReleaseBuffer(frames);
            hr = capture->GetNextPacketSize(&packet);
            if (FAILED(hr)) {
                fail(L"Falha ao consultar proximo pacote de captura: " + HResultText(hr));
                break;
            }
        }
    }

    audio->Stop();
}

void AudioEngine::renderThreadMain() {
    ComApartment apartment;
    if (FAILED(apartment.result)) {
        setError(L"Não foi possível iniciar o áudio do Windows.");
        running_.store(false);
        SetEvent(stopEvent_);
        return;
    }
    MmcssGuard mmcss;

    auto fail = [&](const std::wstring &msg) {
        setError(msg);
        running_.store(false, std::memory_order_release);
        if (stopEvent_)
            SetEvent(stopEvent_);
    };

    ComPtr<IMMDevice> destination = DeviceById(destinationId_);
    if (!destination) {
        fail(L"Segundo dispositivo nao encontrado ou foi desconectado.");
        return;
    }
    const auto destName = FriendlyName(destination.Get());

    EventHandle renderEvent(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    if (!renderEvent.h) {
        fail(L"Falha ao criar evento de renderizacao.");
        return;
    }

    const auto internal = MakeInternalFormat();
    ComPtr<IAudioClient> audio;
    UINT32 bufferFrames = 0;
    std::wstring initDetail;
    if (!InitRenderClient(destination.Get(), &internal.Format, audio, bufferFrames, renderEvent.h,
                          initDetail)) {
        fail(initDetail);
        return;
    }

    ComPtr<IAudioRenderClient> render;
    HRESULT hr = audio->GetService(IID_PPV_ARGS(&render));
    if (FAILED(hr)) {
        fail(L"Falha ao obter IAudioRenderClient: " + HResultText(hr));
        return;
    }

    // Prime WASAPI with silence. This is client buffer initialization, not an artificial app queue.
    BYTE *prime = nullptr;
    if (SUCCEEDED(render->GetBuffer(bufferFrames, &prime))) {
        render->ReleaseBuffer(bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
    }

    hr = audio->Start();
    if (FAILED(hr)) {
        fail(L"Falha ao iniciar a saída: " + HResultText(hr));
        return;
    }

    {
        std::lock_guard<std::mutex> lock(textMutex_);
        destinationStatus_ = destName + L" — " + initDetail;
    }

    std::vector<float> temp;
    temp.resize(static_cast<std::size_t>(bufferFrames) * 2);

    double integral = 0.0;
    bool hadRealAudio = false;
    renderReady_.store(true);
    float currentGain = volume_.load();
    HANDLE waits[2] = {stopEvent_, renderEvent.h};

    while (running_.load(std::memory_order_acquire)) {
        const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0)
            break;
        if (wait != WAIT_OBJECT_0 + 1) {
            fail(L"Erro aguardando evento da saída.");
            break;
        }

        UINT32 padding = 0;
        hr = audio->GetCurrentPadding(&padding);
        if (FAILED(hr)) {
            fail(L"Falha em GetCurrentPadding: " + HResultText(hr));
            break;
        }
        if (padding >= bufferFrames)
            continue;
        const UINT32 framesToWrite = bufferFrames - padding;
        if (framesToWrite == 0)
            continue;

        BYTE *out = nullptr;
        hr = render->GetBuffer(framesToWrite, &out);
        if (FAILED(hr)) {
            fail(L"Falha em IAudioRenderClient::GetBuffer: " + HResultText(hr));
            break;
        }

        const int targetMs = configuredTargetMs();
        const std::size_t targetFrames = static_cast<std::size_t>(targetMs) * 48;
        ring_.setTargetFrames(targetFrames);

        bool recovery = ring_.consumeRecoveryRequest();
        double ratio = 1.0;
        if (!recovery) {
            const double occupancy = static_cast<double>(ring_.availableFrames());
            const double errorFrames = occupancy - static_cast<double>(targetFrames);
            const double dt = static_cast<double>(framesToWrite) / static_cast<double>(kSampleRate);
            integral += errorFrames * dt;
            integral = std::clamp(integral, -static_cast<double>(targetFrames) * 8.0,
                                  static_cast<double>(targetFrames) * 8.0);

            // Very small adaptive correction. Positive error => consume slightly faster.
            double ppm = 0.35 * errorFrames + 0.08 * integral;
            ppm = std::clamp(ppm, -1200.0, 1200.0);
            driftPpm_.store(ppm, std::memory_order_release);
            ratio = 1.0 + ppm * 1.0e-6;
        }

        const std::size_t realFrames = ring_.readAdaptive(temp.data(), framesToWrite, ratio, recovery);
        if (realFrames == framesToWrite) {
            hadRealAudio = true;
        } else if (!recovery && hadRealAudio) {
            underruns_.fetch_add(1, std::memory_order_relaxed);
            hadRealAudio = false;
        }

        const float targetGain = volume_.load();
        for (UINT32 i = 0; i < framesToWrite; ++i) {
            currentGain += (targetGain - currentGain) * 0.004f;
            temp[i * 2] *= currentGain;
            temp[i * 2 + 1] *= currentGain;
        }
        renderedFrames_.fetch_add(realFrames);
        std::memcpy(out, temp.data(), static_cast<std::size_t>(framesToWrite) * 2 * sizeof(float));
        hr = render->ReleaseBuffer(framesToWrite, 0);
        if (FAILED(hr)) {
            fail(L"Falha em IAudioRenderClient::ReleaseBuffer: " + HResultText(hr));
            break;
        }

        const auto u = underruns_.load(std::memory_order_relaxed);
        maybeIncreaseAutomaticLatency(u);
    }

    audio->Stop();
}

} // namespace audioduplo
