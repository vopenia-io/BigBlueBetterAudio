// Copyright 2026
// SPDX-License-Identifier: GPL-3.0-or-later

#import "BBBAEngine.h"

#include "bbba_core.h"
#include <math.h>

@implementation BBBAEngine {
    BbbaCore *_core;
    int _heartbeatCounter;
}

- (nullable instancetype)initWithSampleRate:(int)sampleRate {
    if ((self = [super init])) {
        _core = bbba_create(sampleRate);
        if (_core == nullptr) {
            return nil;
        }
        _heartbeatCounter = 0;
    }
    return self;
}

- (void)dealloc {
    if (_core != nullptr) {
        bbba_destroy(_core);
        _core = nullptr;
    }
}

- (void)setParam:(NSString *)symbol value:(float)value {
    if (_core != nullptr && symbol != nil) {
        bbba_set_param(_core, symbol.UTF8String, value);
    }
}

- (void)processInPlace:(float *)buffer frames:(int)frames {
    if (_core == nullptr || buffer == nullptr || frames <= 0) {
        return;
    }
    constexpr float kToUnit = 1.0f / 32768.0f;   // WebRTC ±32768 -> [-1,1]
    constexpr float kToInt16 = 32768.0f;          // [-1,1] -> WebRTC ±32768

    // Diagnostic: measure RMS energy before and after processing every ~5s.
    // If `rmsBefore` ≈ `rmsAfter` the processor is a (near-)pass-through
    // — the DSP is running but not actually attenuating, or LiveKit's input
    // has already been heavily filtered by WebRTC's built-in APM and there's
    // no headroom for BBBA to remove. If `rmsAfter` is much lower than
    // `rmsBefore` BBBA is suppressing successfully even if the perceptual
    // delta is small (e.g. transient hand-rubbing sounds, which RNNoise
    // doesn't classify as continuous-noise — by design).
    bool logRms = (++_heartbeatCounter % 500) == 0;
    double sumBefore = 0.0;
    if (logRms) {
        for (int i = 0; i < frames; ++i) sumBefore += (double)buffer[i] * (double)buffer[i];
    }

    for (int i = 0; i < frames; ++i) buffer[i] *= kToUnit;
    bbba_process(_core, buffer, buffer, frames);   // in-place safe
    for (int i = 0; i < frames; ++i) buffer[i] *= kToInt16;

    if (logRms) {
        double sumAfter = 0.0;
        for (int i = 0; i < frames; ++i) sumAfter += (double)buffer[i] * (double)buffer[i];
        double rmsBefore = sqrt(sumBefore / frames);
        double rmsAfter  = sqrt(sumAfter  / frames);
        // dB ratio: 20*log10(after/before). 0 dB = no change, negative = BBBA cut.
        double db = (rmsBefore > 1.0) ? 20.0 * log10(rmsAfter / rmsBefore) : 0.0;
        NSLog(@"[BBBA-iOS] RMS in=%.1f out=%.1f Δ=%+.1f dB (frames=%d)",
              rmsBefore, rmsAfter, db, _heartbeatCounter);
    }
}

@end
