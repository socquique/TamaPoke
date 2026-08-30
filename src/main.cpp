#include <Arduino.h>
#include <M5Cardputer.h>
#include <M5GFX.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>
#include <ctype.h>
#include <cstring>
#include <cmath>

#include "pet.h"
#include "dex.h"
#include "audio.h"
#include "pmd_stream.h"
#include "user_config.h"

static constexpr int SD_SCK  = 40;
static constexpr int SD_MISO = 39;
static constexpr int SD_MOSI = 14;
static constexpr int SD_CS   = 12;

static constexpr uint16_t UI_INK       = 0x18C4;
static constexpr uint16_t UI_INK_NIGHT = 0xC638;
static constexpr uint16_t UI_CREAM     = 0xFF5A;
static constexpr uint16_t UI_WHITE     = 0xFFFF;
static constexpr uint16_t UI_TRACK     = 0xBDF7;
static constexpr uint16_t UI_OK        = 0x4D49;
static constexpr uint16_t UI_WARN      = 0xFDC0;
static constexpr uint16_t UI_BAD       = 0xE9C6;
static constexpr uint16_t UI_BLUE      = 0x4C98;
static constexpr uint16_t UI_PINK      = 0xECF3;

static Pet pet;
static PmdStream mon;
static PmdStream evoOld;
static M5Canvas ui(&M5Cardputer.Display);

static bool sdReady = false;
static int16_t monDex = -999;
static bool monShiny = false;

enum Screen : uint8_t {
  HOME,
  CARD,
  DEX_GRID,
  DEX_DETAIL,
  SETTINGS,
  HELP,
  PLAY,
  TRAIN,
  RENAME,
  DIALOG
};

enum DialogKind : uint8_t {
  DLG_NONE = 0,
  DLG_EVOLVE,
  DLG_FAREWELL,
  DLG_RELEASE
};

static Screen screen = HOME;
static uint8_t starterSel = 0;
static uint8_t homeSel = 0;
static bool feedOpen = false;
static uint8_t feedSel = 0;
static uint8_t cardPage = 0;
static uint8_t settingsSel = 0;
static int16_t dexCursor = 1;
static bool dexGridDirty = true;

static DialogKind dialogKind = DLG_NONE;
static uint8_t dialogSel = 0;

static char renameBuf[12] = "";
static uint8_t renameLen = 0;

static String toast;
static uint32_t toastUntil = 0;

static uint8_t ambientAction = PMD_IDLE;
static uint32_t ambientUntil = 0;
static int16_t petX = 120;
static int16_t petTargetX = 120;
static uint32_t lastMotion = 0;
static uint8_t transientAction = PMD_IDLE;
static uint32_t transientUntil = 0;

static bool bathActive = false;
static bool bathPending = false;
static uint32_t bathUntil = 0;
struct BathBubble { int16_t x, y; uint8_t r, phase; };
static BathBubble bubbles[12];

// Play: keep the Pokeball in the air. Keyboard Enter/Space replaces touch.
static bool playActive = false;
static bool playResult = false;
static uint32_t playResultUntil = 0;
static uint32_t lastPlayStep = 0;
static float ballX = 120, ballY = 35, ballVX = 1.7f, ballVY = 0;
static float playPetX = 120;
static uint16_t playScore = 0;
static uint8_t playMisses = 0;
static uint32_t hitTime = 0;
static float hitX = 0, hitY = 0;
static bool playNewHi = false;

// Strength training: original-style punching bag.
static bool trainActive = false;
static bool trainResult = false;
static uint32_t trainUntil = 0;
static uint32_t trainResultUntil = 0;
static uint16_t trainHits = 0;
static uint8_t trainGain = 0;
static float sackShake = 0;
static bool trainNewHi = false;

static uint32_t lastDraw = 0;
static bool dirty = true;

static const int16_t STARTERS[3] = {1, 4, 7};

static uint16_t C565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)((((uint16_t)r >> 3) << 11) |
                    (((uint16_t)g >> 2) << 5) |
                    ((uint16_t)b >> 3));
}

static uint16_t lerp565(uint16_t a, uint16_t b, int i, int n) {
  if (n <= 0) return a;
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)(((ar + (br - ar) * i / n) << 11) |
                    ((ag + (bg - ag) * i / n) << 5) |
                    (ab + (bb - ab) * i / n));
}

static int sceneHour() {
  struct tm ti;
  if (getLocalTime(&ti, 2)) return ti.tm_hour;
  return 13;
}

static bool sceneNight() {
  int h = sceneHour();
  return pet.sleeping || h < 6 || h >= 20;
}

static uint16_t inkColor() {
  return sceneNight() ? UI_INK_NIGHT : UI_INK;
}

static const char* currentName() {
  if (pet.isEgg()) return "EGG";
  if (pet.nick[0]) return pet.nick;
  if (pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) return dexName(pet.speciesId);
  return "?";
}

static void say(const String &s, uint32_t ms = 1800) {
  toast = s;
  toastUntil = millis() + ms;
  dirty = true;
}

static void drawHeart(int cx, int cy, uint16_t col, int s = 1) {
  ui.fillCircle(cx - 3 * s, cy, 4 * s, col);
  ui.fillCircle(cx + 3 * s, cy, 4 * s, col);
  ui.fillTriangle(cx - 7 * s, cy + 1 * s,
                  cx + 7 * s, cy + 1 * s,
                  cx, cy + 9 * s, col);
}

static void drawCloud(int cx, int cy, uint16_t col) {
  ui.fillCircle(cx, cy, 7, col);
  ui.fillCircle(cx + 9, cy + 2, 6, col);
  ui.fillCircle(cx - 7, cy + 2, 5, col);
  ui.fillRect(cx - 8, cy + 2, 18, 6, col);
}

static const uint16_t BIOME_SOIL[6] = {
  0x7E0F, // meadow
  0xDE52, // beach
  0x4C4A, // forest
  0x8AA8, // volcano
  0xA48D, // mountain
  0xE73C  // snow
};

static void drawScene(uint8_t biome, uint32_t now, bool night, int bottomY = 90) {
  if (biome > 5) biome = 0;
  int h = sceneHour();
  uint16_t top, bot;

  if (night) {
    top = C565(0x0c, 0x12, 0x24);
    bot = C565(0x1e, 0x26, 0x46);
  } else if (h < 8) {
    top = C565(0xd1, 0x6a, 0x86);
    bot = C565(0xf3, 0xb8, 0x7c);
  } else if (h < 18) {
    top = C565(0x8f, 0xc8, 0xea);
    bot = C565(0xdc, 0xee, 0xe6);
  } else {
    top = C565(0xc7, 0x5a, 0x4a);
    bot = C565(0xf0, 0xae, 0x64);
  }

  const int horizon = (bottomY * 64) / 100;
  for (int y = 0; y < horizon; y += 4) {
    ui.fillRect(0, y, 240, 4, lerp565(top, bot, y, horizon));
  }

  if (night) {
    ui.fillCircle(208, 18, 10, C565(0xe8, 0xee, 0xf5));
    ui.fillCircle(212, 15, 9, lerp565(top, bot, 18, horizon));
    const int stars[][2] = {{18,17},{40,31},{70,13},{153,20},{181,37},{226,31}};
    for (auto &st : stars) ui.fillRect(st[0], st[1], 2, 2, UI_WHITE);
  } else if (h < 18) {
    ui.fillCircle(206, 22, 11, h < 8 ? C565(0xff,0xd9,0x8a) : C565(0xff,0xe7,0x9f));
    int drift = (now / 90) % 300;
    drawCloud((int)drift - 30, 30, UI_WHITE);
    drawCloud((int)((drift + 145) % 300) - 30, 42, UI_WHITE);
  } else {
    ui.fillCircle(120, horizon - 3, 14, C565(0xff,0xf1,0xc8));
  }

  uint16_t soil = BIOME_SOIL[biome];
  if (night) soil = lerp565(soil, C565(0x16,0x1c,0x30), 9, 16);

  if (biome == 1) {
    uint16_t sea = night ? C565(0x1c,0x34,0x52) : C565(0x4f,0x96,0xc4);
    ui.fillRect(0, horizon - 10, 240, 10, sea);
    for (int i = 0; i < 3; ++i) {
      int wx = 18 + ((now / 80 + i * 71) % 190);
      ui.drawFastHLine(wx, horizon - 8 + i * 3, 18,
                       night ? C565(0x3a,0x58,0x78) : C565(0xbf,0xe6,0xf5));
    }
  }

  ui.fillRect(0, horizon, 240, bottomY - horizon, soil);
  uint16_t hill = lerp565(soil, night ? top : UI_WHITE, 3, 16);
  ui.fillRoundRect(-20, horizon - 5, 280, 23, 12, hill);

  uint16_t dk = lerp565(soil, C565(0x10,0x18,0x20), night ? 11 : 7, 16);
  if (biome == 2) {
    for (int tx : {24, 54, 190, 220}) {
      ui.fillTriangle(tx, horizon - 22, tx - 8, horizon, tx + 8, horizon, dk);
      ui.fillTriangle(tx, horizon - 30, tx - 6, horizon - 13, tx + 6, horizon - 13, dk);
    }
  } else if (biome == 3) {
    ui.fillTriangle(37, horizon, 18, horizon + 14, 56, horizon + 14, dk);
    ui.fillTriangle(205, horizon, 185, horizon + 15, 225, horizon + 15, dk);
    if (!night) {
      for (int i = 0; i < 4; ++i)
        ui.fillRect(68 + i * 38, horizon + 6 + (i & 1) * 4, 2, 2, C565(0xff,0x9b,0x3a));
    }
  } else if (biome == 4) {
    ui.fillTriangle(72, horizon, 35, horizon, 82, horizon - 28, dk);
    ui.fillTriangle(177, horizon, 135, horizon, 178, horizon - 22, dk);
    if (!night) {
      ui.fillTriangle(72, horizon - 20, 82, horizon - 28, 90, horizon - 19, UI_WHITE);
      ui.fillTriangle(168, horizon - 17, 178, horizon - 22, 187, horizon - 16, UI_WHITE);
    }
  } else if (biome == 5 && !night) {
    for (int i = 0; i < 9; ++i) {
      int fx = (i * 37 + now / 45) % 238;
      int fy = (i * 23 + now / 25) % std::max(1, horizon);
      ui.fillRect(fx, fy, 2, 2, UI_WHITE);
    }
  } else if (biome == 0) {
    for (int gx : {24, 65, 176, 217}) {
      ui.drawLine(gx, horizon + 8, gx - 2, horizon + 2, dk);
      ui.drawLine(gx, horizon + 8, gx + 2, horizon + 1, dk);
    }
  }
}

