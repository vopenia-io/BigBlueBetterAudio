Pod::Spec.new do |spec|
  spec.name         = "BBBACore"
  spec.version      = "1.0.0"
  spec.summary      = "BigBlueBetterAudio shared C/C++ noise-suppression core (RNNoise + Faust)."
  spec.description  = <<-DESC
    The BBBA voice processor for iOS apps: the shared `mobile/core/` C/C++
    sources, the Objective-C++ facade `BBBAEngine`, and the RNNoise voice
    isolation sources with their model weights. The podspec sits at the
    repository root because CocoaPods rejects source paths that leave the pod
    root, and RNNoise lives in `deps/`.

    Swift code uses `BBBAEngine`; the C/C++ stays internal.
  DESC

  spec.homepage     = "https://github.com/trummerschlunk/BigBlueBetterAudio"
  spec.license      = { :type => "GPLv3", :file => "LICENSE" }
  spec.authors      = ["Klaus Scheuermann", "Filipe Coelho"]

  spec.ios.deployment_target = "15.0"
  spec.osx.deployment_target = "10.15"

  # Consumed with `pod 'BBBACore', :path => '<this repository>'`; the :git/:tag
  # source only matters if the pod is ever published to a spec repo.
  spec.source = { :git => "https://github.com/trummerschlunk/BigBlueBetterAudio.git", :tag => spec.version.to_s }

  # Explicit RNNoise sources, the same list as mobile/CMakeLists.txt. Not all of
  # deps/rnnoise/src/*.c: dump_features.c / dump_rnnoise_tables.c /
  # write_weights.c are training tools, they #include "src/_kiss_fft_guts.h"
  # with a path that only resolves from the rnnoise repo root, and they are not
  # needed at runtime.
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
