#pragma once

// Pin map for the Waveshare ESP32-S3-Touch-AMOLED-1.75 family.
// Original 1.75: official hardware reference + pin_config.h in
//   github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75
// 1.75C: official pin_config.h + schematic in
//   github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C
//   (examples/arduino/libraries/board_config/pin_config.h)

#include "board.h"  // eleccion de placa (flags) y pines/capacidades de la Guition

#if !defined(BOARD_GUITION_4848S040)  // Waveshare 1.75 / 1.75C (la Guition vive en board.h)

#define XPOWERS_CHIP_AXP2101

// Shared: 466x466 CO5300 over QSPI, CST9217 over I2C, ES8311 + amp, AXP2101
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK  38
#define LCD_CS    12
#define LCD_WIDTH  466
#define LCD_HEIGHT 466

#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT  11

#define I2S_BCK_IO 9
#define I2S_DI_IO  10
#define I2S_WS_IO  45
#define I2S_DO_IO  8
#define PA         46

#if BOARD_AMOLED_175C
  // 1.75C — verified against Waveshare pin_config.h and the official schematic
  #define LCD_RESET  1
  #define TP_RESET   2
  #define I2S_MCK_IO 16
  // PWR button: Key2 → AXP2101 PWRON (4 s hardware power-off) and SYS_OUT → GPIO3
  // (high while pressed, low while released; 10k pull-up on the board).
  // BOOT / Key1 is GPIO0 (active-low strapping pin; input only at runtime).
  #define BOOT_BUTTON_PIN   0
  #define PWR_SENSE_PIN     3
  #define PWR_SENSE_PRESSED HIGH
  #define BOARD_NAME "ESP32-S3-Touch-AMOLED-1.75C"
#else
  // Original 1.75 — official Waveshare pins. I2S MCLK is GPIO42 (the "16" in
  // Waveshare's older Arduino pin_config.h is wrong; see HARDWARE_REFERENCE.md).
  #define LCD_RESET  39
  #define TP_RESET   40
  #define I2S_MCK_IO 42
  #define SDMMC_CLK  2
  #define SDMMC_CMD  1
  #define SDMMC_DATA 3
  #define SDMMC_CS   41
  #define BOARD_NAME "ESP32-S3-Touch-AMOLED-1.75"
#endif

// Frecuencia de gfx->begin(): QSPI a 80MHz (el flush del framebuffer es el
// cuello de botella del fps; a 40MHz ~56ms por frame).
#define DISPLAY_BEGIN_HZ 80000000

// Tactil CST9217
#define TOUCH_ADDR 0x5A
#define TOUCH_POLLING 0  // tiene linea INT: lecturas a puerta por ISR

// Capacidades de la placa: el firmware degrada lo que valga 0.
#define HAS_PMU 1
#if BOARD_AMOLED_175C
#define HAS_RTC 0  // reloj por software (ver rtcbat.cpp)
#else
#define HAS_RTC 1
#endif
#define HAS_AUDIO 1
#define HAS_SD 1  // "hay almacenamiento de sprites": tarjeta SD en la 1.75; en la 1.75C es LittleFS en flash (ver sdmon.cpp)
#endif  // !BOARD_GUITION_4848S040