static void ensureSprite(int16_t dex, bool shiny) {
  if (!sdReady || dex < 1 || dex > 151) {
    mon.unload();
    monDex = dex;
    monShiny = shiny;
    return;
  }
  if (dex == monDex && shiny == monShiny && mon.loaded()) return;
  monDex = dex;
  monShiny = shiny;
  mon.load(dex, shiny);
  ambientAction = PMD_IDLE;
  ambientUntil = 0;
  petX = petTargetX = 120;
  lastMotion = millis();
}

static void triggerAction(uint8_t action, uint32_t ms) {
  transientAction = action;
  transientUntil = millis() + ms;
  dirty = true;
}

static uint8_t chooseExisting(const uint8_t *choices, int n) {
  if (!mon.loaded()) return PMD_IDLE;
  uint8_t available[PMD_NACTS];
  int count = 0;
  for (int i = 0; i < n; ++i) {
    if (choices[i] < PMD_NACTS && mon.has(choices[i])) available[count++] = choices[i];
  }
  return count ? available[random(count)] : PMD_IDLE;
}

static void updateAmbient(uint32_t now) {
  if (!mon.loaded() || pet.isEgg() || pet.sleeping || pet.evolving() || pet.ceremony) return;

  uint32_t dt = now - lastMotion;
  lastMotion = now;

  if (petX < petTargetX) {
    int step = std::max(1, (int)(dt / 28));
    petX = std::min<int16_t>(petTargetX, petX + step);
  } else if (petX > petTargetX) {
    int step = std::max(1, (int)(dt / 28));
    petX = std::max<int16_t>(petTargetX, petX - step);
  }

  if (now < ambientUntil) return;

  int r = random(100);
  if (r < 38 && (mon.has(PMD_WALKL) || mon.has(PMD_WALKR))) {
    petTargetX = random(82, 159);
    ambientAction = (petTargetX >= petX) ? PMD_WALKR : PMD_WALKL;
    ambientUntil = now + 1800 + random(1700);
  } else if (r < 67) {
    static const uint8_t flair[] = {PMD_POSE, PMD_NOD, PMD_BREATH};
    ambientAction = chooseExisting(flair, sizeof(flair));
    ambientUntil = now + 1200 + random(1200);
  } else {
    ambientAction = PMD_IDLE;
    ambientUntil = now + 1800 + random(2600);
  }
}

static uint8_t currentAction(uint32_t now) {
  if (pet.sleeping) return PMD_SLEEP;
  if (pet.eating()) return PMD_EAT;
  if (now < transientUntil) return transientAction;
  if (pet.showHeart()) return mon.has(PMD_NOD) ? PMD_NOD : PMD_POSE;
  if (pet.lowestStat() <= 10 && mon.has(PMD_HURT)) return PMD_HURT;
  return ambientAction;
}

static const char* statusMsg() {
  if (pet.sleeping) return "Zzz...";
  if (pet.fullness <= 20) return "I'm hungry...";
  if (pet.hygiene <= 20) return "Bath time...";
  if (pet.energy <= 20) return "So sleepy...";
  if (pet.joy <= 20) return "Play with me!";
  if (pet.showHeart()) return "<3";
  return "Happy";
}

static void drawHeaderText() {
  const bool night = sceneNight();
  uint16_t accent = (!pet.isEgg() && pet.speciesId > 0) ? DEX_TBL[pet.speciesId].accent : UI_INK;
  uint16_t bg = night ? C565(0x16, 0x20, 0x35) : C565(0xff, 0xf7, 0xdf);
  uint16_t border = night ? C565(0x62, 0x76, 0x9a) : C565(0xc7, 0xb9, 0x94);

  // Solid header plate: no shadow, no text drawn twice, and no moving sky
  // directly behind the glyphs. This keeps the tiny ST7789 text crisp.
  ui.fillRoundRect(27, 1, 186, 30, 7, bg);
  ui.drawRoundRect(27, 1, 186, 30, 7, border);

  char name[36];
  if (pet.isEgg()) snprintf(name, sizeof(name), "EGG");
  else snprintf(name, sizeof(name), "%s%s  Lv.%u",
                pet.shiny ? "*" : "", currentName(), pet.level());

  const int len = strlen(name);
  ui.setTextSize(len <= 14 ? 2 : 1);
  ui.setTextColor(night ? UI_INK_NIGHT : accent);
  ui.drawCentreString(name, 120, len <= 14 ? 3 : 6, 1);

  ui.setTextSize(1);
  ui.setTextColor(night ? C565(0xc8, 0xd5, 0xeb) : UI_INK);
  const char *msg = pet.isEgg() ? "ENTER TO HATCH" : statusMsg();
  ui.drawCentreString(msg, 120, 21, 1);

  if (pet.streak) {
    int x = 6, y = 5;
    ui.fillTriangle(x + 5, y, x, y + 10, x + 10, y + 10, UI_BAD);
    ui.fillTriangle(x + 5, y + 4, x + 2, y + 10, x + 8, y + 10, UI_WARN);
    ui.setTextSize(1);
    ui.setTextColor(night ? UI_INK_NIGHT : UI_INK);
    char s[8];
    snprintf(s, sizeof(s), "%u", pet.streak);
    ui.drawString(s, x + 13, y + 2);
  }
}

static void drawEgg(int cx, int groundY) {
  int cy = groundY - 24;
  ui.fillEllipse(cx, cy, 18, 25, UI_CREAM);
  ui.drawEllipse(cx, cy, 18, 25, UI_INK);
  ui.fillTriangle(cx - 12, cy - 1, cx - 4, cy - 8, cx + 4, cy, C565(0x7f,0xc4,0xb4));
  ui.fillTriangle(cx + 4, cy + 4, cx + 12, cy - 5, cx + 15, cy + 7, C565(0xf0,0x9b,0xa8));

  uint8_t cracks = pet.eggCracks();
  if (cracks >= 1) ui.drawLine(cx, cy - 8, cx - 6, cy - 2, UI_INK);
  if (cracks >= 2) {
    ui.drawLine(cx - 6, cy - 2, cx + 1, cy + 4, UI_INK);
    ui.drawLine(cx + 1, cy + 4, cx + 8, cy - 2, UI_INK);
  }
}

static void drawPoops() {
  for (int i = 0; i < pet.poops && i < 3; ++i) {
    int x = 12 + i * 15, y = 78;
    ui.fillCircle(x, y + 6, 5, C565(0x78,0x4a,0x2a));
    ui.fillCircle(x, y + 2, 4, C565(0x8e,0x58,0x31));
    ui.fillCircle(x, y - 1, 2, C565(0x9c,0x64,0x38));
  }
}

static void drawNeedBar(int x, int y, const char *label, uint8_t value) {
  uint16_t fill = value >= 55 ? UI_OK : (value >= 25 ? UI_WARN : UI_BAD);
  ui.setTextSize(1);
  ui.setTextColor(sceneNight() ? UI_INK_NIGHT : UI_INK);
  ui.drawString(label, x, y);
  ui.fillRoundRect(x + 29, y + 2, 25, 5, 2, UI_TRACK);
  int fw = 23 * value / 100;
  if (fw > 0) ui.fillRoundRect(x + 30, y + 3, fw, 3, 1, fill);
}

static void drawBerryIcon(M5Canvas &g, int cx, int cy, uint16_t berry = 0xF800) {
  g.fillCircle(cx - 2, cy + 1, 4, berry);
  g.fillCircle(cx + 3, cy + 1, 4, berry);
  g.fillTriangle(cx, cy - 3, cx + 4, cy - 7, cx + 7, cy - 2, UI_OK);
}

static void drawBallIcon(M5Canvas &g, int cx, int cy) {
  g.fillCircle(cx, cy, 7, UI_WHITE);
  g.fillRect(cx - 6, cy - 6, 13, 6, UI_BAD);
  g.drawFastHLine(cx - 7, cy, 15, UI_INK);
  g.fillCircle(cx, cy, 2, UI_WHITE);
  g.drawCircle(cx, cy, 2, UI_INK);
  g.drawCircle(cx, cy, 7, UI_INK);
}

