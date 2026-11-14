#pragma once

// Target board for this firmware.
//
//   0 = Waveshare ESP32-S3-Touch-AMOLED-1.75  (original TamaPoke board, microSD) - default
//   1 = Waveshare ESP32-S3-Touch-AMOLED-1.75C (aluminium case, no microSD, 32 MB flash)
//       Experimental community support. Build with -DBOARD_AMOLED_175C=1 and the
//       partition table in boards/175c/partitions.csv (tools/build_175c.sh does both).
//   (Guition 4848S040: ver mas abajo, se elige con -DBOARD_GUITION_4848S040)
//   (SpotPear 1.28" AI Ball: ver mas abajo, se elige con -DBOARD_SPOTPEAR_BALL128)
//
// The default build (CI, web installer, Arduino IDE without extra flags) is the
// original 1.75 and is unchanged by the 1.75C code, which is all behind this flag.
#ifndef BOARD_AMOLED_175C
#define BOARD_AMOLED_175C 0
#endif

// Guition / Sunton ESP32-S3-4848S040 (ST7701S RGB 480x480 + GT911, sin PMU/RTC/audio):
// se elige con el flag de compilacion BOARD_GUITION_4848S040 (-DBOARD_GUITION_4848S040).
// Sus pines y capacidades estan abajo; los de las Waveshare, en pin_config.h.
//
// HAS_SD significa "hay almacenamiento externo para sprites" (tarjeta SD, o LittleFS
// en flash en la 1.75C), no necesariamente una tarjeta SD. Regla: el codigo de juego/UI
// nunca toca GPIO crudos; solo macros de placa, y el firmware degrada lo que valga 0.

#if defined(BOARD_GUITION_4848S040)
// Guition / Sunton ESP32-S3-4848S040: panel IPS 4.0" 480x480, driver ST7701S.
// Pixels por bus paralelo RGB de 16 bits (DMA de la S3); la secuencia de
// init del ST7701S va por SPI de 3 hilos. Táctil GT911 por I2C.
// Fuente de los pines: la propia libreria Arduino_GFX (config
// ESP32_4848S040_86BOX_GUITION de examples/PDQgraphicstest/Arduino_GFX_dev_device.h),
// que coincide con los ejemplos de la placa (Yoradio myoptions_4848S040.h).
//
// ATENCION: el SKU original del proyecto (Waveshare AMOLED) no comparte ningun
// pin util con esta placa. No mezclar: una compilacion, una placa.

// --- Pantalla ST7701S (480x480) ---------------------------------------------
#define LCD_WIDTH 480
#define LCD_HEIGHT 480

// SPI de 3 hilos para comandos/init del ST7701S (no lleva linea DC: el modo
// D/C viaja como 9º bit; tampoco hay reset cableado en el modulo)
#define LCD_CS 39
#define LCD_SCLK 48
#define LCD_MOSI 47
#define LCD_BL 38  // retroiluminacion (PWM)
// Suelo de brillo (power.h), solo con el bicho durmiendo o la pantalla atenuada (nunca con
// la pantalla despierta, donde manda el nivel de ajustes) y sin pasar del brillo que tendria
// despierta con ese mismo nivel. Sin boton PWR ni despertar por tactil, la unica forma de
// despertar al bicho es pulsar LUZ; la paleta nocturna es muy oscura y un IPS con poco duty
// PWM se lee mal (80/255 resultaba demasiado tenue). Sin AMOLED que proteger ni bateria,
// 170/255 es aceptable; se puede ajustar aqui.
#define BRIGHTNESS_FLOOR 170
#define PANEL_ROUND 0   // panel cuadrado: los fondos de menu son rectangulos

// Bus paralelo RGB de 16 bits (datos de pixel por DMA)
#define RGB_DE 18
#define RGB_VSYNC 17
#define RGB_HSYNC 16
#define RGB_PCLK 21
#define RGB_R0 11
#define RGB_R1 12
#define RGB_R2 13
#define RGB_R3 14
#define RGB_R4 0
#define RGB_G0 8
#define RGB_G1 20
#define RGB_G2 3
#define RGB_G3 46
#define RGB_G4 9
#define RGB_G5 10
#define RGB_B0 4
#define RGB_B1 5
#define RGB_B2 6
#define RGB_B3 7
#define RGB_B4 15

// Reloj de pixel: 12MHz, el valor estable de la libreria Arduino_GFX para
// este modulo (480x480 x 2 bytes ~ 460KB por frame; con PSRAM OPI va sobrado).
#define RGB_PCLK_HZ 12000000
// begin() del panel RGB: -1 = usar el prefer_speed del rgbpanel (RGB_PCLK_HZ)
#define DISPLAY_BEGIN_HZ (-1)

