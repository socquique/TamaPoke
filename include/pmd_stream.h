#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include <FS.h>
#include <SD.h>

// Same TPK2 action IDs used by upstream TamaPoke.
enum PmdAction : uint8_t {
  PMD_IDLE = 0,
  PMD_WALKL,
  PMD_WALKR,
  PMD_SLEEP,
  PMD_EAT,
  PMD_HURT,
  PMD_ATTACK,
  PMD_POSE,
  PMD_HOP,
  PMD_NOD,
  PMD_BREATH,
  PMD_SIT,
  PMD_NACTS
};

class PmdStream {
public:
  PmdStream();
  ~PmdStream();

  bool load(uint16_t dex, bool shiny = false);
  void unload();

  bool loaded() const { return _loaded; }
  bool has(uint8_t action) const;

  // Streams one action while keeping only one PMD frame in internal RAM.
  // Missing actions fall back to IDLE.
  void draw(M5Canvas &target, uint8_t action, int16_t cx, int16_t groundY,
            uint32_t nowMs, int8_t forcedScale = 0,
            bool silhouette = false, uint16_t silhouetteColor = 0x18C4);

private:
  struct ActionMeta {
    bool present = false;
    uint8_t w = 0;
    uint8_t h = 0;
    uint8_t frames = 0;
    uint8_t base = 0;
    uint16_t dur[24] = {0};
    uint32_t dataOffset = 0;
  };

  File _file;
  bool _loaded = false;
  uint16_t _palCount = 0;
  uint16_t _pal[256] = {0};
  ActionMeta _act[PMD_NACTS];

  uint8_t *_frameBuf = nullptr;
  uint32_t _frameCapacity = 0;
  int8_t _cachedAction = -1;
  int8_t _cachedFrame = -1;

  int frameAt(uint8_t action, uint32_t nowMs) const;
  bool readFrame(uint8_t action, int frame);
  bool computeBase(uint8_t action);
  uint8_t resolvedAction(uint8_t requested) const;
};

// Lightweight Pokédex/starter preview. Opens the TPK2 file only long enough to
// read the palette and first IDLE frame; it does not load the whole animation.
bool drawPmdPreview(M5Canvas &target, uint16_t dex, bool shiny,
                    int16_t boxX, int16_t boxY, int16_t boxW, int16_t boxH,
                    bool silhouette = false, uint16_t silhouetteColor = 0x18C4);
