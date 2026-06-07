// Copyright 2026
// SPDX-License-Identifier: GPL-3.0-or-later
//
// bbba-core implementation. Faithful, DPF-free port of the simplified (mono)
// processing path from plugin/PluginDSP.cpp: RNNoise denoise + VAD-driven
// mute/grace gate + intensity dry/wet + the Faust aesthetic chain (mydsp).

#include "bbba_core.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "rnnoise.h"
#include "bbba_faust.hpp"     // defines mydsp + BbbaParamUI (no DPF)
#include "bbba_smoother.hpp"

namespace {
constexpr int kFrame = 480;            // RNNoise FRAME_SIZE (10 ms @ 48 kHz)
constexpr float kMuteAttack = 0.005f;
constexpr float kMuteRelease = 0.1f;
constexpr float kGracePeriodMs = 500.f;   // fixed in the simplified build
constexpr float kThreshold = 0.5f;        // VAD unmute threshold

// Simple single-threaded float FIFO (replaces DPF HeapRingBuffer for the
// output path; the dry ring buffer is unused in the mono build and dropped).
struct FloatFifo {
    float* buf = nullptr;
    int cap = 0, head = 0, tail = 0, count = 0;
    void init(int capacity) { cap = capacity; buf = new float[cap]; head = tail = count = 0; }
    void deinit() { delete[] buf; buf = nullptr; }
    void write(const float* src, int n) {
        for (int i = 0; i < n; ++i) {
            buf[head] = src[i];
            head = (head + 1) % cap;
            if (count < cap) ++count; else tail = (tail + 1) % cap;
        }
    }
    void read(float* dst, int n) {
        for (int i = 0; i < n; ++i) {
            if (count > 0) { dst[i] = buf[tail]; tail = (tail + 1) % cap; --count; }
            else dst[i] = 0.f;
        }
    }
    int readable() const { return count; }
};
}  // namespace

struct BbbaCore {
    int sampleRate;
    DenoiseState* denoise;
    mydsp faust;
    BbbaParamUI ui;
    FAUSTFLOAT* zVad = nullptr;   // Faust "vad_ext" control zone

    float bufferIn[kFrame];
    float bufferOut[kFrame];
    float bufferPrevIn[kFrame];
    float bufferOldPrevIn[kFrame];
    int bufferInPos = 0;
    bool processing = false;
    FloatFifo ringOut;

    LinearValueSmoother muteValue;
    LinearValueSmoother denoiserDryValue;
    unsigned gracePeriodInSamples = 0;
    unsigned numUntilGraceOver = 0;
};

BbbaCore* bbba_create(int sampleRate) {
    BbbaCore* c = new BbbaCore();
    c->sampleRate = sampleRate;
    c->denoise = rnnoise_create(nullptr);

    c->faust.init(sampleRate);
    c->faust.buildUserInterface(&c->ui);
    auto it = c->ui.bySymbol.find("vad_ext");
    c->zVad = (it != c->ui.bySymbol.end()) ? it->second : nullptr;

    std::memset(c->bufferPrevIn, 0, sizeof(c->bufferPrevIn));
    std::memset(c->bufferOldPrevIn, 0, sizeof(c->bufferOldPrevIn));
    c->ringOut.init(kFrame * 4);

    c->muteValue.setSampleRate((float)sampleRate);
    c->muteValue.setTimeConstant(kMuteRelease);
    c->muteValue.setTargetValue(0.f);
    c->muteValue.clearToTargetValue();

    c->denoiserDryValue.setSampleRate((float)sampleRate);
    c->denoiserDryValue.setTimeConstant(0.02f);
    c->denoiserDryValue.setTargetValue(0.f);   // intensity default 100% -> fully wet
    c->denoiserDryValue.clearToTargetValue();

    c->gracePeriodInSamples = (unsigned)std::lround(kGracePeriodMs * sampleRate / 1000.0);
    c->numUntilGraceOver = 0;
    return c;
}

void bbba_destroy(BbbaCore* c) {
    if (!c) return;
    rnnoise_destroy(c->denoise);
    c->ringOut.deinit();
    delete c;
}

void bbba_set_param(BbbaCore* c, const char* symbol, float value) {
    if (!c || !symbol) return;
    if (std::strcmp(symbol, "intensity") == 0) {
        c->denoiserDryValue.setTargetValue(1.f - value * 0.01f);
        return;
    }
    auto it = c->ui.bySymbol.find(symbol);
    if (it != c->ui.bySymbol.end()) *(it->second) = value;
}

void bbba_process(BbbaCore* c, const float* in, float* out, int frames) {
    if (!c || frames <= 0) return;

    for (int offset = 0; offset != frames;) {
        const int framesCycle = std::min(kFrame - c->bufferInPos, frames - offset);
        std::memcpy(c->bufferIn + c->bufferInPos, in + offset, framesCycle * sizeof(float));

        if ((c->bufferInPos += framesCycle) == kFrame) {
            c->bufferInPos = 0;

            const float vad = rnnoise_process_frame(c->denoise, c->bufferOut, c->bufferIn);

            // VAD-driven mute with grace period
            if (vad >= kThreshold) {
                c->muteValue.setTimeConstant(kMuteAttack);
                c->muteValue.setTargetValue(1.f);
                c->numUntilGraceOver = c->gracePeriodInSamples;
            } else if (c->gracePeriodInSamples == 0) {
                c->muteValue.setTimeConstant(kMuteRelease);
                c->muteValue.setTargetValue(0.f);
            }
            for (int i = 0; i < kFrame; ++i) {
                if (c->numUntilGraceOver != 0 && --c->numUntilGraceOver == 0) {
                    c->muteValue.setTimeConstant(kMuteRelease);
                    c->muteValue.setTargetValue(0.f);
                }
                c->bufferOut[i] *= c->muteValue.next();
            }

            // denoiser intensity (dry/wet vs the 2-cycles-ago dry signal)
            for (int i = 0; i < kFrame; ++i) {
                const float dry = c->denoiserDryValue.next();
                const float wet = 1.f - dry;
                c->bufferOut[i] = c->bufferOut[i] * wet + c->bufferOldPrevIn[i] * dry;
            }
            std::memcpy(c->bufferOldPrevIn, c->bufferPrevIn, kFrame * sizeof(float));
            std::memcpy(c->bufferPrevIn, c->bufferIn, kFrame * sizeof(float));

            // Faust aesthetic chain, fed the RNNoise VAD; reuse bufferIn as out
            if (c->zVad) *c->zVad = vad;
            FAUSTFLOAT* ins[1] = { c->bufferOut };
            FAUSTFLOAT* outs[1] = { c->bufferIn };
            c->faust.compute(kFrame, ins, outs);

            c->ringOut.write(outs[0], kFrame);
        }

        if (c->processing) {
            c->ringOut.read(out + offset, framesCycle);
        } else {
            std::memset(out + offset, 0, framesCycle * sizeof(float));
            if (c->ringOut.readable() >= kFrame) c->processing = true;
        }
        offset += framesCycle;
    }
}

int bbba_get_latency(const BbbaCore* c) {
    if (!c) return 0;
    return kFrame * 3 + (int)std::lround(c->sampleRate * 0.005);
}
