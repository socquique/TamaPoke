#pragma once

// Pines para el port a la Elecrow CrowPanel 1.28inch-HMI ESP32 Rotary Display
// (ESP32-S3R8, GC9A01 240x240 redondo por SPI, tactil CST816D por I2C).
// Fuente: github.com/Elecrow-RD/CrowPanel-1.28inch-HMI-ESP32-Rotary-Display-240-240-IPS-Round-Touch-Knob-Screen
// y makerguides.com/getting-started-crowpanel-1-28inch-hmi-esp32-rotary-display
//
// Rama de este port: elecrow-port. pin_config.h de la Waveshare original queda
// en main; este archivo es incompatible con esa placa (resolucion, bus SPI en
// vez de QSPI, sin PMU/audio).

// Pantalla IPS 240x240 redonda, driver GC9A01 por SPI
#define TFT_SCLK 10
#define TFT_MOSI 11
#define TFT_MISO -1
#define TFT_DC    3
#define TFT_CS    9
#define TFT_RES  14
#define TFT_BLK  46          // backlight, PWM (ledcAttach/ledcWrite)
#define LCD_WIDTH 240
#define LCD_HEIGHT 240

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
// tiene ninguno de los tres. audioBegin() esta deshabilitado en TamaPoke.ino
// (sus pines I2S/PA reales chocaban con SCLK/CS/backlight de arriba).
// rtcbat.cpp y sdmon.cpp quedan enlazados pero inertes: pmu.begin(),
// rtc.begin() y SD_MMC.begin() fallan solos por I2C/SDMMC sin colgar el bus,
// asi que estos valores son placeholders en GPIOs libres, no pines reales.
#define XPOWERS_CHIP_AXP2101
#define PA 21
#define I2S_MCK_IO 38
#define I2S_BCK_IO 39
#define I2S_WS_IO 40
#define I2S_DI_IO 17
#define I2S_DO_IO 18
#define SDMMC_CLK 19
#define SDMMC_CMD 20
#define SDMMC_DATA 4
