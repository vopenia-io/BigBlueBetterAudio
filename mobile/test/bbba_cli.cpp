// Copyright 2026
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Full-chain harness: runs a mono 16-bit WAV through the complete bbba-core
// pipeline (RNNoise + VAD gate + Faust chain) via the public C API, applying
// the [-1,1] scaling contract. Prints RMS stats. Usage:
//   bbba_cli in.wav out.wav [intensity 0..100]

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bbba_core.h"

#define FRAME 480

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static int16_t* read_wav(const char* path, uint32_t* n_out, uint32_t* sr_out, uint16_t* ch_out) {
    FILE* f = fopen(path, "rb");
    if (!f) { perror("open input"); return nullptr; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return nullptr; }
    fclose(f);
    if (sz < 12 || memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) { free(buf); return nullptr; }
    uint32_t sr = 0; uint16_t ch = 1, bits = 16; uint8_t* data = nullptr; uint32_t datalen = 0;
    long p = 12;
    while (p + 8 <= sz) {
        uint32_t clen = rd32(buf + p + 4); uint8_t* body = buf + p + 8;
        if (!memcmp(buf + p, "fmt ", 4)) { ch = rd16(body + 2); sr = rd32(body + 4); bits = rd16(body + 14); }
        else if (!memcmp(buf + p, "data", 4)) { data = body; datalen = clen; }
        p += 8 + clen + (clen & 1);
    }
    if (!data || bits != 16) { fprintf(stderr, "need 16-bit PCM\n"); free(buf); return nullptr; }
    uint32_t nsamp = datalen / 2;
    int16_t* out = (int16_t*)malloc(nsamp * sizeof(int16_t));
    memcpy(out, data, nsamp * sizeof(int16_t));
    free(buf);
    *n_out = nsamp; *sr_out = sr; *ch_out = ch; return out;
}

static void write_wav(const char* path, const int16_t* s, uint32_t n, uint32_t sr, uint16_t ch) {
    FILE* f = fopen(path, "wb");
    uint32_t datalen = n * 2, riff = 36 + datalen, byterate = sr * ch * 2, fmtlen = 16;
    uint16_t blockalign = ch * 2, pcm = 1, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); fwrite(&fmtlen, 4, 1, f); fwrite(&pcm, 2, 1, f);
    fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&byterate, 4, 1, f);
    fwrite(&blockalign, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&datalen, 4, 1, f); fwrite(s, 2, n, f); fclose(f);
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s in.wav out.wav [intensity 0..100]\n", argv[0]); return 1; }
    uint32_t n, sr; uint16_t ch;
    int16_t* in = read_wav(argv[1], &n, &sr, &ch);
    if (!in) return 1;
    fprintf(stderr, "input: %u samples, %u Hz, %u ch\n", n, sr, ch);
    if (sr != 48000) fprintf(stderr, "WARNING: RNNoise expects 48000 Hz, got %u\n", sr);

    BbbaCore* core = bbba_create((int)sr);
    if (argc >= 4) bbba_set_param(core, "intensity", (float)atof(argv[3]));
    fprintf(stderr, "latency: %d samples (%.1f ms)\n", bbba_get_latency(core),
            1000.0 * bbba_get_latency(core) / sr);

    float* fin = (float*)malloc(n * sizeof(float));
    float* fout = (float*)calloc(n, sizeof(float));
    for (uint32_t i = 0; i < n; ++i) fin[i] = in[i] / 32768.0f;     // int16 -> [-1,1]

    // process in 480-sample blocks to mimic a real capture callback
    for (uint32_t i = 0; i < n; i += FRAME) {
        int blk = (int)((i + FRAME <= n) ? FRAME : (n - i));
        bbba_process(core, fin + i, fout + i, blk);
    }

    int16_t* out = (int16_t*)malloc(n * sizeof(int16_t));
    double in_e = 0, out_e = 0;
    for (uint32_t i = 0; i < n; ++i) {
        float v = fout[i] * 32768.0f;                                // [-1,1] -> int16
        if (v > 32767.f) v = 32767.f; if (v < -32768.f) v = -32768.f;
        out[i] = (int16_t)lrintf(v);
        in_e += (double)in[i] * in[i];
        out_e += (double)out[i] * out[i];
    }
    write_wav(argv[2], out, n, sr, ch);

    double in_rms = sqrt(in_e / n), out_rms = sqrt(out_e / n);
    fprintf(stderr, "in_rms=%.1f out_rms=%.1f reduction=%.1f dB\n",
            in_rms, out_rms, 20.0 * log10((out_rms + 1e-9) / (in_rms + 1e-9)));

    bbba_destroy(core);
    free(in); free(out); free(fin); free(fout);
    return 0;
}
