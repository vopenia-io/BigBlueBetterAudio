# Copyright 2026
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Decoupled, platform-agnostic build for the BigBlueBetterAudio mobile core.
# Stage 1 (this file): compile RNNoise + a desktop WAV denoise CLI to validate
# the toolchain and the [-1,1] sample-scaling contract (Gate 1).
cmake_minimum_required(VERSION 3.16)
project(bbba_mobile C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

set(RN  ${CMAKE_CURRENT_SOURCE_DIR}/../deps/rnnoise)
set(MD  ${CMAKE_CURRENT_SOURCE_DIR}/../deps/rnnoise-model-data)

# --- RNNoise (the noise suppression engine), built straight from the submodule ---
add_library(rnnoise STATIC
    ${RN}/src/celt_lpc.c
    ${RN}/src/denoise.c
    ${RN}/src/kiss_fft.c
    ${RN}/src/nnet.c
    ${RN}/src/nnet_default.c
    ${RN}/src/parse_lpcnet_weights.c
    ${RN}/src/pitch.c
    ${RN}/src/rnn.c
    ${RN}/src/rnnoise_tables.c
    ${MD}/rnnoise_data.c
)
target_include_directories(rnnoise PUBLIC ${RN}/include ${RN}/src ${MD})
target_compile_definitions(rnnoise PRIVATE RNNOISE_EXPORT= DISABLE_DEBUG_FLOAT FLOAT_APPROX)

# --- bbba-core: full chain (RNNoise + VAD gate + Faust), C API, no DPF ---
add_library(bbba_core STATIC core/bbba_core.cpp)
target_include_directories(bbba_core PUBLIC core)
target_link_libraries(bbba_core PUBLIC rnnoise)

# --- Gate 1 desktop harnesses ---
add_executable(denoise_cli test/denoise_cli.c)     # RNNoise only
target_link_libraries(denoise_cli rnnoise m)

add_executable(bbba_cli test/bbba_cli.cpp)         # full chain via C API
target_link_libraries(bbba_cli bbba_core m)