static void drawMoonIcon(M5Canvas &g, int cx, int cy, uint16_t bg) {
  g.fillCircle(cx, cy, 7, UI_WARN);
  g.fillCircle(cx + 4, cy - 3, 7, bg);
}

static void drawBathIcon(M5Canvas &g, int cx, int cy) {
  g.drawCircle(cx - 4, cy + 2, 4, UI_BLUE);
  g.drawCircle(cx + 4, cy - 1, 5, UI_BLUE);
  g.drawCircle(cx + 7, cy + 6, 3, UI_BLUE);
}

static void drawHomePanel() {
  bool night = sceneNight();
  uint16_t panel = night ? C565(0x18,0x20,0x34) : UI_CREAM;
  ui.fillRect(0, 90, 240, 45, panel);
  ui.drawFastHLine(0, 90, 240, night ? C565(0x4b,0x58,0x73) : UI_TRACK);

  drawNeedBar(5,   94, "FOOD", pet.fullness);
  drawNeedBar(123, 94, "JOY",  pet.joy);
  drawNeedBar(5,  104, "ENE",  pet.energy);
  drawNeedBar(123,104, "HYG",  pet.hygiene);

  const int xs[4] = {2, 62, 122, 182};
  const char *labs[4] = {"FEED", "PLAY", "LIGHT", "BATH"};
  for (int i = 0; i < 4; ++i) {
    bool disabled = pet.sleeping && i != 2;
    uint16_t box = night ? C565(0x20,0x2b,0x42) : UI_WHITE;
    uint16_t border = (i == homeSel) ? UI_WARN : (night ? UI_INK_NIGHT : UI_INK);
    if (disabled) border = UI_TRACK;

    ui.fillRoundRect(xs[i], 114, 56, 19, 5, box);
    ui.drawRoundRect(xs[i], 114, 56, 19, 5, border);
    if (i == homeSel) ui.drawRoundRect(xs[i] + 1, 115, 54, 17, 4, border);

    int cx = xs[i] + 12, cy = 123;
    if (i == 0) drawBerryIcon(ui, cx, cy);
    else if (i == 1) drawBallIcon(ui, cx, cy);
    else if (i == 2) drawMoonIcon(ui, cx, cy, box);
    else drawBathIcon(ui, cx, cy);

    ui.setTextSize(1);
    ui.setTextColor(disabled ? UI_TRACK : (night ? UI_INK_NIGHT : UI_INK));
    ui.drawString(labs[i], xs[i] + 23, 120);
  }
}

static void drawToast() {
  if (!toast.length() || millis() >= toastUntil) return;
  int w = ui.textWidth(toast) + 12;
  if (w > 226) w = 226;
  int x = (240 - w) / 2;
  ui.fillRoundRect(x, 66, w, 17, 5, UI_WHITE);
  ui.drawRoundRect(x, 66, w, 17, 5, UI_INK);
  ui.setTextSize(1);
  ui.setTextColor(UI_INK, UI_WHITE);
  ui.drawCentreString(toast, 120, 71, 1);
}

static void startBath() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathActive) {
    sfxPlay(SFX_DENY);
    return;
  }
  bathActive = true;
  bathPending = true;
  bathUntil = millis() + 3000;
  for (auto &b : bubbles) {
    b.x = 75 + random(92);
    b.y = 38 + random(48);
    b.r = 3 + random(7);
    b.phase = random(64);
  }
  triggerAction(mon.has(PMD_HOP) ? PMD_HOP : PMD_POSE, 2600);
  sfxPlay(SFX_TAP);
}

static void updateBath(uint32_t now) {
  if (!bathActive) return;
  if (now < bathUntil) return;
  bathActive = false;
  if (bathPending) {
    bathPending = false;
    pet.clean();
    say("All clean!");
    triggerAction(mon.has(PMD_POSE) ? PMD_POSE : PMD_IDLE, 1400);
  }
  dirty = true;
}

static void drawBathFx(uint32_t now) {
  if (!bathActive) return;
  uint32_t left = bathUntil > now ? bathUntil - now : 0;
  if (left > 750) {
    for (auto &b : bubbles) {
      int bx = b.x + (int)(sinf(now / 220.0f + b.phase) * 3);
      int by = b.y - (int)((3000 - left) / 120);
      ui.fillCircle(bx, by, b.r, UI_WHITE);
      ui.drawCircle(bx, by, b.r, UI_BLUE);
      if (b.r > 4) ui.fillCircle(bx - b.r / 3, by - b.r / 3, 1, C565(0xc8,0xee,0xff));
    }
  } else {
    for (int i = 0; i < 8; ++i) {
      int sx = bubbles[i].x, sy = bubbles[i].y;
      ui.drawFastHLine(sx - 4, sy, 9, (i & 1) ? UI_WARN : UI_WHITE);
      ui.drawFastVLine(sx, sy - 4, 9, (i & 1) ? UI_WARN : UI_WHITE);
    }
  }
}

static void drawFeedOverlay() {
  if (!feedOpen) return;

  ui.fillRoundRect(11, 56, 218, 31, 8, UI_WHITE);
  ui.drawRoundRect(11, 56, 218, 31, 8, UI_INK);

  const uint16_t cols[4] = {0xF800, 0x001F, 0x07E0, UI_PINK};
  const char *labels[4] = {"RED", "BLUE", "GREEN", "CANDY"};

  for (int i = 0; i < 4; ++i) {
    int x = 15 + i * 53;
    if (i == feedSel) {
      ui.fillRoundRect(x, 59, 49, 25, 6, C565(0xff,0xeb,0xb8));
      ui.drawRoundRect(x, 59, 49, 25, 6, UI_WARN);
    }

    if (i < 3) drawBerryIcon(ui, x + 10, 69, cols[i]);
    else {
      ui.fillRoundRect(x + 5, 64, 10, 10, 3, cols[i]);
      ui.drawLine(x + 5, 64, x + 1, 61, UI_WARN);
      ui.drawLine(x + 15, 64, x + 19, 61, UI_WARN);
    }

    ui.setTextSize(1);
    ui.setTextColor(UI_INK);
    ui.drawString(labels[i], x + 21, 66);
  }
}

static void openDialog(DialogKind k) {
  dialogKind = k;
  dialogSel = 0;
  screen = DIALOG;
  dirty = true;
}

static void drawEvolution(uint32_t now, uint8_t biome) {
  drawScene(biome, now, false, 135);
  int cx = 120, cy = 72;
  float t = pet.evolveT();

  int halo = 18 + (int)(t * 70) + (int)(4 * sinf(now * 0.02f));
  for (int k = 0; k < 3; ++k) {
    int r = halo - k * 5;
    if (r > 0) ui.drawCircle(cx, cy, r, UI_WHITE);
  }

  float base = now * 0.004f;
  for (int i = 0; i < 10; ++i) {
    float a = base + i * (float)(M_PI / 5.0);
    int len = 38 + (i % 3) * 8;
    ui.drawLine(cx, cy, cx + (int)(cosf(a) * len), cy + (int)(sinf(a) * len), UI_WHITE);
  }

  int period = 70 + (int)(180 * (1.0f - t));
  bool showOld = t < 0.9f && evoOld.loaded() && ((now / std::max(60, period)) & 1);
  if (showOld) evoOld.draw(ui, PMD_IDLE, 120, 103, 0, 0, true, UI_INK);
  else {
    ensureSprite(pet.speciesId, pet.shiny);
    if (mon.loaded()) mon.draw(ui, PMD_IDLE, 120, 103, 0, 0, true, UI_INK);
  }

  ui.setTextSize(2);
  ui.setTextColor(UI_WHITE);
  ui.drawCentreString("EVOLUTION!", 120, 10, 1);

  if (t > 0.91f) {
    int r = (int)(150 * (t - 0.91f) / 0.09f);
    ui.fillCircle(cx, cy, r, UI_WHITE);
  }
}

static void drawCeremony(uint32_t now, uint8_t biome) {
  bool runaway = pet.ceremony == CER_RUNAWAY;
  drawScene(biome, now, runaway || sceneNight(), 135);

  float t = pet.ceremonyT();
  int x = 120;
  uint8_t act = PMD_IDLE;

  ensureSprite(pet.speciesId, pet.shiny);

  if (runaway) {
    for (int i = 0; i < 28; ++i) {
      int rx = (i * 37 + now / 3) % 244;
      int ry = (i * 53 + now / 2) % 135;
      ui.drawLine(rx, ry, rx - 2, ry + 7, C565(0x6a,0x84,0xb0));
    }
    if (t < 0.3f) {
      act = mon.has(PMD_HURT) ? PMD_HURT : PMD_IDLE;
      x += (int)(2 * sinf(now * 0.04f));
    } else {
      act = mon.has(PMD_WALKL) ? PMD_WALKL : PMD_IDLE;
      x = 120 - (int)(((t - 0.3f) / 0.7f) * 160);
    }
    if (mon.loaded()) mon.draw(ui, act, x, 113, now);
    ui.setTextColor(UI_INK_NIGHT);
    ui.setTextSize(1);
    ui.drawCentreString("feels abandoned...", 120, 13, 1);
  } else {
    for (int k = 0; k < 3; ++k) {
      int r = 24 + k * 13 + (int)(4 * sinf(now * 0.02f));
      ui.drawCircle(120, 66, r, UI_WARN);
    }
    for (int i = 0; i < 10; ++i) {
      int py = 112 - (int)((now / 12 + i * 22) % 105);
      int px = 14 + (i * 43) % 212;
      if ((i % 3) == 0) drawHeart(px, py, UI_PINK);
      else ui.fillRect(px, py, 2, 2, UI_WARN);
    }
    if (t < 0.45f) act = mon.has(PMD_POSE) ? PMD_POSE : (mon.has(PMD_NOD) ? PMD_NOD : PMD_IDLE);
    else {
      act = mon.has(PMD_WALKR) ? PMD_WALKR : PMD_IDLE;
      x = 120 + (int)(((t - 0.45f) / 0.55f) * 165);
    }
    if (mon.loaded()) mon.draw(ui, act, x, 113, now);
    ui.setTextColor(UI_INK);
    ui.setTextSize(1);
    ui.drawCentreString("Goodbye, my friend...", 120, 13, 1);
  }
}

