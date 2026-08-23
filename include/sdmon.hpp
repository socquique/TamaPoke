#pragma once
#include <Arduino.h>

// Sprite animado TPK1 (formato heredado, camino de respaldo). El proyecto usa
// PMD/TPK2 (PmdMon) para todo; esta ruta queda inactiva si no hay NNN.bin en la SD.
// Los datos indexados viven en PSRAM; la paleta es RGB565.
struct SdMon {
  bool loaded = false;
  uint16_t w = 0, h = 0, frames = 0, frameMs = 100;
  uint8_t scale = 2;       // factor de zoom entero al dibujar
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *data = nullptr;  // frames * w * h indices (0xFF = transparente)

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
};

// acciones de los sprites PMD (formato TPK2)
enum : uint8_t {
  PMD_IDLE = 0, PMD_WALKL, PMD_WALKR, PMD_SLEEP, PMD_EAT, PMD_HURT,
  PMD_ATTACK, PMD_POSE, PMD_HOP, PMD_NOD, PMD_BREATH, PMD_SIT,
  PMD_NACTS
};

struct PmdAct {
  uint8_t w = 0, h = 0, frames = 0;
  uint8_t base = 0;  // fila+1 del pixel mas bajo (anclar por los pies, no el lienzo)
  uint16_t ms[24];
  const uint8_t *data = nullptr;  // frames * w * h en el blob
};

// sprite PMD multi-accion cargado de la SD a PSRAM
struct PmdMon {
  bool loaded = false;
  uint16_t palCount = 0;
  uint16_t pal[256];
  uint8_t *blob = nullptr;
  PmdAct acts[PMD_NACTS];

  bool load(uint8_t dexNum, bool shiny = false);
  void unload();
  bool has(uint8_t a) const { return loaded && a < PMD_NACTS && acts[a].frames > 0; }
};

// miniaturas de la galeria (thumbs.bin entero en PSRAM)
struct SdThumbs {
  bool loaded = false;
  uint8_t *data = nullptr;
  uint16_t count = 0;
  bool load();
  const uint8_t *get(int16_t dex) const;  // blob: w,h,palCount,pal[],idx[]
};
extern SdThumbs thumbs;

bool sdBegin();                 // monta la SD (SDMMC 1-bit), true si hay tarjeta
bool sdSerialCommand(const String &line);  // PUT/LS por USB; true si la maneja
extern bool sdReady;
extern bool sdDirty;  // true tras recibir archivos: recargar sprite
