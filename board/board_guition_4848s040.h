#pragma once

// Guition / Sunton ESP32-S3-4848S040: panel IPS 4.0" 480x480, driver ST7701S.
// Pixels por bus paralelo RGB de 16 bits (DMA de la S3); la secuencia de
// init del ST7701S va por SPI de 3 hilos. Táctil GT911 por I2C.
// Fuente de los pines: la propia libreria Arduino_GFX (config
// ESP32_4848S040_86BOX_GUITION de examples/PDQgraphicstest/Arduino_GFX_dev_device.h),
// que coincide con los ejemplos de la placa (Yoradio myoptions_4848S040.h).
// Ver tambien DOCS/guition-4848s040-screen-notes.md.
//
// ATENCON: el SKU original del proyecto (Waveshare AMOLED) no comparte ningun
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
// Suelo de brillo (power.h): sin boton PWR ni despertar por tactil, la unica forma de
// despertar al bicho es pulsar LUZ. La paleta nocturna es muy oscura y un IPS con poco duty
// PWM se lee mal (80/255 resultaba demasiado tenue). Esta placa no tiene AMOLED que proteger
// ni bateria, asi que 170/255 es aceptable; se puede ajustar aqui.
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

// --- Capacidades (plan.md seccion 5) -----------------------------------------
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
