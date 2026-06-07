// Copyright 2026
// SPDX-License-Identifier: GPL-3.0-or-later
//
// bbba-core: platform-agnostic C API for the BigBlueBetterAudio voice
// processor (RNNoise voice isolation + VAD gate + Faust aesthetic chain).
// No platform, plugin-framework, LiveKit or app dependencies — this is the
// shared core consumed by the Android (JNI) and iOS (ObjC++) bindings.
//
// Sample convention: mono float samples in [-1, 1]. RNNoise requires 48 kHz.
#ifndef BBBA_CORE_H
#define BBBA_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BbbaCore BbbaCore;

// Create a mono processor for the given sample rate (use 48000).
BbbaCore* bbba_create(int sampleRate);

// Free a processor created by bbba_create().
void bbba_destroy(BbbaCore* core);

// Set a parameter by BBBA symbol. Recognized symbols:
//   "intensity"      0..100  RNNoise dry/wet (percent wet; 100 = full isolation)
//   "pre_gain"       dB      input gain
//   "post_gain"      dB      output gain (pre-limiter)
//   "leveler_target" dB      multiband compressor base threshold
//   "sb_strength"    0..100  spectral balancer strength
//   "mb_strength"    0..100  multiband compressor strength
// Unknown symbols are ignored.
void bbba_set_param(BbbaCore* core, const char* symbol, float value);

// Process `frames` mono samples. `in` and `out` may alias (in-place safe).
// Samples are floats in [-1, 1]. Any block size is accepted; the core buffers
// internally into RNNoise's 480-sample (10 ms @ 48 kHz) frames.
void bbba_process(BbbaCore* core, const float* in, float* out, int frames);

// Algorithmic latency in samples introduced by the processor.
int bbba_get_latency(const BbbaCore* core);

#ifdef __cplusplus
}
#endif

#endif  // BBBA_CORE_H