static void drawHome(uint32_t now) {
  uint8_t biome = 0;
  if (!pet.isEgg() && pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT)
    biome = DEX_TBL[pet.speciesId].biome;

  if (pet.evolving()) {
    drawEvolution(now, biome);
    return;
  } else if (evoOld.loaded()) {
    evoOld.unload();
  }

  if (pet.ceremony) {
    drawCeremony(now, biome);
    return;
  }

  drawScene(biome, now, sceneNight(), 90);

  if (pet.isEgg()) {
    drawEgg(120, 87);
    if (pet.eggRarity() >= R_RARO) {
      ui.setTextSize(1);
      ui.setTextColor(pet.eggRarity() == R_LEGENDARIO ? UI_WARN : UI_BLUE);
      ui.drawCentreString(pet.eggRarity() == R_LEGENDARIO ? "LEGENDARY EGG" : "RARE EGG",
                          120, 74, 1);
    }
  } else {
    ensureSprite(pet.speciesId, pet.shiny);
    updateAmbient(now);

    if (mon.loaded()) {
      mon.draw(ui, currentAction(now), petX, 88, now, -1);
    } else {
      ui.fillRoundRect(69, 43, 102, 30, 6, UI_WHITE);
      ui.drawRoundRect(69, 43, 102, 30, 6, UI_INK);
      ui.setTextSize(1);
      ui.setTextColor(UI_INK);
      ui.drawCentreString("SPRITES MISSING", 120, 50, 1);
      ui.drawCentreString("/mons/pNNN.bin", 120, 61, 1);
    }

    drawPoops();

    if (pet.showHeart()) drawHeart(petX + 24, 34, UI_PINK);

    if (pet.sleeping) {
      ui.setTextSize(2);
      ui.setTextColor(UI_INK_NIGHT);
      ui.drawString("Zz", 193, 33);
    }

    if (pet.wantEvolveButton()) {
      int pulse = (int)(2 * sinf(now * 0.008f));
      ui.fillRoundRect(72 - pulse, 68 - pulse, 96 + pulse * 2, 16 + pulse * 2, 5, UI_BAD);
      ui.drawRoundRect(72 - pulse, 68 - pulse, 96 + pulse * 2, 16 + pulse * 2, 5, UI_WHITE);
      ui.setTextSize(1);
      ui.setTextColor(UI_WHITE);
      ui.drawCentreString("E  EVOLVE", 120, 73, 1);
    } else if (pet.canRunawayNow()) {
      ui.fillRoundRect(56, 68, 128, 16, 5, C565(0x3a,0x44,0x5a));
      ui.setTextColor(UI_INK_NIGHT);
      ui.setTextSize(1);
      ui.drawCentreString("G  FEELS ABANDONED...", 120, 73, 1);
    } else if (pet.wantFarewellButton()) {
      ui.fillRoundRect(56, 68, 128, 16, 5, UI_WARN);
      ui.setTextColor(UI_INK);
      ui.setTextSize(1);
      ui.drawCentreString("G  WANTS TO SAY GOODBYE", 120, 73, 1);
    }
  }

  drawBathFx(now);
  drawHeaderText();
  drawHomePanel();
  drawFeedOverlay();
  drawToast();
}

static void drawStarter() {
  ui.fillScreen(UI_CREAM);
  ui.setTextColor(UI_INK);
  ui.setTextSize(2);
  ui.drawCentreString("CHOOSE YOUR STARTER", 120, 7, 1);
  ui.setTextSize(1);
  ui.setTextColor(C565(0x6d,0x6b,0x68));
  ui.drawCentreString("ARROWS + ENTER", 120, 25, 1);

  for (int i = 0; i < 3; ++i) {
    int y = 38 + i * 30;
    int16_t d = STARTERS[i];
    uint16_t tint = lerp565(DEX_TBL[d].accent, UI_WHITE, 5, 8);
    ui.fillRoundRect(20, y, 200, 25, 7, tint);
    ui.drawRoundRect(20, y, 200, 25, 7, i == starterSel ? UI_WARN : DEX_TBL[d].accent);
    if (i == starterSel) ui.drawRoundRect(22, y + 2, 196, 21, 5, UI_WARN);

    if (sdReady) drawPmdPreview(ui, d, false, 25, y + 2, 38, 21, false);

    ui.setTextSize(1);
    ui.setTextColor(UI_INK);
    char line[32];
    snprintf(line, sizeof(line), "#%03d  %s", d, dexName(d));
    ui.drawString(line, 72, y + 9);
  }
}

static void cardBase(const char *title) {
  ui.fillScreen(UI_CREAM);
  ui.drawRoundRect(3, 3, 234, 129, 10, UI_INK);
  ui.setTextColor(UI_INK);
  ui.setTextSize(2);
  ui.drawCentreString(title, 120, 8, 1);
}

static void drawCardBar(int x, int y, const char *label, uint16_t val, uint16_t maxVal, uint16_t col) {
  ui.setTextSize(1);
  ui.setTextColor(UI_INK);
  ui.drawString(label, x, y);
  ui.fillRoundRect(x + 45, y + 2, 100, 7, 2, UI_TRACK);
  int fw = (maxVal ? (int)((uint32_t)val * 98 / maxVal) : 0);
  if (fw > 98) fw = 98;
  if (fw > 0) ui.fillRoundRect(x + 46, y + 3, fw, 5, 2, col);
  char n[10];
  snprintf(n, sizeof(n), "%u", val);
  ui.drawString(n, x + 150, y);
}

static void drawPageDots(uint8_t page, uint8_t count) {
  int start = 120 - ((count - 1) * 12) / 2;
  for (int i = 0; i < count; ++i) {
    if (i == page) ui.fillCircle(start + i * 12, 124, 3, UI_INK);
    else ui.drawCircle(start + i * 12, 124, 2, UI_INK);
  }
}

