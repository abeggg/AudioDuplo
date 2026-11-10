#include "AudioEngine.hpp"
#include "AudioFormat.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace audioduplo;
int checks = 0;
void check(bool ok, const char *name) {
    ++checks;
    if (!ok)
        throw std::runtime_error(name);
}
int main() {
    try {
        check(!LooksHandsFreeName(L"USB Gaming Headset"), "generic headset is allowed");
        check(LooksHandsFreeName(L"Wireless Hands-Free AG Audio"), "HFP hint");
        AudioEngine engine;
        check(!engine.start(L"", L"", LatencyChoice::Automatic), "empty endpoints");
        check(!engine.start(L"same", L"same", LatencyChoice::Automatic), "feedback route rejected");
        check(!engine.start(L"a", L"b", static_cast<LatencyChoice>(-1)), "invalid latency rejected");
        engine.stop();
        engine.stop();
        check(!engine.snapshot().running, "idempotent stop");
        auto fmt = MakeInternalFormat();
        std::vector<float> out;
        float samples[] = {0.5f, -0.5f, std::numeric_limits<float>::quiet_NaN(), 2.0f};
        check(DecodeToStereoFloat(reinterpret_cast<BYTE *>(samples), 2, &fmt.Format, false, out),
              "float decode");
        check(out[0] == 0.5f && out[1] == -0.5f && out[2] == 0 && out[3] == 1, "finite and clamped");
        check(DecodeToStereoFloat(nullptr, 2, &fmt.Format, true, out) && out[0] == 0, "silent flag");
        check(!DecodeToStereoFloat(nullptr, 2, &fmt.Format, false, out), "null data rejected");
        auto bad = fmt;
        bad.Format.wBitsPerSample = 12;
        check(!DecodeToStereoFloat(nullptr, 2, &bad.Format, true, out), "unsupported format rejected");
        WAVEFORMATEX mono{};
        mono.wFormatTag = WAVE_FORMAT_PCM;
        mono.nChannels = 1;
        mono.nSamplesPerSec = 44100;
        mono.wBitsPerSample = 16;
        mono.nBlockAlign = 2;
        short pcm[] = {-32768, 16384};
        check(DecodeToStereoFloat(reinterpret_cast<BYTE *>(pcm), 2, &mono, false, out), "mono PCM16");
        check(out[0] == -1 && out[1] == -1 && out[2] == 0.5f && out[3] == 0.5f, "mono duplicated");
        mono.wBitsPerSample = 8;
        mono.nBlockAlign = 1;
        BYTE p8[] = {0, 128, 255};
        check(DecodeToStereoFloat(p8, 3, &mono, false, out) && out[0] == -1 && out[2] == 0 && out[4] > 0.99f,
              "PCM8");
        mono.wBitsPerSample = 24;
        mono.nBlockAlign = 3;
        BYTE p24[] = {0, 0, 128, 0xff, 0xff, 0x7f};
        check(DecodeToStereoFloat(p24, 2, &mono, false, out) && out[0] == -1 && out[2] > 0.99f,
              "PCM24 sign extension");
        mono.wBitsPerSample = 32;
        mono.nBlockAlign = 4;
        std::int32_t p32[] = {std::numeric_limits<std::int32_t>::min(), 1073741824};
        check(DecodeToStereoFloat(reinterpret_cast<BYTE *>(p32), 2, &mono, false, out) && out[0] == -1 &&
                  out[2] == 0.5f,
              "PCM32");
        mono.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        mono.wBitsPerSample = 64;
        mono.nBlockAlign = 8;
        double f64[] = {-0.25, std::numeric_limits<double>::infinity()};
        check(DecodeToStereoFloat(reinterpret_cast<BYTE *>(f64), 2, &mono, false, out) && out[0] == -0.25f &&
                  out[2] == 0,
              "float64 finite conversion");
        auto invalid = fmt;
        invalid.Format.nBlockAlign = 2;
        check(!IsSupportedCaptureFormat(&invalid.Format), "invalid block alignment");
        invalid = fmt;
        invalid.Format.nSamplesPerSec = 0;
        check(!IsSupportedCaptureFormat(&invalid.Format), "invalid sample rate");
        auto surround = fmt;
        surround.Format.nChannels = 6;
        surround.Format.nBlockAlign = 24;
        surround.dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
        float center[] = {0, 0, 1, 0, 0, 0};
        check(DecodeToStereoFloat(reinterpret_cast<BYTE *>(center), 1, &surround.Format, false, out) &&
                  out[0] > 0 && out[0] == out[1],
              "5.1 center preserved");
        float lfe[] = {0, 0, 0, 1, 0, 0};
        check(DecodeToStereoFloat(reinterpret_cast<BYTE *>(lfe), 1, &surround.Format, false, out) &&
                  out[0] == 0 && out[1] == 0,
              "LFE omitted");
        StreamingStereoResampler whole, split;
        whole.reset(44100);
        split.reset(44100);
        std::vector<float> signal(4410 * 2);
        for (size_t i = 0; i < signal.size(); ++i)
            signal[i] = std::sin(static_cast<float>(i) * 0.01f);
        auto expected = whole.process(signal.data(), 4410);
        std::vector<float> actual;
        for (size_t i = 0; i < 4410; i += 147) {
            auto &part = split.process(signal.data() + i * 2, 147);
            actual.insert(actual.end(), part.begin(), part.end());
        }
        check(actual.size() == expected.size(), "resampler packet length");
        float error = 0;
        for (size_t i = 0; i < actual.size(); ++i)
            error = std::max(error, std::abs(actual[i] - expected[i]));
        check(error < 0.0001f, "resampler boundary continuity");
        SpscAudioRing ring(4096);
        ring.setTargetFrames(960);
        std::vector<float> input(2000 * 2, 0.3f), output(480 * 2);
        check(ring.write(input.data(), 2000) == 2000, "ring writes");
        check(ring.readAdaptive(output.data(), 480, 1.0002) == 480, "ring reads");
        ring.requestReprime();
        ring.reset();
        check(!ring.consumeRecoveryRequest(), "reset clears recovery");
        check(ring.readAdaptive(output.data(), 480, 1) == 0, "empty ring");
        for (float v : output)
            check(v == 0, "silence");
        ring.write(input.data(), 2000);
        for (int i = 0; i < 12; ++i)
            ring.readAdaptive(output.data(), 480, 1);
        for (float v : output)
            check(std::isfinite(v), "underrun finite");
        ring.reset();
        std::atomic<bool> done{false};
        std::thread producer([&] {
            for (int i = 0; i < 10000; ++i) {
                ring.write(input.data(), 128);
                if (i % 11 == 0)
                    ring.requestReprime();
            }
            done = true;
        });
        while (!done.load()) {
            ring.readAdaptive(output.data(), 480, 1.0002, ring.consumeRecoveryRequest());
            for (float v : output)
                if (!std::isfinite(v))
                    std::terminate();
        }
        producer.join();
        check(ring.availableFrames() <= ring.capacityFrames(), "bounded ring");
        std::cout << "PASS: " << checks << " assertions\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
