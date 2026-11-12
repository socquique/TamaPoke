#pragma once

// Target board for this firmware.
//
//   0 = Waveshare ESP32-S3-Touch-AMOLED-1.75  (original TamaPoke board, microSD) - default
//   1 = Waveshare ESP32-S3-Touch-AMOLED-1.75C (aluminium case, no microSD, 32 MB flash)
//       Experimental community support. Build with -DBOARD_AMOLED_175C=1 and the
//       partition table in boards/175c/partitions.csv (tools/build_175c.sh does both).
//
// The default build (CI, web installer, Arduino IDE without extra flags) is the
// original 1.75 and is unchanged by the 1.75C code, which is all behind this flag.
#ifndef BOARD_AMOLED_175C
#define BOARD_AMOLED_175C 0
#endif