static void drawCard(uint32_t now) {
  if (pet.isEgg()) {
    cardBase("PET CARD");
    ui.setTextSize(1);
    ui.drawCentreString("Hatch the egg first.", 120, 62, 1);
    return;
  }

  const DexEntry &d = DEX_TBL[pet.speciesId];
  ensureSprite(pet.speciesId, pet.shiny);

  if (cardPage == 0) {
    cardBase("PROFILE");
    char head[34];
    snprintf(head, sizeof(head), "%s%s  Lv.%u", pet.shiny ? "*" : "", currentName(), pet.level());
    ui.setTextColor(d.accent);
    ui.setTextSize(strlen(head) <= 15 ? 2 : 1);
    ui.drawCentreString(head, 120, 28, 1);

    if (mon.loaded()) mon.draw(ui, PMD_IDLE, 60, 93, now, 0);

    ui.setTextColor(UI_INK);
    ui.setTextSize(1);
    char line[42];
    snprintf(line, sizeof(line), "#%03d %s", pet.speciesId, dexName(pet.speciesId));
    ui.drawString(line, 111, 51);
    snprintf(line, sizeof(line), "Age: %lu day%s",
             (unsigned long)(pet.ageMinutes / 1440),
             pet.ageMinutes >= 2880 ? "s" : "");
    ui.drawString(line, 111, 64);
    snprintf(line, sizeof(line), "Streak: %u  Best: %u", pet.streak, pet.bestStreak);
    ui.drawString(line, 111, 77);
    snprintf(line, sizeof(line), "Bond: %u/100", pet.bond);
    ui.drawString(line, 111, 90);

    const char *berry = !pet.berryKnown ? "Favorite berry: ?"
                        : pet.lovesBerry(0) ? "Favorite: RED"
                        : pet.lovesBerry(1) ? "Favorite: BLUE"
                                           : "Favorite: GREEN";
    ui.drawString(berry, 111, 103);
    ui.setTextColor(C565(0x6d,0x6b,0x68));
    ui.drawString("N rename   R release", 48, 113);
  } else if (cardPage == 1) {
    cardBase("BATTLE");
    drawCardBar(18, 37, "ATK", pet.atkStat(), 260, UI_BAD);
    drawCardBar(18, 56, "DEF", pet.defStat(), 260, UI_BLUE);
    drawCardBar(18, 75, "SPE", pet.speStat(), 260, UI_WARN);
    drawCardBar(18, 94, "WGT", pet.weight, 100, C565(0xb3,0x79,0x55));
    ui.fillRoundRect(55, 109, 130, 12, 4, UI_BAD);
    ui.setTextColor(UI_WHITE);
    ui.setTextSize(1);
    ui.drawCentreString("ENTER: TRAIN STRENGTH", 120, 112, 1);
  } else if (cardPage == 2) {
    cardBase("MEDALS");
    static const char *labs[8] = {"LV10","LV25","LV50","BERRY","7 DAY","BOND","FINAL","FIT"};
    for (int i = 0; i < 8; ++i) {
      int x = 17 + (i % 2) * 108;
      int y = 34 + (i / 2) * 20;
      bool got = pet.hasMedal(1 << i);
      ui.fillRoundRect(x, y, 98, 16, 5, got ? UI_OK : UI_TRACK);
      ui.setTextSize(1);
      ui.setTextColor(got ? UI_WHITE : C565(0x78,0x78,0x78));
      ui.drawCentreString(labs[i], x + 49, y + 5, 1);
    }
    char m[32];
    snprintf(m, sizeof(m), "Total earned: %u", pet.totalMedals);
    ui.setTextColor(UI_INK);
    ui.drawCentreString(m, 120, 115, 1);
  } else {
    cardBase("PROGRESS");
    char lv[20];
    snprintf(lv, sizeof(lv), "LEVEL %u", pet.level());
    ui.setTextColor(d.accent);
    ui.setTextSize(3);
    ui.drawCentreString(lv, 120, 31, 1);

    uint8_t into = pet.ageMinutes % MINUTES_PER_LEVEL;
    ui.fillRoundRect(34, 62, 172, 12, 4, UI_TRACK);
    int fw = 168 * into / MINUTES_PER_LEVEL;
    if (fw) ui.fillRoundRect(36, 64, fw, 8, 3, UI_OK);

    ui.setTextColor(UI_INK);
    ui.setTextSize(1);
    char next[36];
    snprintf(next, sizeof(next), "Next level in %u min", MINUTES_PER_LEVEL - into);
    ui.drawCentreString(next, 120, 79, 1);

    const char *evo = "Final form";
    char evoBuf[40];
    uint16_t evoCol = UI_INK;
    if (d.evolvesTo) {
      int needed = d.evolveLevel + pet.careMistakes;
      if (pet.level() >= needed) {
        if (pet.lowestStat() >= 40) {
          evo = "Evolution ready!";
          evoCol = UI_OK;
        } else {
          evo = "Needs must be >= 40";
          evoCol = UI_BAD;
        }
      } else {
        snprintf(evoBuf, sizeof(evoBuf), "Evolution in %d level%s",
                 needed - pet.level(), (needed - pet.level()) == 1 ? "" : "s");
        evo = evoBuf;
      }
    }
    ui.setTextColor(evoCol);
    ui.drawCentreString(evo, 120, 96, 1);

    char mistakes[32];
    snprintf(mistakes, sizeof(mistakes), "Care mistakes: %u", pet.careMistakes);
    ui.setTextColor(pet.careMistakes ? UI_BAD : UI_INK);
    ui.drawCentreString(mistakes, 120, 109, 1);
  }

  drawPageDots(cardPage, 4);
}

static void drawDexGrid() {
  ui.fillScreen(UI_CREAM);
  char head[30];
  snprintf(head, sizeof(head), "POKEDEX %u/151", pet.registeredCount());
  ui.setTextSize(2);
  ui.setTextColor(UI_INK);
  ui.drawCentreString(head, 120, 3, 1);

  int page = (dexCursor - 1) / 16;
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      int dex = page * 16 + r * 4 + c + 1;
      if (dex > 151) continue;
      int x = 3 + c * 59;
      int y = 20 + r * 23;
      bool sel = dex == dexCursor;
      bool known = pet.isRegistered(dex);

      ui.fillRoundRect(x, y, 57, 21, 4, sel ? C565(0xff,0xeb,0xb8) : UI_WHITE);
      ui.drawRoundRect(x, y, 57, 21, 4, sel ? UI_WARN : UI_TRACK);

      if (sdReady) {
        drawPmdPreview(ui, dex, pet.isShinyRegistered(dex),
                       x + 2, y + 1, 53, 18, !known, UI_INK);
      } else {
        ui.setTextSize(1);
        ui.setTextColor(known ? DEX_TBL[dex].accent : UI_TRACK);
        ui.drawCentreString(known ? "MON" : "?", x + 28, y + 7, 1);
      }

      if (pet.isShinyRegistered(dex)) {
        ui.setTextSize(1);
        ui.setTextColor(UI_WARN);
        ui.drawString("*", x + 48, y + 2);
      }
    }
  }

  ui.fillRect(0, 113, 240, 22, UI_CREAM);
  char selLine[38];
  bool known = pet.isRegistered(dexCursor);
  snprintf(selLine, sizeof(selLine), "#%03d  %s", dexCursor, known ? dexName(dexCursor) : "???");
  ui.setTextSize(1);
  ui.setTextColor(known ? DEX_TBL[dexCursor].accent : UI_INK);
  ui.drawCentreString(selLine, 120, 115, 1);

  int start = 57;
  for (int i = 0; i < 10; ++i) {
    if (i == page) ui.fillCircle(start + i * 14, 130, 3, UI_INK);
    else ui.drawCircle(start + i * 14, 130, 2, UI_INK);
  }
}

static void drawDexDetail(uint32_t now) {
  ui.fillScreen(UI_CREAM);
  bool known = pet.isRegistered(dexCursor);
  const DexEntry &d = DEX_TBL[dexCursor];

  char head[40];
  snprintf(head, sizeof(head), "#%03d  %s%s",
           dexCursor, pet.isShinyRegistered(dexCursor) ? "*" : "",
           known ? dexName(dexCursor) : "???");
  ui.setTextColor(known ? d.accent : UI_INK);
  ui.setTextSize(strlen(head) <= 17 ? 2 : 1);
  ui.drawCentreString(head, 120, 6, 1);

  ensureSprite(dexCursor, pet.isShinyRegistered(dexCursor));
  if (mon.loaded()) mon.draw(ui, PMD_IDLE, 120, 91, known ? now : 0, 0, !known, UI_INK);

  ui.setTextSize(1);
  ui.setTextColor(UI_INK);
  char stats[48];
  snprintf(stats, sizeof(stats), "ATK %u   DEF %u   SPE %u", d.bAtk, d.bDef, d.bSpe);
  ui.drawCentreString(stats, 120, 98, 1);

  static const char *biomes[6] = {"MEADOW","BEACH","FOREST","VOLCANO","MOUNTAIN","SNOW"};
  char info[48];
  snprintf(info, sizeof(info), "Habitat: %s", biomes[d.biome < 6 ? d.biome : 0]);
  ui.drawCentreString(info, 120, 111, 1);

  ui.setTextColor(C565(0x6d,0x6b,0x68));
  ui.drawCentreString("LEFT/RIGHT browse   ESC grid", 120, 125, 1);
}

static void drawSettings() {
  ui.fillScreen(UI_CREAM);
  ui.setTextColor(UI_INK);
  ui.setTextSize(2);
  ui.drawCentreString("SETTINGS", 120, 7, 1);

  const char *items[5] = {
    audioEnabled() ? "SOUND: ON" : "SOUND: OFF",
    "POKEDEX",
    "CONTROLS",
    "RELEASE POKEMON",
    "BACK"
  };

  for (int i = 0; i < 5; ++i) {
    int y = 30 + i * 19;
    bool sel = i == settingsSel;
    ui.fillRoundRect(38, y, 164, 15, 5, sel ? C565(0xff,0xeb,0xb8) : UI_WHITE);
    ui.drawRoundRect(38, y, 164, 15, 5, sel ? UI_WARN : UI_TRACK);
    ui.setTextSize(1);
    ui.setTextColor(i == 3 ? UI_BAD : UI_INK);
    ui.drawCentreString(items[i], 120, y + 5, 1);
  }

  ui.setTextColor(C565(0x6d,0x6b,0x68));
  ui.setTextSize(1);
  const bool ntp = strlen(TAMAPOKE_WIFI_SSID) > 0;
  ui.drawCentreString(ntp ? "Clock: NTP at boot" : "Clock: runtime only", 120, 126, 1);
}

static void drawHelp() {
  ui.fillScreen(UI_CREAM);
  ui.setTextColor(UI_INK);
  ui.setTextSize(2);
  ui.drawCentreString("CONTROLS", 120, 5, 1);
  ui.setTextSize(1);
  const char *lines[] = {
    "HOME: LEFT/RIGHT select action",
    "ENTER: selected action",
    "SPACE: pet / hit / bounce",
    "UP: pet card    DOWN: settings",
    "D: Pokedex      E: evolution",
    "G: farewell/runaway",
    "N: rename       R: release",
    "ESC/BACKSPACE: back",
    "Arrow keycaps also work without Fn"
  };
  for (int i = 0; i < 9; ++i) ui.drawString(lines[i], 15, 27 + i * 11);
}

