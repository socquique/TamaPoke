#pragma once
#include "board_select.h"

#if defined(BOARD_ELECROW_CROWPANEL_128)
// ---------------------------------------------------------------------------
// Elecrow CrowPanel 1.28inch-HMI ESP32 Rotary Display
// (ESP32-S3R8, GC9A01 240x240 redondo por SPI, tactil CST816D por I2C).
// Fuente: github.com/Elecrow-RD/CrowPanel-1.28inch-HMI-ESP32-Rotary-Display-240-240-IPS-Round-Touch-Knob-Screen
// y makerguides.com/getting-started-crowpanel-1-28inch-hmi-esp32-rotary-display
// ---------------------------------------------------------------------------

// Pantalla IPS 240x240 redonda, driver GC9A01 por SPI
#define TFT_SCLK 10
#define TFT_MOSI 11
#define TFT_MISO -1
#define TFT_DC    3
#define TFT_CS    9
#define TFT_RES  14
#define TFT_BLK  46          // backlight, PWM (ledcAttach/ledcWrite)

// Resolucion FISICA del panel (240x240). El canvas logico se queda en 466x466
// (ver LCD_WIDTH/LCD_HEIGHT mas abajo) porque todo TamaPoke.ino tiene
// coordenadas absolutas cableadas para 466 y no hay factor de escala en el
// codigo. flushScaled() en TamaPoke.ino reduce el framebuffer de 466x466 a
// esto por vecino-mas-cercano justo antes de mandarlo al panel.
#define TFT_WIDTH 240
#define TFT_HEIGHT 240

// Canvas logico: NO es la resolucion real del panel en esta placa (ver arriba).
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Rails de alimentacion del panel: deben ir HIGH antes de gfx->begin(), si no
// la pantalla queda a oscuras aunque el resto de la placa funcione.
#define LCD_PWR_EN1 1        // LCD_3V3
#define LCD_PWR_EN2 2        // LEDA_3V3 (anodo backlight)

// Tactil capacitivo CST816D por I2C
#define IIC_SDA 6
#define IIC_SCL 7
#define TP_INT 5
#define TP_RESET 13

// Encoder rotativo + boton (no usado todavia por el juego; disponible para
// mapear a navegacion de menus mas adelante)
#define ENCODER_A_PIN 45
#define ENCODER_B_PIN 42
#define ENCODER_BTN_PIN 41

// Audio (ES8311), bateria/RTC (AXP2101/PCF85063) y ranura SD: esta placa no
// tiene ninguno de los tres. audioBegin() y sdBegin() se saltan para esta
// placa en TamaPoke.ino (sus pines I2S/PA reales chocaban con SCLK/CS/
// backlight de arriba, y SD_MMC.begin(..., formatOnFail=true) sin tarjeta se
// queda colgado el tiempo suficiente en frio como para perder la ventana de
// enumeracion USB del host). rtcbat.cpp queda enlazado pero inerte
// (pmu.begin() falla solo por I2C sin colgar el bus), asi que estos valores
// son placeholders en GPIOs libres, no pines reales.
#define XPOWERS_CHIP_AXP2101
#define PA 21
#define I2S_MCK_IO 38
#define I2S_BCK_IO 39
#define I2S_WS_IO 40
#define I2S_DI_IO 17
#define I2S_DO_IO 18
// OJO: GPIO19/20 son las lineas nativas USB D-/D+ del ESP32-S3 -- si
// SD_MMC.setPins() las reclama, el USB-JTAG-serial deja de enumerar en el
// host aunque la placa siga corriendo. GPIO35-37 tambien evitados: los usa
// el PSRAM octal (PSRAM=opi).
#define SDMMC_CLK 4
#define SDMMC_CMD 8
#define SDMMC_DATA 12

#else  // BOARD_WAVESHARE_AMOLED (placa original, sin cambios)
// ---------------------------------------------------------------------------
// Pines oficiales de la Waveshare ESP32-S3-Touch-AMOLED-1.75
// Fuente: github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75 (libraries/Mylibrary/pin_config.h)
// ---------------------------------------------------------------------------

#define XPOWERS_CHIP_AXP2101

// Pantalla AMOLED 466x466, driver CO5300 por QSPI
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Táctil capacitivo CST9217 por I2C
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40

// Audio ES8311. NOTA: el MCLK real es GPIO42 (verificado en placa con el
// proyecto PlaneRadar2.0); el 16 que figuraba era erroneo. Aun asi el codec se
// configura con reloj derivado del BCLK, asi que el MCLK apenas importa.
#define I2S_MCK_IO 42
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define PA 46

// Ranura TF (no usada todavía)
#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3
#define SDMMC_CS 41

#endif
