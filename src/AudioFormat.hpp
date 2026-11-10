#pragma once

#include <windows.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace audioduplo {

inline bool IsExtensibleSubtype(const WAVEFORMATEX *f, const GUID &subtype) {
    if (!f || f->wFormatTag != WAVE_FORMAT_EXTENSIBLE || f->cbSize < 22)
        return false;
    const auto *ex = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(f);
    return IsEqualGUID(ex->SubFormat, subtype) != FALSE;
}

inline bool IsFloatFormat(const WAVEFORMATEX *f) {
    return f && (f->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                 IsExtensibleSubtype(f, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));
}

inline bool IsPcmFormat(const WAVEFORMATEX *f) {
    return f && (f->wFormatTag == WAVE_FORMAT_PCM || IsExtensibleSubtype(f, KSDATAFORMAT_SUBTYPE_PCM));
}

inline float ClampAudio(float x) {
    return std::isfinite(x) ? std::clamp(x, -1.0f, 1.0f) : 0.0f;
}

inline bool IsSupportedCaptureFormat(const WAVEFORMATEX *fmt) {
    if (!fmt || !fmt->nChannels || fmt->nChannels > 8 || fmt->nSamplesPerSec < 8000 ||
        fmt->nSamplesPerSec > 384000)
        return false;
    const bool validBits = IsFloatFormat(fmt)
                               ? (fmt->wBitsPerSample == 32 || fmt->wBitsPerSample == 64)
                               : IsPcmFormat(fmt) && (fmt->wBitsPerSample == 8 || fmt->wBitsPerSample == 16 ||
                                                      fmt->wBitsPerSample == 24 || fmt->wBitsPerSample == 32);
    return validBits && fmt->nBlockAlign == fmt->nChannels * (fmt->wBitsPerSample / 8);
}

inline bool DecodeToStereoFloat(const BYTE *data, UINT32 frames, const WAVEFORMATEX *fmt, bool silent,
                                std::vector<float> &out) {
    if (!IsSupportedCaptureFormat(fmt))
        return false;
    out.resize(static_cast<std::size_t>(frames) * 2);
    if (silent) {
        std::fill(out.begin(), out.end(), 0.0f);
        return true;
    }
    if (!data)
        return false;

    const UINT32 ch = fmt->nChannels;
    const UINT32 bytesPerSample = fmt->nBlockAlign / ch;
    const bool isFloat = IsFloatFormat(fmt);
    const bool isPcm = IsPcmFormat(fmt);
    if (!isFloat && !isPcm)
        return false;

    auto sampleAt = [&](UINT32 frame, UINT32 channel) -> float {
        channel = std::min(channel, ch - 1);
        const BYTE *p = data + static_cast<std::size_t>(frame) * fmt->nBlockAlign +
                        static_cast<std::size_t>(channel) * bytesPerSample;
        if (isFloat && fmt->wBitsPerSample == 64) {
            double v;
            std::memcpy(&v, p, sizeof(v));
            return ClampAudio(static_cast<float>(v));
        }
        if (isFloat && fmt->wBitsPerSample == 32 && bytesPerSample >= 4) {
            float v;
            std::memcpy(&v, p, sizeof(v));
            return std::isfinite(v) ? ClampAudio(v) : 0.0f;
        }
        if (isPcm && fmt->wBitsPerSample == 16 && bytesPerSample >= 2) {
            std::int16_t v;
            std::memcpy(&v, p, sizeof(v));
            return static_cast<float>(v) / 32768.0f;
        }
        if (isPcm && fmt->wBitsPerSample == 8)
            return (static_cast<float>(*p) - 128.0f) / 128.0f;
        if (isPcm && fmt->wBitsPerSample == 24 && bytesPerSample >= 3) {
            std::int32_t v = (static_cast<std::int32_t>(p[0]) | (static_cast<std::int32_t>(p[1]) << 8) |
                              (static_cast<std::int32_t>(p[2]) << 16));
            if (v & 0x00800000)
                v |= 0xFF000000;
            return static_cast<float>(v) / 8388608.0f;
        }
        if (isPcm && fmt->wBitsPerSample == 32 && bytesPerSample >= 4) {
            std::int32_t v;
            std::memcpy(&v, p, sizeof(v));
            return static_cast<float>(v / 2147483648.0);
        }
        return 0.0f;
    };

    // Stereo downmix for standard Windows speaker masks. LFE is intentionally omitted.
    float leftWeight[8]{}, rightWeight[8]{};
    if (ch > 2) {
        DWORD mask = fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE
                         ? reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(fmt)->dwChannelMask
                         : 0;
        if (!mask)
            mask = ch == 6 ? KSAUDIO_SPEAKER_5POINT1 : (ch == 8 ? KSAUDIO_SPEAKER_7POINT1_SURROUND : 0);
        if (!mask)
            return false;
        UINT32 channel = 0;
        for (DWORD bit = 1; bit && channel < ch; bit <<= 1) {
            if (!(mask & bit))
                continue;
            switch (bit) {
            case SPEAKER_FRONT_LEFT:
                leftWeight[channel] = 1;
                break;
            case SPEAKER_FRONT_RIGHT:
                rightWeight[channel] = 1;
                break;
            case SPEAKER_FRONT_CENTER:
            case SPEAKER_BACK_CENTER:
                leftWeight[channel] = rightWeight[channel] = 0.70710678f;
                break;
            case SPEAKER_BACK_LEFT:
            case SPEAKER_SIDE_LEFT:
            case SPEAKER_FRONT_LEFT_OF_CENTER:
                leftWeight[channel] = 0.70710678f;
                break;
            case SPEAKER_BACK_RIGHT:
            case SPEAKER_SIDE_RIGHT:
            case SPEAKER_FRONT_RIGHT_OF_CENTER:
                rightWeight[channel] = 0.70710678f;
                break;
            default:
                break;
            }
            ++channel;
        }
        if (channel != ch)
            return false;
        float totalL = 0, totalR = 0;
        for (UINT32 c = 0; c < ch; ++c) {
            totalL += leftWeight[c];
            totalR += rightWeight[c];
        }
        const float scale = 1.0f / std::max(1.0f, std::max(totalL, totalR));
        for (UINT32 c = 0; c < ch; ++c) {
            leftWeight[c] *= scale;
            rightWeight[c] *= scale;
        }
    }
    for (UINT32 i = 0; i < frames; ++i) {
        float l = sampleAt(i, 0);
        float r = (ch == 1) ? l : sampleAt(i, 1);
        if (ch > 2) {
            l = r = 0;
            for (UINT32 c = 0; c < ch; ++c) {
                const float v = sampleAt(i, c);
                l += v * leftWeight[c];
                r += v * rightWeight[c];
            }
        }
        out[static_cast<std::size_t>(i) * 2] = l;
        out[static_cast<std::size_t>(i) * 2 + 1] = r;
    }
    return true;
}