static void drawRename() {
  ui.fillScreen(UI_CREAM);
  ui.setTextColor(UI_INK);
  ui.setTextSize(2);
  ui.drawCentreString("RENAME", 120, 10, 1);

  ui.fillRoundRect(27, 42, 186, 30, 7, UI_WHITE);
  ui.drawRoundRect(27, 42, 186, 30, 7, UI_INK);
  ui.setTextSize(2);
  ui.drawCentreString(renameLen ? renameBuf : "_", 120, 49, 1);

  ui.setTextSize(1);
  ui.setTextColor(C565(0x6d,0x6b,0x68));
  ui.drawCentreString("TYPE NAME  |  ENTER SAVE", 120, 86, 1);
  ui.drawCentreString("BACKSPACE DELETE  |  ESC CANCEL", 120, 102, 1);
  ui.drawCentreString("MAX 11 CHARACTERS", 120, 118, 1);
}

static void drawDialog() {
  ui.fillScreen(UI_CREAM);

  const char *title = "";
  const char *a = "";
  const char *b = "";
  uint16_t actionCol = UI_OK;

  if (dialogKind == DLG_EVOLVE) {
    title = "EVOLVE?";
    a = "EVOLVE";
    b = "KEEP FORM";
    actionCol = UI_BAD;
  } else if (dialogKind == DLG_FAREWELL) {
    title = "SAY GOODBYE?";
    a = "SAY GOODBYE";
    b = "STAY TOGETHER";
    actionCol = UI_WARN;
  } else {
    title = "RELEASE POKEMON?";
    a = "RELEASE";
    b = "CANCEL";
    actionCol = UI_BAD;
  }

  ui.setTextColor(UI_INK);
  ui.setTextSize(2);
  ui.drawCentreString(title, 120, 18, 1);

  for (int i = 0; i < 2; ++i) {
    int y = 53 + i * 38;
    uint16_t bg = (i == 0) ? actionCol : UI_OK;
    if (i == dialogSel) {
      ui.fillRoundRect(42, y, 156, 27, 8, bg);
      ui.drawRoundRect(40, y - 2, 160, 31, 9, UI_INK);
    } else {
      ui.fillRoundRect(42, y, 156, 27, 8, lerp565(bg, UI_WHITE, 3, 8));
      ui.drawRoundRect(42, y, 156, 27, 8, UI_TRACK);
    }
    ui.setTextSize(1);
    ui.setTextColor(i == 0 && dialogKind == DLG_FAREWELL ? UI_INK : UI_WHITE);
    ui.drawCentreString(i == 0 ? a : b, 120, y + 10, 1);
  }
}

static void resetBall() {
  ballX = 85 + random(70);
  ballY = 31;
  ballVX = random(2) ? 1.8f : -1.8f;
  ballVY = 0;
}

static void startPlay() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) {
    sfxPlay(SFX_DENY);
    say(pet.isEgg() ? "Hatch the egg first" : "Wake your Pokemon first");
    return;
  }
  playActive = true;
  playResult = false;
  playScore = 0;
  playMisses = 0;
  playPetX = 120;
  hitTime = 0;
  lastPlayStep = millis();
  resetBall();
  screen = PLAY;
  dirty = true;
}

static void hitBall() {
  if (!playActive || playResult) return;
  ++playScore;
  float lift = 4.8f + std::min<float>(2.7f, playScore * 0.05f);
  ballVY = -lift;
  ballVX += (ballX - 120.0f) * 0.012f;
  if (ballVX > 4.5f) ballVX = 4.5f;
  if (ballVX < -4.5f) ballVX = -4.5f;
  hitX = ballX;
  hitY = ballY;
  hitTime = millis();
  sfxPlay(SFX_PLAY);
}

static void updatePlay(uint32_t now) {
  if (!playActive || playResult) {
    if (playResult && now >= playResultUntil) {
      playActive = false;
      playResult = false;
      screen = HOME;
      dirty = true;
    }
    return;
  }

  if (now - lastPlayStep < 35) return;
  uint32_t steps = (now - lastPlayStep) / 35;
  if (steps > 3) steps = 3;
  lastPlayStep += steps * 35;

  while (steps--) {
    float gravity = 0.21f + std::min<float>(0.18f, playScore * 0.004f);
    ballVY += gravity;
    ballX += ballVX;
    ballY += ballVY;

    if (ballX < 15) {
      ballX = 15;
      ballVX = fabsf(ballVX) * 0.92f;
    } else if (ballX > 225) {
      ballX = 225;
      ballVX = -fabsf(ballVX) * 0.92f;
    }
    if (ballY < 17) {
      ballY = 17;
      ballVY = fabsf(ballVY) * 0.65f;
    }

    if (ballY > 111) {
      ++playMisses;
      if (playMisses >= 3) {
        playNewHi = playScore > pet.gameHi;
        pet.playResult((uint8_t)std::min<uint16_t>(255, playScore));
        sfxPlay(playNewHi && playScore ? SFX_MEDAL : SFX_LEVEL);
        playResult = true;
        playResultUntil = now + 3200;
        dirty = true;
        break;
      }
      resetBall();
    }

    float chase = (ballX - playPetX) * 0.10f;
    if (chase > 4.0f) chase = 4.0f;
    if (chase < -4.0f) chase = -4.0f;
    playPetX += chase;
  }
}

static void drawPlay(uint32_t now) {
  uint8_t biome = pet.speciesId > 0 ? DEX_TBL[pet.speciesId].biome : 0;
  drawScene(biome, now, sceneNight(), 135);

  if (playResult) {
    ui.setTextColor(sceneNight() ? UI_INK_NIGHT : UI_INK);
    ui.setTextSize(3);
    char sc[28];
    snprintf(sc, sizeof(sc), "SCORE %u", playScore);
    ui.drawCentreString(sc, 120, 42, 1);
    ui.setTextSize(1);
    ui.drawCentreString(playNewHi ? "NEW RECORD!" : "Good job!", 120, 78, 1);
    return;
  }

  char score[20];
  snprintf(score, sizeof(score), "%u", playScore);
  ui.setTextColor(sceneNight() ? UI_INK_NIGHT : UI_INK);
  ui.setTextSize(2);
  ui.drawCentreString(score, 120, 3, 1);

  for (int i = 0; i < 3; ++i) {
    if (i < 3 - playMisses) ui.fillCircle(94 + i * 26, 19, 3, UI_BAD);
    else ui.drawCircle(94 + i * 26, 19, 3, UI_TRACK);
  }

  ensureSprite(pet.speciesId, pet.shiny);
  if (mon.loaded()) {
    uint8_t act = ballX > playPetX + 4 ? PMD_WALKR :
                  ballX < playPetX - 4 ? PMD_WALKL : PMD_IDLE;
    if (!mon.has(act)) act = PMD_IDLE;
    mon.draw(ui, act, (int)playPetX, 127, now, 1);
  }

  if (hitTime && now - hitTime < 250) {
    int rad = 8 + (now - hitTime) / 18;
    ui.drawCircle((int)hitX, (int)hitY, rad, UI_WARN);
  }

  drawBallIcon(ui, (int)ballX, (int)ballY);

  ui.setTextSize(1);
  ui.setTextColor(sceneNight() ? UI_INK_NIGHT : UI_INK);
  ui.drawCentreString("SPACE / ENTER = BOUNCE", 120, 126, 1);
}

static void startTrain() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) {
    sfxPlay(SFX_DENY);
    return;
  }
  trainActive = true;
  trainResult = false;
  trainUntil = millis() + 10000;
  trainHits = 0;
  trainGain = 0;
  sackShake = 0;
  screen = TRAIN;
  dirty = true;
}

static void hitSack() {
  if (!trainActive || trainResult || millis() >= trainUntil) return;
  ++trainHits;
  sackShake = 10.0f;
  sfxPlay(SFX_PLAY);
}

static void updateTrain(uint32_t now) {
  if (!trainActive) return;

  sackShake *= 0.84f;

  if (!trainResult && now >= trainUntil) {
    trainNewHi = trainHits > pet.strHi;
    trainGain = pet.trainStrength(trainHits);
    trainResult = true;
    trainResultUntil = now + 3200;
    sfxPlay(trainNewHi ? SFX_MEDAL : SFX_LEVEL);
    dirty = true;
  } else if (trainResult && now >= trainResultUntil) {
    trainActive = false;
    trainResult = false;
    screen = CARD;
    cardPage = 1;
    dirty = true;
  }
}

static void drawTrain(uint32_t now) {
  uint8_t biome = pet.speciesId > 0 ? DEX_TBL[pet.speciesId].biome : 0;
  drawScene(biome, now, sceneNight(), 135);

  if (trainResult) {
    ui.setTextColor(sceneNight() ? UI_INK_NIGHT : UI_INK);
    ui.setTextSize(2);
    char h[24];
    snprintf(h, sizeof(h), "%u HITS", trainHits);
    ui.drawCentreString(h, 120, 43, 1);
    char g[28];
    snprintf(g, sizeof(g), "STRENGTH +%u", trainGain);
    ui.setTextColor(UI_BAD);
    ui.drawCentreString(g, 120, 69, 1);
    ui.setTextSize(1);
    ui.setTextColor(sceneNight() ? UI_INK_NIGHT : UI_INK);
    ui.drawCentreString(trainNewHi ? "NEW RECORD!" : "Training complete", 120, 95, 1);
    return;
  }

  int off = (int)(sackShake * sinf(now * 0.08f));
  int sx = 164 + off;
  uint16_t ink = sceneNight() ? UI_INK_NIGHT : UI_INK;

  ui.fillRect(162, 20, 4, 15, ink);
  ui.fillRect(sx - 2, 30, 4, 10, ink);
  ui.fillRoundRect(sx - 22, 38, 44, 67, 13, C565(0xb5,0x3a,0x3a));
  ui.fillRoundRect(sx - 22, 38, 44, 11, 8, C565(0x7e,0x28,0x28));
  ui.drawRoundRect(sx - 22, 38, 44, 67, 13, ink);

  ensureSprite(pet.speciesId, pet.shiny);
  if (mon.loaded()) mon.draw(ui, PMD_ATTACK, 70, 110, now, 1);

  char hits[12];
  snprintf(hits, sizeof(hits), "%u", trainHits);
  ui.setTextColor(ink);
  ui.setTextSize(3);
  ui.drawCentreString(hits, 120, 5, 1);

  uint32_t left = trainUntil > now ? trainUntil - now : 0;
  ui.fillRoundRect(39, 116, 162, 8, 3, UI_TRACK);
  int fw = (int)((uint32_t)158 * left / 10000);
  if (fw > 0) ui.fillRoundRect(41, 118, fw, 4, 2, UI_OK);

  ui.setTextSize(1);
  ui.drawCentreString("SPACE / ENTER = HIT", 120, 126, 1);
}

