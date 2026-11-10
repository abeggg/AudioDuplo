#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace audioduplo {

class SpscAudioRing {
  public:
    explicit SpscAudioRing(std::size_t capacityFrames)
        : capacityFrames_(capacityFrames), data_(capacityFrames * 2, 0.0f) {}

    void reset() {
        writeFrame_.store(0, std::memory_order_release);
        readFloor_.store(0, std::memory_order_release);
        readPos_ = 0.0;
        primed_ = false;
        recoveryRequested_.store(false);
        fadeInRemaining_ = 0;
        lastL_ = 0.0f;
        lastR_ = 0.0f;
    }

    std::size_t capacityFrames() const {
        return capacityFrames_;
    }

    std::size_t availableFrames() const {
        const auto w = writeFrame_.load(std::memory_order_acquire);
        const auto r = readFloor_.load(std::memory_order_acquire);
        const auto diff = (w > r) ? (w - r) : 0;
        return static_cast<std::size_t>(std::min<std::uint64_t>(diff, capacityFrames_));
    }

    std::size_t write(const float *stereo, std::size_t frames) {
        if (!stereo || frames == 0)
            return 0;

        const auto w = writeFrame_.load(std::memory_order_relaxed);
        const auto r = readFloor_.load(std::memory_order_acquire);
        const auto used = (w > r) ? (w - r) : 0;
        const std::size_t freeFrames =
            used >= capacityFrames_ ? 0 : capacityFrames_ - static_cast<std::size_t>(used);
        const std::size_t n = std::min(frames, freeFrames);
        if (n == 0)
            return 0;

        std::size_t pos = static_cast<std::size_t>(w % capacityFrames_);
        const std::size_t first = std::min(n, capacityFrames_ - pos);
        std::memcpy(&data_[pos * 2], stereo, first * 2 * sizeof(float));
        if (first < n) {
            std::memcpy(&data_[0], stereo + first * 2, (n - first) * 2 * sizeof(float));
        }
        writeFrame_.store(w + n, std::memory_order_release);
        return n;
    }

    void setTargetFrames(std::size_t target) {
        targetFrames_ = std::clamp<std::size_t>(target, 2, capacityFrames_ / 2);
    }

    std::size_t targetFrames() const {
        return targetFrames_;
    }

    void requestReprime() {
        recoveryRequested_.store(true, std::memory_order_release);
    }
    bool consumeRecoveryRequest() {
        return recoveryRequested_.exchange(false, std::memory_order_acq_rel);
    }

    // Single-consumer only. ratio > 1.0 drains the ring slightly faster.
    // Returns number of real audio frames produced. Remainder is click-free fade/silence.
    std::size_t readAdaptive(float *out, std::size_t outFrames, double ratio, bool forceFadeDown = false) {
        if (!out || outFrames == 0)
            return 0;

        const auto wNow = writeFrame_.load(std::memory_order_acquire);
        const auto rNow = readFloor_.load(std::memory_order_acquire);
        const auto availNow = (wNow > rNow) ? static_cast<std::size_t>(wNow - rNow) : 0;

        if (forceFadeDown) {
            const std::size_t fade = std::min<std::size_t>(outFrames, 240); // 5 ms @ 48 kHz
            for (std::size_t i = 0; i < fade; ++i) {
                const float g = 1.0f - static_cast<float>(i + 1) / static_cast<float>(fade);
                out[i * 2] = lastL_ * g;
                out[i * 2 + 1] = lastR_ * g;
            }
            std::fill(out + fade * 2, out + outFrames * 2, 0.0f);
            primed_ = false;
            fadeInRemaining_ = 240;
            return 0;
        }

        if (!primed_) {
            if (availNow < targetFrames_) {
                std::fill(out, out + outFrames * 2, 0.0f);
                lastL_ = lastR_ = 0.0f;
                return 0;
            }
            const auto w = writeFrame_.load(std::memory_order_acquire);
            readPos_ = static_cast<double>(w - targetFrames_);
            readFloor_.store(static_cast<std::uint64_t>(std::floor(readPos_)), std::memory_order_release);
            primed_ = true;
            fadeInRemaining_ = 240;
        }

        ratio = std::clamp(ratio, 0.995, 1.005);
        std::size_t produced = 0;
        for (; produced < outFrames; ++produced) {
            const auto w = writeFrame_.load(std::memory_order_acquire);
            const auto base = static_cast<std::uint64_t>(std::floor(readPos_));
            if (base + 1 >= w)
                break;

            const double frac = readPos_ - static_cast<double>(base);
            const std::size_t i0 = static_cast<std::size_t>(base % capacityFrames_);
            const std::size_t i1 = static_cast<std::size_t>((base + 1) % capacityFrames_);

            float l = static_cast<float>(data_[i0 * 2] + (data_[i1 * 2] - data_[i0 * 2]) * frac);
            float r = static_cast<float>(data_[i0 * 2 + 1] + (data_[i1 * 2 + 1] - data_[i0 * 2 + 1]) * frac);

            if (fadeInRemaining_ > 0) {
                const float g = 1.0f - static_cast<float>(fadeInRemaining_) / 240.0f;
                l *= g;
                r *= g;
                --fadeInRemaining_;
            }

            out[produced * 2] = l;
            out[produced * 2 + 1] = r;
            lastL_ = l;
            lastR_ = r;
            readPos_ += ratio;
        }

        readFloor_.store(static_cast<std::uint64_t>(std::floor(readPos_)), std::memory_order_release);

        if (produced < outFrames) {
            // No block repetition. Fade the last valid sample smoothly to silence.
            const std::size_t missing = outFrames - produced;
            const std::size_t fade = std::min<std::size_t>(missing, 96); // 2 ms @ 48 kHz
            for (std::size_t i = 0; i < fade; ++i) {
                const float g = 1.0f - static_cast<float>(i + 1) / static_cast<float>(fade);
                out[(produced + i) * 2] = lastL_ * g;
                out[(produced + i) * 2 + 1] = lastR_ * g;
            }
            std::fill(out + (produced + fade) * 2, out + outFrames * 2, 0.0f);
            primed_ = false;
            fadeInRemaining_ = 240;
            lastL_ = lastR_ = 0.0f;
        }
        return produced;
    }

  private:
    std::size_t capacityFrames_{};
    std::vector<float> data_;
    std::atomic<std::uint64_t> writeFrame_{0};
    std::atomic<std::uint64_t> readFloor_{0};
    std::atomic<bool> recoveryRequested_{false};

    // Consumer-owned state.
    double readPos_{0.0};
    bool primed_{false};
    std::size_t targetFrames_{960};
    int fadeInRemaining_{0};
    float lastL_{0.0f};
    float lastR_{0.0f};
};

} // namespace audioduplo
