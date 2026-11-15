// halfscale.h: 2x2 -> 1 RGB565 downscale for 240 px panels (SpotPear 1.28 AI Ball).
// Each output pixel is the per-channel rounded mean of its 2x2 block.
#include "framework.h"
#include "../halfscale.h"

TEST(halfscale, color_solido_no_cambia) {
  for (uint16_t c : {0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0x18C4, 0x7BEF})
    CHECK_EQ(avg4_565(c, c, c, c), c);
}

TEST(halfscale, canales_independientes_sin_acarreo) {
  CHECK_EQ(avg4_565(0xF800, 0xF800, 0x0000, 0x0000), 0x8000);  // R 31,31,0,0 -> 16 (round half up)
  CHECK_EQ(avg4_565(0x07E0, 0x0000, 0x0000, 0x0000), 0x0200);  // G 63,0,0,0 -> 16
  CHECK_EQ(avg4_565(0x001F, 0x001F, 0x001F, 0x0000), 0x0017);  // B 31*3/4 = 23.25 -> 23
  CHECK_EQ(avg4_565(0xFFFF, 0x0000, 0x0000, 0x0000), 0x4208);  // 8,16,8: nothing leaks between channels
}

TEST(halfscale, filas_completas) {
  // 4x2 input -> 2 output pixels; row1 differs from row0 so both rows are used
  uint16_t r0[4] = {0xFFFF, 0xFFFF, 0x0000, 0x0000};
  uint16_t r1[4] = {0xFFFF, 0xFFFF, 0xF800, 0xF800};
  uint16_t out[3] = {1, 1, 0xABCD};
  halfscaleRows565(r0, r1, out, 2);
  CHECK_EQ(out[0], 0xFFFF);
  CHECK_EQ(out[1], 0x8000);
  CHECK_EQ(out[2], 0xABCD);  // writes exactly outW pixels
}
