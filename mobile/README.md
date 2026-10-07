# BBBA mobile core

A platform-agnostic C/C++ build of the BigBlueBetterAudio voice processor for native
mobile apps (iOS and Android). It runs the same chain as the simplified (mono) plugin
build, without DPF:

1. RNNoise voice isolation, with its voice activity detection (VAD);
2. the VAD-driven mute gate with its 500 ms grace period;
3. the `intensity` dry/wet mix;
4. the Faust chain from `dsp/bbba.dsp`.

`core/bbba_core.cpp` is a port of that path in `plugin/PluginDSP.cpp`.

## Layout

| Path | Contents |
|---|---|
| `core/bbba_core.h` | The public C API |
| `core/bbba_core.cpp` | The processing chain |
| `core/bbba_faust.hpp` | Faust base classes and a symbol-to-parameter map, standing in for DPF |
| `core/bbba_faust_generated.h` | The Faust `mydsp` class, extracted from `plugin/pregen/FaustPlugin.cpp` |
| `core/bbba_smoother.hpp` | Linear value smoother |
| `ios/BBBAEngine.{h,mm}` | Objective-C++ facade for Swift |
| `test/` | Desktop WAV harnesses |
| `tools/extract_faust.sh` | Refreshes `bbba_faust_generated.h` |
| `../BBBACore.podspec` | CocoaPods spec |

## C API

```c
BbbaCore* core = bbba_create(48000);          // RNNoise needs 48 kHz
bbba_set_param(core, "intensity", 100.f);
bbba_process(core, in, out, frames);          // mono float in [-1, 1], in place allowed
bbba_destroy(core);
```

Any block size works: the core buffers into RNNoise's 480-sample frames. The latency
is 3 frames plus 5 ms, 1680 samples (35 ms) at 48 kHz (`bbba_get_latency`).

Parameters, by BBBA symbol: `intensity` (0 to 100, % wet), `pre_gain` and `post_gain`
(dB), `leveler_target` (dB), `sb_strength` and `mb_strength` (0 to 100). Any other
Faust symbol of `dsp/bbba.dsp` works too; unknown symbols are ignored.

The core starts with the Faust defaults (`leveler_target` -18 dB, `sb_strength` and
`mb_strength` 60). The web build (`web/index.html`) starts with `intensity` 100, both
strengths 100 and `leveler_target` -23 dB; set those to sound like it.

## Building

RNNoise comes from the submodule:

```sh
git submodule update --init deps/rnnoise
```

### Desktop

```sh
cmake -S mobile -B mobile/build && cmake --build mobile/build
mobile/build/bbba_cli in.wav out.wav 100      # full chain, mono 16-bit WAV
mobile/build/denoise_cli in.wav out.wav       # RNNoise only
```

### iOS

```ruby
pod 'BBBACore', :path => 'path/to/BigBlueBetterAudio'
```

Swift sees `BBBAEngine` only:

```swift
let engine = BBBAEngine(sampleRate: 48000)
engine?.setParam("intensity", value: 100)
engine?.processInPlace(buffer, frames: Int32(count))   // buffer: UnsafeMutablePointer<Float>
```

`processInPlace` takes samples in the int16 range (±32768), as WebRTC's capture
post-processing hands them over, and converts to and from [-1, 1] itself.

### Android

From the app's NDK CMake build:

```cmake
set(BBBA_MOBILE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/BigBlueBetterAudio/mobile bbba)
target_link_libraries(your_jni_lib bbba_core)
```

The JNI binding belongs to the app; call the C API from it.

## Keeping the Faust code in sync

`core/bbba_faust_generated.h` is a copy of the class that faustpp generates into
`plugin/pregen/FaustPlugin.cpp`. `make pregen` refreshes it. After regenerating the
plugin sources another way, run:

```sh
mobile/tools/extract_faust.sh            # rewrite the header
mobile/tools/extract_faust.sh --check    # exit 1 when it is out of date
```

## License

GPL-3.0-or-later, like the rest of BigBlueBetterAudio.
