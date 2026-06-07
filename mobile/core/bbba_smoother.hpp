// Copyright 2026
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Linear value smoother — a self-contained reimplementation that is
// numerically identical to DPF's LinearValueSmoother
// (deps/dpf/distrho/extra/ValueSmoother.hpp, ISC-licensed), used by the
// original PluginDSP.cpp for the mute and dry/wet ramps. Kept dependency-free
// so bbba-core needs only RNNoise + the Faust DSP (no DPF on mobile).
//
// `step` is the per-sample magnitude (recomputed only when sampleRate / tau /
// target change); next() advances `mem` toward `target` by that magnitude,
// clamped so it never overshoots — i.e. a linear ramp of duration `tau`.
#pragma once

#include <cmath>

class LinearValueSmoother {
    float step = 0.f;
    float target = 0.f;
    float mem = 0.f;
    float tau = 0.f;          // ramp duration, seconds
    float sampleRate = 0.f;

    void updateStep() { step = std::fabs(target - mem) / (tau * sampleRate); }

public:
    void setSampleRate(float sr) { sampleRate = sr; updateStep(); }
    void setTimeConstant(float seconds) { tau = seconds; updateStep(); }
    void setTargetValue(float v) { target = v; updateStep(); }
    void clearToTargetValue() { mem = target; }

    float next() {
        const float y0 = mem;
        const float dy = target - y0;
        return (mem = y0 + std::copysign(std::fmin(std::fabs(dy), step), dy));
    }
};
