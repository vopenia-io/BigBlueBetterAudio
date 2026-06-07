// Copyright 2026
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Objective-C façade over the decoupled bbba-core C API, so Swift (which cannot
// call C++ directly) can drive the BigBlueBetterAudio processor. The C++ stays
// behind this ObjC interface; only this header is exposed to Swift.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface BBBAEngine : NSObject

/// Create a mono processor for the given sample rate (RNNoise requires 48000).
/// Returns nil if the core could not be created.
- (nullable instancetype)initWithSampleRate:(int)sampleRate;

/// Set a BBBA parameter by symbol (e.g. "intensity", "sb_strength").
- (void)setParam:(NSString *)symbol value:(float)value;

/// Process `frames` mono float samples IN PLACE. Input is in WebRTC's int16
/// scale (±32768) — this method converts to/from the core's [-1,1] domain.
- (void)processInPlace:(float *)buffer frames:(int)frames;

@end

NS_ASSUME_NONNULL_END