// --- Táctil capacitivo GT911 por I2C ----------------------------------------
// Ojo: la mayoria de SKUs del 4848S040 NO sacan INT ni RESET del GT911: hay
// que sondearlo por I2C (TOUCH_POLLING 1). La SCL es GPIO45 en el pinout
// oficial de esta placa (no 20: ese pin es G1 del bus RGB).
#define IIC_SDA 19
#define IIC_SCL 45
#define TOUCH_ADDR 0x5D  // GT911 con INT alto a reposo (el de la mayoria de modulos)
#define TOUCH_POLLING 1

// --- Capacidades -------------------------------------------------
// El panel suelto no lleva AXP2101, PCF85063, ES8311 ni SDMMC. El SD del
// 4848S040 es SPI (CS 42, CLK 48, MOSI 47, MISO 41). SCK/MOSI son los mismos
// pines que el bus 3-wire de comandos de la pantalla, que solo se usa dentro de
// gfx->begin(): el SD se monta despues, con LCD_CS alto. Sin probar en hardware.
#define HAS_PMU 0
#define HAS_RTC 0
#define HAS_AUDIO 0
#define BOARD_NAME "Guition 4848S040"
#define HAS_SD 1
#define SD_SPI 1
#define SD_SPI_CS 42
#define SD_SPI_SCK 48   // == LCD_SCLK
#define SD_SPI_MOSI 47  // == LCD_MOSI
#define SD_SPI_MISO 41
#define SD_SPI_HZ 10000000

#elif defined(BOARD_SPOTPEAR_BALL128)
// SpotPear "AI Voice Robot Ball" (ESP32-S3 N16R8, 1.28" round GC9A01A 240x240,
// CST816D touch, ES8311, microSD, battery). Experimental. The stock XiaoZhi
// firmware is replaced entirely. Pins from 78/xiaozhi-esp32
// main/boards/spotpear/sp-esp32-s3-1.28-box/config.h and the SpotPear wiki.
//
// The UI is NOT redrawn for 240 px: it is drawn unchanged into a 480x480 canvas
// (466 design centred by UI_DX = 7, as on the Guition) and every flush averages
// each 2x2 block into one panel pixel (halfscale.h). Touch is scaled x2 back.

// --- Display GC9A01A over SPI ---------------------------------------------
#define LCD_WIDTH 480     // logical canvas the UI draws into
#define LCD_HEIGHT 480
#define PANEL_WIDTH 240   // physical panel
#define PANEL_HEIGHT 240
#define PANEL_ROUND 1
#define LCD_SCLK 4
#define LCD_MOSI 2
#define LCD_CS 5
#define LCD_DC 47
#define LCD_RESET 38
#define LCD_BL 42         // backlight, PWM, ACTIVE LOW
// Orientation: Arduino_GC9A01 rotation 0 (MADCTL 0x08) is upright and not
// mirrored on the ball (XiaoZhi's "mirror X" is relative to esp_lcd's default)
// SPI clock. Measured flush (2:1 shrink + transfer): 49 ms at 40 MHz,
// 38 ms at 80 MHz, image clean at 80 on the ball
#define DISPLAY_BEGIN_HZ 80000000

// --- Touch CST816D, on its OWN I2C bus (Wire1) ------------------------------
#define TP_SDA 11
#define TP_SCL 7
#define TP_INT 12
#define TP_RESET 6
#define TOUCH_ADDR 0x15
#define TOUCH_POLLING 0   // has an INT line
#define TOUCH_WIRE Wire1

// --- Audio: ES8311 on Wire (same I2S pins as the 1.75C) ---------------------
#define IIC_SDA 15
#define IIC_SCL 14
#define I2S_MCK_IO 16
#define I2S_BCK_IO 9
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define I2S_DI_IO 10
#define PA 46

// --- microSD, SDMMC 1-bit ---------------------------------------------------
#define SDMMC_CLK 17
#define SDMMC_CMD 18
#define SDMMC_DATA 21

// --- Power and buttons ------------------------------------------------------
// The board only stays on from battery while GPIO3 is held HIGH: setup()
// drives it first thing.
#define POWER_LATCH_PIN 3
// Battery gauge (rtcbat.cpp): raw ADC on GPIO1 mapped with XiaoZhi's table
// (batcurve.h), charge status on GPIO41 (pull-up, HIGH = charging). No VBUS
// sense, so the firmware still treats the ball as always on USB.
#define BAT_ADC_PIN 1
#define BAT_CHARGE_PIN 41
#define BOOT_BUTTON_PIN 0

// --- Capabilities -----------------------------------------------------------
// No AXP2101 and no PCF85063: same degrade paths as the Guition.
#define HAS_PMU 0
#define HAS_RTC 0
#define HAS_AUDIO 1
#define HAS_SD 1
#define SD_SPI 0
#define BOARD_NAME "SpotPear ESP32-S3 1.28 AI Ball"
#endif

// panel redondo (AMOLED 466) o cuadrado; la placa puede redefinirlo
#ifndef PANEL_ROUND
#define PANEL_ROUND 1
#endif

// SD por SPI (en vez de SDMMC); solo lo activa la placa que lo necesite
#ifndef SD_SPI
#define SD_SPI 0
#endif
