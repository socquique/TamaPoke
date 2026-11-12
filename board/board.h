#pragma once

// Seleccion de placa en tiempo de compilacion (plan.md seccion 5, la capa
// "board/" que sustituye al pin_config.h concreto del proyecto base).
//
//   - Placa por defecto: Waveshare ESP32-S3-Touch-AMOLED-1.75 (CO5300 QSPI,
//     CST9217, AXP2101, PCF85063, ES8311, SDMMC).
//   - Define BOARD_GUITION_4848S040 (flag de compilacion, p. ej.
//     -DBOARD_GUITION_4848S040) para el panel Guition/Sunton ESP32-S3-4848S040
//     (ST7701S RGB 480x480 + GT911 por I2C, sin PMU/RTC/bateria/audio).
//
// Cada cabecera de placa define los pines y las capacidades (HAS_*) que el
// firmware usa para degradar funciones (plan.md: "degradar, no petar").
// Regla: el codigo de juego/UI nunca toca GPIO crudos; solo macros de placa.

#if defined(BOARD_GUITION_4848S040)
#include "board_guition_4848s040.h"
#else
#include "board_waveshare_amoled.h"
#endif

// panel redondo (AMOLED 466) o cuadrado; la cabecera de placa puede redefinirlo
#ifndef PANEL_ROUND
#define PANEL_ROUND 1
#endif

// SD por SPI (en vez de SDMMC); solo lo activa la cabecera de placa que lo necesite
#ifndef SD_SPI
#define SD_SPI 0
#endif