class StreamingStereoResampler {
  public:
    void reset(double sourceRate, double destRate = 48000.0) {
        sourceRate_ = sourceRate;
        destRate_ = destRate;
        ratio_ = sourceRate_ / destRate_;
        pos_ = 0.0;
        cache_.clear();
    }

    // Stateful linear interpolation. Keeps fractional phase and the boundary sample between packets.
    const std::vector<float> &process(const float *stereo, std::size_t frames) {
        output_.clear();
        if (!stereo || frames == 0)
            return output_;
        cache_.insert(cache_.end(), stereo, stereo + frames * 2);
        const std::size_t cacheFrames = cache_.size() / 2;

        while (static_cast<std::size_t>(std::floor(pos_)) + 1 < cacheFrames) {
            const auto i0 = static_cast<std::size_t>(std::floor(pos_));
            const auto i1 = i0 + 1;
            const double frac = pos_ - static_cast<double>(i0);
            output_.push_back(static_cast<float>(cache_[i0 * 2] + (cache_[i1 * 2] - cache_[i0 * 2]) * frac));
            output_.push_back(
                static_cast<float>(cache_[i0 * 2 + 1] + (cache_[i1 * 2 + 1] - cache_[i0 * 2 + 1]) * frac));
            pos_ += ratio_;
        }

        const std::size_t dropFrames = static_cast<std::size_t>(std::floor(pos_));
        if (dropFrames > 0) {
            const std::size_t keepFrom = std::min(dropFrames, cache_.size() / 2);
            cache_.erase(cache_.begin(), cache_.begin() + static_cast<std::ptrdiff_t>(keepFrom * 2));
            pos_ -= static_cast<double>(keepFrom);
        }
        return output_;
    }

  private:
    double sourceRate_{48000.0};
    double destRate_{48000.0};
    double ratio_{1.0};
    double pos_{0.0};
    std::vector<float> cache_;
    std::vector<float> output_;
};

inline WAVEFORMATEXTENSIBLE MakeInternalFormat() {
    WAVEFORMATEXTENSIBLE f{};
    f.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    f.Format.nChannels = 2;
    f.Format.nSamplesPerSec = 48000;
    f.Format.wBitsPerSample = 32;
    f.Format.nBlockAlign = 8;
    f.Format.nAvgBytesPerSec = 48000 * 8;
    f.Format.cbSize = 22;
    f.Samples.wValidBitsPerSample = 32;
    f.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    f.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    return f;
}

} // namespace audioduplo
