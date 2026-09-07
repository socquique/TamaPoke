#pragma once

// Elige tu placa descomentando UNA linea. Por defecto (sin tocar nada) se
// compila para la Waveshare ESP32-S3-Touch-AMOLED-1.75 original -- checkouts
// existentes no se ven afectados por este archivo.
//
// #define BOARD_ELECROW_CROWPANEL_128

#if !defined(BOARD_ELECROW_CROWPANEL_128)
#define BOARD_WAVESHARE_AMOLED
#endif
