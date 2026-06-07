Pod::Spec.new do |spec|
  spec.name         = "BBBACore"
  spec.version      = "1.0.0"
  spec.summary      = "BigBlueBetterAudio shared C/C++ noise-suppression core (RNNoise + Faust)."
  spec.description  = <<-DESC
    Dedicated pod that vendors the BBBA C/C++ noise-suppression core (the
    shared `mobile/core/` files, the iOS ObjC++ façade `BBBAEngine`, and the
    RNNoise voice-isolation backbone + model weights) inside its own pod
    root so CocoaPods doesn't reject `../`-relative source paths.

    Consumers (LiveKitClientKotlin) link against this pod and use the ObjC
    `BBBAEngine` from Swift; the C/C++ stays internal.
  DESC

  spec.homepage     = "https://github.com/trummerschlunk/BigBlueBetterAudio"
  spec.license      = { :type => "GPLv3", :file => "LICENSE" }
  spec.author       = "Guido Aulisi (BBBA) / Vopenia mobile integration"

  spec.ios.deployment_target = "13.0"
  spec.osx.deployment_target = "10.15"

  # Locally referenced via :path from the consumer Gradle's cocoapods block;
  # the :git/:tag here is only used if BBBACore ever gets published.
  spec.source = { :git => "https://github.com/trummerschlunk/BigBlueBetterAudio.git", :tag => spec.version.to_s }

  # Explicit RNNoise sources — same list as the Android CMakeLists
  # (vopenia-participants/src/androidMain/cpp/CMakeLists.txt). NOT all of
  # deps/rnnoise/src/*.c: dump_features.c / dump_rnnoise_tables.c /
  # write_weights.c are training-tool sources, they #include
  # "src/_kiss_fft_guts.h" with a path that only resolves from the rnnoise
  # repo root and they're not needed at runtime.
  spec.source_files = [
    "mobile/ios/BBBAEngine.h",
    "mobile/ios/BBBAEngine.mm",
    "mobile/core/bbba_core.cpp",
    "mobile/core/bbba_core.h",
    "mobile/core/bbba_faust.hpp",
    "mobile/core/bbba_faust_generated.h",
    "mobile/core/bbba_smoother.hpp",
    "deps/rnnoise/src/celt_lpc.c",
    "deps/rnnoise/src/denoise.c",
    "deps/rnnoise/src/kiss_fft.c",
    "deps/rnnoise/src/nnet.c",
    "deps/rnnoise/src/nnet_default.c",
    "deps/rnnoise/src/parse_lpcnet_weights.c",
    "deps/rnnoise/src/pitch.c",
    "deps/rnnoise/src/rnn.c",
    "deps/rnnoise/src/rnnoise_tables.c",
    "deps/rnnoise/src/*.h",
    "deps/rnnoise-model-data/rnnoise_data.c",
    "deps/rnnoise-model-data/rnnoise_data.h",
  ]

  # Only BBBAEngine.h is exposed to Swift consumers; everything else is
  # implementation detail of the C/C++ core.
  spec.public_header_files  = "mobile/ios/BBBAEngine.h"
  spec.private_header_files = [
    "mobile/core/*.h",
    "mobile/core/*.hpp",
    "deps/rnnoise/src/*.h",
    "deps/rnnoise-model-data/*.h",
  ]

  spec.libraries = "c++"

  spec.pod_target_xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD"   => "gnu++14",
    "CLANG_CXX_LIBRARY"             => "libc++",
    "GCC_PREPROCESSOR_DEFINITIONS"  => "RNNOISE_EXPORT= DISABLE_DEBUG_FLOAT=1 FLOAT_APPROX=1",
    "HEADER_SEARCH_PATHS"           => [
      "\"$(PODS_TARGET_SRCROOT)/mobile/core\"",
      "\"$(PODS_TARGET_SRCROOT)/deps/rnnoise/include\"",
      "\"$(PODS_TARGET_SRCROOT)/deps/rnnoise/src\"",
      "\"$(PODS_TARGET_SRCROOT)/deps/rnnoise-model-data\"",
    ].join(" "),
    # ObjC++ for BBBAEngine.mm bridging is automatic via the .mm extension;
    # bbba_core.cpp compiles as plain C++.
  }
end