static void chooseFeed() {
  if (feedSel < 3) pet.feedBerry(feedSel);
  else pet.feedCandy();
  feedOpen = false;
  sfxPlay(SFX_EAT);
  triggerAction(PMD_EAT, 2500);
  say(feedSel == 3 ? "Candy!" : "Yum!");
}

static void activateHomeAction() {
  if (pet.isEgg()) {
    pet.eggTap();
    sfxPlay(SFX_TAP);
    dirty = true;
    return;
  }

  if (pet.sleeping && homeSel != 2) {
    sfxPlay(SFX_DENY);
    say("Wake your Pokemon first");
    return;
  }

  if (homeSel == 0) {
    feedOpen = true;
    feedSel = 0;
    sfxPlay(SFX_TAP);
  } else if (homeSel == 1) {
    startPlay();
  } else if (homeSel == 2) {
    pet.toggleLight();
    sfxPlay(SFX_TAP);
    triggerAction(pet.sleeping ? PMD_SLEEP : PMD_HOP, 1300);
  } else {
    startBath();
  }
  dirty = true;
}

static void acceptDialog() {
  if (dialogSel == 1) {
    if (dialogKind == DLG_EVOLVE) pet.declineEvolve();
    else if (dialogKind == DLG_FAREWELL) pet.declineFarewell();
    dialogKind = DLG_NONE;
    screen = HOME;
    dirty = true;
    return;
  }

  if (dialogKind == DLG_EVOLVE) {
    int16_t old = pet.speciesId;
    bool oldShiny = pet.shiny;
    pet.evolve();
    if (sdReady && old > 0) evoOld.load(old, oldShiny);
    monDex = -999;
    sfxPlay(SFX_EVOLVE);
  } else if (dialogKind == DLG_FAREWELL) {
    pet.startFarewell();
    sfxPlay(SFX_BYE);
  } else if (dialogKind == DLG_RELEASE) {
    pet.release();
    sfxPlay(SFX_BYE);
  }

  dialogKind = DLG_NONE;
  screen = HOME;
  dirty = true;
}

static void openRename() {
  if (pet.isEgg()) return;
  strncpy(renameBuf, pet.nick, sizeof(renameBuf) - 1);
  renameBuf[sizeof(renameBuf) - 1] = 0;
  renameLen = strlen(renameBuf);
  screen = RENAME;
  dirty = true;
}

static void handleRenameInput(const bool chars[128], const bool prevChars[128],
                              bool backEdge, bool enterEdge, bool escEdge) {
  if (escEdge) {
    screen = CARD;
    dirty = true;
    return;
  }
  if (backEdge) {
    if (renameLen) renameBuf[--renameLen] = 0;
    dirty = true;
  }
  if (enterEdge) {
    pet.rename(renameBuf);
    screen = CARD;
    say("Name saved");
    dirty = true;
    return;
  }
  for (int i = 0; i < 128; ++i) {
    if (!chars[i] || prevChars[i]) continue;
    char c = (char)i;
    if (isalnum((unsigned char)c) || c == '-' || c == '.') {
      if (renameLen < sizeof(renameBuf) - 1) {
        renameBuf[renameLen++] = (char)toupper((unsigned char)c);
        renameBuf[renameLen] = 0;
        dirty = true;
      }
    }
  }
}

static void onKeyboard() {
  Keyboard_Class::KeysState st = M5Cardputer.Keyboard.keysState();

  static bool prevChars[128] = {false};
  static bool prevEnter = false, prevSpace = false;
  static bool prevBackspace = false, prevEsc = false;
  static bool prevUp = false, prevDown = false, prevLeft = false, prevRight = false;

  bool chars[128] = {false};
  for (char raw : st.word) {
    unsigned char u = (unsigned char)raw;
    if (u < 128) {
      char c = (char)tolower(u);
      chars[(uint8_t)c] = true;
    }
  }

  // Cardputer arrow keycaps are ; , . / on the physical keyboard.
  // M5Cardputer reports arrows on the Fn layer. Accept both so the printed
  // arrow keycaps work directly, without Fn.
  bool upNow    = st.up    || chars[(uint8_t)';'];
  bool leftNow  = st.left  || chars[(uint8_t)','];
  bool downNow  = st.down  || chars[(uint8_t)'.'];
  bool rightNow = st.right || chars[(uint8_t)'/'];

  bool upEdge = upNow && !prevUp;
  bool downEdge = downNow && !prevDown;
  bool leftEdge = leftNow && !prevLeft;
  bool rightEdge = rightNow && !prevRight;
  bool enterEdge = st.enter && !prevEnter;
  bool spaceEdge = st.space && !prevSpace;
  bool backNow = st.backspace || st.del;
  bool escNow = st.esc || chars[(uint8_t)'`'];
  bool backEdge = backNow && !prevBackspace;
  bool escEdge = escNow && !prevEsc;

  if (screen == RENAME) {
    handleRenameInput(chars, prevChars, backEdge, enterEdge, escEdge);
    goto save_input_state;
  }

  if (pet.awaitingStarter()) {
    if (leftEdge || upEdge) {
      starterSel = starterSel == 0 ? 2 : starterSel - 1;
      dirty = true;
    }
    if (rightEdge || downEdge) {
      starterSel = (starterSel + 1) % 3;
      dirty = true;
    }
    if (enterEdge || spaceEdge) {
      pet.chooseStarter(STARTERS[starterSel]);
      sfxPlay(SFX_TAP);
      say(String(dexName(STARTERS[starterSel])) + " chosen!");
      screen = HOME;
      dirty = true;
    }
    goto printable_keys;
  }

  if (screen == HOME) {
    if (feedOpen) {
      if (leftEdge) {
        feedSel = feedSel == 0 ? 3 : feedSel - 1;
        dirty = true;
      }
      if (rightEdge) {
        feedSel = (feedSel + 1) % 4;
        dirty = true;
      }
      if (enterEdge || spaceEdge) chooseFeed();
      if (escEdge || backEdge) {
        feedOpen = false;
        dirty = true;
      }
    } else {
      if (leftEdge) {
        homeSel = homeSel == 0 ? 3 : homeSel - 1;
        dirty = true;
      }
      if (rightEdge) {
        homeSel = (homeSel + 1) % 4;
        dirty = true;
      }
      if (upEdge && !pet.isEgg()) {
        cardPage = 0;
        screen = CARD;
        dirty = true;
      }
      if (downEdge) {
        settingsSel = 0;
        screen = SETTINGS;
        dirty = true;
      }
      if (enterEdge) activateHomeAction();
      if (spaceEdge) {
        if (pet.isEgg()) {
          pet.eggTap();
          sfxPlay(SFX_TAP);
        } else if (!pet.sleeping) {
          pet.caress();
          sfxPlay(SFX_HEART);
          triggerAction(mon.has(PMD_NOD) ? PMD_NOD : PMD_POSE, 1300);
        }
        dirty = true;
      }
    }
  } else if (screen == CARD) {
    if (leftEdge) {
      cardPage = cardPage == 0 ? 3 : cardPage - 1;
      dirty = true;
    }
    if (rightEdge) {
      cardPage = (cardPage + 1) % 4;
      dirty = true;
    }
    if ((enterEdge || spaceEdge) && cardPage == 1) startTrain();
    if (escEdge || backEdge || downEdge) {
      screen = HOME;
      dirty = true;
    }
  } else if (screen == DEX_GRID) {
    int old = dexCursor;
    if (leftEdge && dexCursor > 1) --dexCursor;
    if (rightEdge && dexCursor < 151) ++dexCursor;
    if (upEdge && dexCursor > 4) dexCursor -= 4;
    if (downEdge && dexCursor <= 147) dexCursor += 4;
    if (dexCursor != old) {
      dexGridDirty = true;
      dirty = true;
    }
    if (enterEdge || spaceEdge) {
      screen = DEX_DETAIL;
      monDex = -999;
      dirty = true;
    }
    if (escEdge || backEdge) {
      screen = HOME;
      dirty = true;
    }
  } else if (screen == DEX_DETAIL) {
    if (leftEdge && dexCursor > 1) {
      --dexCursor;
      monDex = -999;
      dirty = true;
    }
    if (rightEdge && dexCursor < 151) {
      ++dexCursor;
      monDex = -999;
      dirty = true;
    }
    if (escEdge || backEdge || enterEdge || spaceEdge) {
      screen = DEX_GRID;
      dexGridDirty = true;
      dirty = true;
    }
  } else if (screen == SETTINGS) {
    if (upEdge) {
      settingsSel = settingsSel == 0 ? 4 : settingsSel - 1;
      dirty = true;
    }
    if (downEdge) {
      settingsSel = (settingsSel + 1) % 5;
      dirty = true;
    }
    if (enterEdge || spaceEdge) {
      if (settingsSel == 0) {
        audioSetEnabled(!audioEnabled());
        if (audioEnabled()) sfxPlay(SFX_TAP);
        dirty = true;
      } else if (settingsSel == 1) {
        dexCursor = pet.speciesId > 0 ? pet.speciesId : 1;
        screen = DEX_GRID;
        dexGridDirty = true;
        dirty = true;
      } else if (settingsSel == 2) {
        screen = HELP;
        dirty = true;
      } else if (settingsSel == 3) {
        if (!pet.isEgg()) openDialog(DLG_RELEASE);
      } else {
        screen = HOME;
        dirty = true;
      }
    }
    if (escEdge || backEdge) {
      screen = HOME;
      dirty = true;
    }
  } else if (screen == HELP) {
    if (escEdge || backEdge || enterEdge || spaceEdge) {
      screen = SETTINGS;
      dirty = true;
    }
  } else if (screen == PLAY) {
    if ((enterEdge || spaceEdge) && !playResult) hitBall();
    if (escEdge || backEdge) {
      playActive = false;
      playResult = false;
      screen = HOME;
      dirty = true;
    }
  } else if (screen == TRAIN) {
    if ((enterEdge || spaceEdge) && !trainResult) hitSack();
    if (escEdge || backEdge) {
      trainActive = false;
      trainResult = false;
      screen = CARD;
      cardPage = 1;
      dirty = true;
    }
  } else if (screen == DIALOG) {
    if (upEdge || leftEdge || downEdge || rightEdge) {
      dialogSel ^= 1;
      dirty = true;
    }
    if (enterEdge || spaceEdge) acceptDialog();
    if (escEdge || backEdge) {
      dialogKind = DLG_NONE;
      screen = HOME;
      dirty = true;
    }
  }

printable_keys:
  for (int i = 0; i < 128; ++i) {
    if (!chars[i] || prevChars[i]) continue;
    char c = (char)i;
    if (c == '`' || c == ';' || c == ',' || c == '.' || c == '/') continue;

    if (screen == HOME && !feedOpen) {
      if (c == 'f') { homeSel = 0; activateHomeAction(); }
      else if (c == 'p') { homeSel = 1; activateHomeAction(); }
      else if (c == 'l') { homeSel = 2; activateHomeAction(); }
      else if (c == 'b') { homeSel = 3; activateHomeAction(); }
      else if (c == 'd') {
        dexCursor = pet.speciesId > 0 ? pet.speciesId : 1;
        screen = DEX_GRID;
        dexGridDirty = true;
        dirty = true;
      } else if (c == 'i' && !pet.isEgg()) {
        cardPage = 0;
        screen = CARD;
        dirty = true;
      } else if (c == 'e' && pet.wantEvolveButton()) {
        openDialog(DLG_EVOLVE);
      } else if (c == 'g') {
        if (pet.canRunawayNow()) {
          pet.startRunaway();
          sfxPlay(SFX_BYE);
          dirty = true;
        } else if (pet.wantFarewellButton()) {
          openDialog(DLG_FAREWELL);
        } else {
          sfxPlay(SFX_DENY);
        }
      } else if (c == 'r' && !pet.isEgg()) {
        openDialog(DLG_RELEASE);
      } else if (c == 's') {
        audioSetEnabled(!audioEnabled());
        if (audioEnabled()) sfxPlay(SFX_TAP);
        say(audioEnabled() ? "Sound ON" : "Sound OFF");
      }
    } else if (screen == CARD) {
      if (c == 'n' && cardPage == 0) openRename();
      else if (c == 'r') openDialog(DLG_RELEASE);
      else if (c == 'd') {
        dexCursor = pet.speciesId;
        screen = DEX_GRID;
        dexGridDirty = true;
        dirty = true;
      }
    }
  }

save_input_state:
  memcpy(prevChars, chars, sizeof(prevChars));
  prevEnter = st.enter;
  prevSpace = st.space;
  prevBackspace = backNow;
  prevEsc = escNow;
  prevUp = upNow;
  prevDown = downNow;
  prevLeft = leftNow;
  prevRight = rightNow;
}

