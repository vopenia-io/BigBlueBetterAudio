// Copyright 2026
// SPDX-License-Identifier: GPL-3.0-or-later

#import "BBBAEngine.h"

#include "bbba_core.h"

@implementation BBBAEngine {
    BbbaCore *_core;
}

- (nullable instancetype)initWithSampleRate:(int)sampleRate {
    if ((self = [super init])) {
        _core = bbba_create(sampleRate);
        if (_core == nullptr) {
            return nil;
        }
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

    for (int i = 0; i < frames; ++i) buffer[i] *= kToUnit;
    bbba_process(_core, buffer, buffer, frames);   // in-place safe
    for (int i = 0; i < frames; ++i) buffer[i] *= kToInt16;
}

@end
