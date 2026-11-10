#pragma once

#include "SpscAudioRing.hpp"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace audioduplo {

struct DeviceInfo {
    std::wstring id;
    std::wstring name;
    bool isDefault{false};
    bool looksHandsFree{false};
};

struct EngineStatsSnapshot {
    bool running{false};
    bool ready{false};
    float peak{0};
    std::uint64_t capturedFrames{0};
    std::uint64_t renderedFrames{0};
    double bufferMs{0.0};
    std::uint64_t underruns{0};
    std::uint64_t overruns{0};
    std::uint64_t discontinuities{0};
    int targetLatencyMs{20};
    double driftPpm{0.0};
    std::wstring sourceStatus;
    std::wstring destinationStatus;
    std::wstring lastError;
};

enum class LatencyChoice : int {
    Automatic = 0,
    Ms20 = 20,
    Ms30 = 30,
    Ms40 = 40,
    Ms60 = 60,
    Ms80 = 80,
    Ms120 = 120,
    Ms160 = 160,
};

std::vector<DeviceInfo> EnumerateRenderDevices();
bool LooksHandsFreeName(const std::wstring &name);

class AudioEngine {
  public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine &) = delete;
    AudioEngine &operator=(const AudioEngine &) = delete;

    bool start(const std::wstring &sourceId, const std::wstring &destinationId, LatencyChoice choice);
    void stop();
    void setVolume(float gain) {
        volume_.store(gain < 0 ? 0 : (gain > 1 ? 1 : gain));
    }
    EngineStatsSnapshot snapshot() const;

  private:
    void captureThreadMain();
    void renderThreadMain();
    void setError(const std::wstring &text);
    int configuredTargetMs() const;
    void maybeIncreaseAutomaticLatency(std::uint64_t currentUnderruns);

    static constexpr std::size_t kSampleRate = 48000;
    static constexpr std::size_t kRingCapacityFrames =
        kSampleRate * 2; // 2 s capacity; target occupancy stays 20-80 ms.

    SpscAudioRing ring_{kRingCapacityFrames};
    std::wstring sourceId_;
    std::wstring destinationId_;
    LatencyChoice latencyChoice_{LatencyChoice::Automatic};

    std::atomic<bool> running_{false};
    std::atomic<bool> captureReady_{false}, renderReady_{false};
    std::atomic<float> volume_{1.0f}, peak_{0};
    std::atomic<std::uint64_t> capturedFrames_{0}, renderedFrames_{0};
    HANDLE stopEvent_{nullptr};
    std::thread captureThread_;
    std::thread renderThread_;

    std::atomic<std::uint64_t> underruns_{0};
    std::atomic<std::uint64_t> overruns_{0};
    std::atomic<std::uint64_t> discontinuities_{0};
    std::atomic<int> targetLatencyMs_{20};
    std::atomic<double> driftPpm_{0.0};

    mutable std::mutex textMutex_;
    std::wstring sourceStatus_;
    std::wstring destinationStatus_;
    std::wstring lastError_;

    std::uint64_t lastAutoUnderruns_{0};
    ULONGLONG autoWindowStartMs_{0};
};

} // namespace audioduplo
