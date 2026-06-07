// Copyright 2026
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Gate-1 harness: read a mono 16-bit PCM WAV, run it through RNNoise frame by
// frame applying the confirmed [-1,1] scaling contract, write a denoised WAV,
// and print VAD + RMS-reduction stats. This validates the toolchain and the
// sample-scaling convention before any mobile binding is written.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rnnoise.h"

#define FRAME 480

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static int16_t *read_wav(const char *path, uint32_t *n_out, uint32_t *sr_out, uint16_t *ch_out) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("open input"); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return NULL; }
    fclose(f);

    if (sz < 12 || memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) {
        fprintf(stderr, "not a RIFF/WAVE file\n");
        free(buf);
        return NULL;
    }
    uint32_t sr = 0;
    uint16_t ch = 1, bits = 16;
    uint8_t *data = NULL;
    uint32_t datalen = 0;
    long p = 12;
    while (p + 8 <= sz) {
        uint32_t clen = rd32(buf + p + 4);
        uint8_t *body = buf + p + 8;
        if (!memcmp(buf + p, "fmt ", 4)) {
            ch = rd16(body + 2);
            sr = rd32(body + 4);
            bits = rd16(body + 14);
        } else if (!memcmp(buf + p, "data", 4)) {
            data = body;
            datalen = clen;
        }
        p += 8 + clen + (clen & 1);
    }
    if (!data || bits != 16) {
        fprintf(stderr, "need 16-bit PCM with a data chunk (bits=%u)\n", bits);
        free(buf);
        return NULL;
    }
    uint32_t nsamp = datalen / 2;
    int16_t *out = (int16_t *)malloc(nsamp * sizeof(int16_t));
    memcpy(out, data, nsamp * sizeof(int16_t));
    free(buf);
    *n_out = nsamp;
    *sr_out = sr;
    *ch_out = ch;
    return out;
}

static void write_wav(const char *path, const int16_t *s, uint32_t n, uint32_t sr, uint16_t ch) {
    FILE *f = fopen(path, "wb");
    uint32_t datalen = n * 2, riff = 36 + datalen, byterate = sr * ch * 2;
    uint16_t blockalign = ch * 2, pcm = 1, bits = 16, f16 = 16;
    fwrite("RIFF", 1, 4, f);
    fwrite(&riff, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t fmtlen = f16;
    fwrite(&fmtlen, 4, 1, f);
    fwrite(&pcm, 2, 1, f);
    fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f);
    fwrite(&byterate, 4, 1, f);
    fwrite(&blockalign, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&datalen, 4, 1, f);
    fwrite(s, 2, n, f);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s in.wav out.wav\n", argv[0]);
        return 1;
    }
    uint32_t n, sr;
    uint16_t ch;
    int16_t *in = read_wav(argv[1], &n, &sr, &ch);
    if (!in) return 1;
    fprintf(stderr, "input: %u samples, %u Hz, %u ch\n", n, sr, ch);
    if (sr != 48000) fprintf(stderr, "WARNING: RNNoise expects 48000 Hz, got %u\n", sr);
    if (ch != 1) fprintf(stderr, "WARNING: expected mono; treating stream as mono\n");

    DenoiseState *st = rnnoise_create(NULL);
    int16_t *out = (int16_t *)calloc(n, sizeof(int16_t));
    float fin[FRAME], fout[FRAME];
    double vad_sum = 0, in_e = 0, out_e = 0;
    int nframes = 0;
    for (uint32_t i = 0; i + FRAME <= n; i += FRAME) {
        for (int j = 0; j < FRAME; j++) fin[j] = in[i + j] / 32768.0f;  // int16 -> [-1,1]
        float vad = rnnoise_process_frame(st, fout, fin);
        vad_sum += vad;
        nframes++;
        for (int j = 0; j < FRAME; j++) {
            float v = fout[j] * 32768.0f;  // [-1,1] -> int16
            if (v > 32767.f) v = 32767.f;
            if (v < -32768.f) v = -32768.f;
            out[i + j] = (int16_t)lrintf(v);
            in_e += (double)in[i + j] * in[i + j];
            out_e += (double)out[i + j] * out[i + j];
        }
    }
    rnnoise_destroy(st);
    write_wav(argv[2], out, n, sr, ch);

    long tot = (long)nframes * FRAME;
    double in_rms = sqrt(in_e / (tot ? tot : 1));
    double out_rms = sqrt(out_e / (tot ? tot : 1));
    fprintf(stderr, "frames=%d avg_vad=%.3f in_rms=%.1f out_rms=%.1f reduction=%.1f dB\n",
            nframes, nframes ? vad_sum / nframes : 0.0, in_rms, out_rms,
            20.0 * log10((out_rms + 1e-9) / (in_rms + 1e-9)));
    free(in);
    free(out);
    return 0;
}
