#pragma once

// Pines oficiales de la Waveshare ESP32-S3-Touch-AMOLED-1.75
// Fuente: github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75 (libraries/Mylibrary/pin_config.h)

#define XPOWERS_CHIP_AXP2101

// Pantalla AMOLED 466x466, driver CO5300 por QSPI
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#if BOARD_AMOLED_175C
#define LCD_RESET 1
#else
#define LCD_RESET 39
#endif
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Frecuencia de gfx->begin(): QSPI a 80MHz (el flush del framebuffer es el
// cuello de botella del fps; a 40MHz ~56ms por frame).
#define DISPLAY_BEGIN_HZ 80000000

// Táctil capacitivo CST9217 por I2C
#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#if BOARD_AMOLED_175C
#define TP_RESET 2
#else
#define TP_RESET 40
#endif
#define TOUCH_ADDR 0x5A
#define TOUCH_POLLING 0  // tiene linea INT: lecturas a puerta por ISR

// Audio ES8311. NOTA: el MCLK real es GPIO42 (verificado en placa con el
// proyecto PlaneRadar2.0); el 16 que figuraba era erroneo. Aun asi el codec se
// configura con reloj derivado del BCLK, asi que el MCLK apenas importa.
#if BOARD_AMOLED_175C
#define I2S_MCK_IO 16
#else
#define I2S_MCK_IO 42
#endif
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define PA 46

#if BOARD_AMOLED_175C
// 1.75C (experimental, sin microSD ni PCF85063): los sprites van en una particion
// LittleFS en flash y el reloj es por software. Boton PWR: SYS_OUT -> GPIO3 (alto al
// pulsar). BOOT / Key1 es GPIO0 (strapping, activo en bajo).
#define BOOT_BUTTON_PIN 0
#define PWR_SENSE_PIN 3
#define PWR_SENSE_PRESSED HIGH
#define BOARD_NAME "ESP32-S3-Touch-AMOLED-1.75C"
#else
// Ranura TF (SDMMC 1-bit)
#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3
#define SDMMC_CS 41
#define BOARD_NAME "ESP32-S3-Touch-AMOLED-1.75"
#endif

// Capacidades de la placa (plan.md seccion 5, BoardCaps en tiempo de
// compilacion). El firmware degrada lo que valga 0.
#define HAS_PMU 1
#if BOARD_AMOLED_175C
#define HAS_RTC 0  // reloj por software (ver rtcbat.cpp)
#else
#define HAS_RTC 1
#endif
#define HAS_AUDIO 1
#define HAS_SD 1  // en la 1.75C es LittleFS en flash (ver sdmon.cpp)