static uint32_t getNtpEpoch() {
  if (strlen(TAMAPOKE_WIFI_SSID) == 0) return 0;

  ui.fillScreen(BLACK);
  ui.setTextColor(WHITE, BLACK);
  ui.setTextSize(1);
  ui.drawCentreString("Syncing clock...", 120, 61, 1);
  ui.pushSprite(0, 0);

  WiFi.mode(WIFI_STA);
  WiFi.begin(TAMAPOKE_WIFI_SSID, TAMAPOKE_WIFI_PASSWORD);
  uint32_t start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) delay(100);

  uint32_t epoch = 0;
  if (WiFi.status() == WL_CONNECTED) {
    configTzTime(TAMAPOKE_TZ, "pool.ntp.org", "time.nist.gov");
    time_t t = 0;
    start = millis();
    while (t < 1700000000 && millis() - start < 6000) {
      delay(100);
      time(&t);
    }
    if (t >= 1700000000) epoch = (uint32_t)t;
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return epoch;
}

static bool screenAnimated() {
  if (pet.awaitingStarter()) return false;
  if (screen == HOME) return true;
  if (screen == CARD && cardPage == 0) return true;
  if (screen == DEX_DETAIL) return true;
  if (screen == PLAY || screen == TRAIN) return true;
  if (screen == DIALOG) return false;
  return false;
}

static void render(uint32_t now) {
  if (pet.awaitingStarter()) {
    drawStarter();
  } else {
    switch (screen) {
      case HOME:       drawHome(now); break;
      case CARD:       drawCard(now); break;
      case DEX_GRID:
        // The grid is expensive because it streams 16 tiny previews from SD.
        // Only rebuild it after navigation/page changes.
        if (dexGridDirty || dirty) {
          drawDexGrid();
          dexGridDirty = false;
        }
        break;
      case DEX_DETAIL: drawDexDetail(now); break;
      case SETTINGS:   drawSettings(); break;
      case HELP:       drawHelp(); break;
      case PLAY:       drawPlay(now); break;
      case TRAIN:      drawTrain(now); break;
      case RENAME:     drawRename(); break;
      case DIALOG:     drawDialog(); break;
    }
  }

  // One complete RGB565 frame is pushed after all drawing is finished.
  // The LCD never sees a black clear followed by partial drawing.
  ui.pushSprite(0, 0);
}

void setup() {
  Serial.begin(115200);

  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(BLACK);

  // 240x135x16-bit = 64,800 bytes. Cardputer ADV has no PSRAM, so this is the
  // only full-screen buffer; PMD keeps only one animation frame in RAM.
  ui.setColorDepth(16);
  if (!ui.createSprite(240, 135)) {
    Serial.println("FATAL: display canvas allocation failed");
    while (true) delay(1000);
  }
  ui.setTextDatum(top_left);
  ui.setTextSize(1);
  ui.fillScreen(BLACK);
  ui.pushSprite(0, 0);

  randomSeed((uint32_t)micros());
  audioBegin();

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SD.begin(SD_CS, SPI, 25000000);
  Serial.printf("SD: %s\n", sdReady ? "OK" : "not mounted");

  // v0.6: Pet::begin() itself loads the direct microSD save before NVS/new-game logic.
  pet.begin();

  uint32_t epoch = getNtpEpoch();
  if (epoch) {
    pet.syncClock(epoch);
    Serial.printf("Clock synced: %lu\n", (unsigned long)epoch);
  } else {
    Serial.println("Clock: runtime-only (set Wi-Fi in include/user_config.h for NTP)");
  }

  dexCursor = pet.speciesId > 0 ? pet.speciesId : 1;
  dirty = true;
}

void loop() {
  M5Cardputer.update();
  uint32_t now = millis();

  pet.update(now);
  onKeyboard();


  updateBath(now);
  updatePlay(now);
  updateTrain(now);

  static uint32_t lastSaveCheck = 0;
  if (now - lastSaveCheck > 15000) {
    lastSaveCheck = now;
    if (pet.savePending() && screen != PLAY && screen != TRAIN) pet.flushSave();
  }

  static uint32_t lastClockTick = 0;
  if (lastClockTick == 0) lastClockTick = now;
  if (pet.lastSeenEpoch && now - lastClockTick >= 30000) {
    uint32_t steps = (now - lastClockTick) / 30000;
    lastClockTick += steps * 30000;
    pet.lastSeenEpoch += steps * 30;
  }

  bool anim = screenAnimated();
  uint32_t interval = (screen == PLAY || screen == TRAIN) ? 70 : 100;
  if (dirty || (anim && now - lastDraw >= interval)) {
    lastDraw = now;
    bool wasDirty = dirty;
    dirty = false;
    render(now);
    (void)wasDirty;
  }

  delay(4);
}
