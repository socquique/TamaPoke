#pragma once
#include <stdint.h>

// 2:1 downscale of RGB565 for 240 px panels that show the 480 px canvas
// (SpotPear 1.28 AI Ball). Each output pixel is the per-channel mean of its
// 2x2 block, rounded half up: a solid colour stays exactly the same, and no
// channel carries into the next one. Header-only; only that board includes it.

inline uint16_t avg4_565(uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
  uint32_t r = (a >> 11) + (b >> 11) + (c >> 11) + (d >> 11);
  uint32_t g = ((a >> 5) & 0x3F) + ((b >> 5) & 0x3F) + ((c >> 5) & 0x3F) + ((d >> 5) & 0x3F);
  uint32_t bl = (a & 0x1F) + (b & 0x1F) + (c & 0x1F) + (d & 0x1F);
  return (uint16_t)((((r + 2) >> 2) << 11) | (((g + 2) >> 2) << 5) | ((bl + 2) >> 2));
}

// row0/row1: two consecutive source rows of 2*outW pixels; writes outW pixels to out
inline void halfscaleRows565(const uint16_t *row0, const uint16_t *row1, uint16_t *out, int outW) {
  for (int i = 0; i < outW; i++, row0 += 2, row1 += 2)
    out[i] = avg4_565(row0[0], row0[1], row1[0], row1[1]);
}
