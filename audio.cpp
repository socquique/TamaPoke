#include "audio.h"
#include "pin_config.h"
#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <Preferences.h>

// ---------------------------------------------------------------------------
// Audio del TamaPoke: códec ES8311 (DAC -> amplificador PA -> altavoz) por I2S.
// Init del ES8311 portado del driver oficial de Espressif (esp-bsp), fijado a
// MCLK=4.096MHz (256*fs), 16kHz, 16-bit, esclavo I2S. Los efectos son tonos
// cuadrados (estilo Game Boy) sintetizados en una tarea aparte para no
// bloquear el loop de juego.
// ---------------------------------------------------------------------------

#define ES8311_ADDR 0x18
#define SAMPLE_RATE 16000

static I2SClass i2s;
static bool gReady = false;
static uint8_t gMode = SOUND_FULL;
static QueueHandle_t gQ = nullptr;

// ---- I2C del códec ----
static bool esW(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static uint8_t esR(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ES8311_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

// Secuencia de init VERIFICADA EN ESTA PLACA (proyecto PlaneRadar2.0, misma
// Waveshare 1.75). Clave: reloj DERIVADO DEL BCLK (reg01=0xBF, sin MCLK externo)
// y referencia interna que alimenta el DAC (reg44=0x58); sin esos dos el codec
// respondia por I2C pero no salia audio. 16kHz, 16-bit, esclavo I2S.
static bool es8311Init() {
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) return false;

  // open()
  esW(0x0D, 0xFA); esW(0x44, 0x08); esW(0x44, 0x08);  // power up + quirk de 1a escritura
  esW(0x01, 0x30); esW(0x02, 0x00); esW(0x03, 0x10); esW(0x16, 0x24);
  esW(0x04, 0x10); esW(0x05, 0x00); esW(0x0B, 0x00); esW(0x0C, 0x00);
  esW(0x10, 0x1F); esW(0x11, 0x7F);
  esW(0x00, 0x80); esW(0x00, 0x80);                   // reset clock, esclavo
  esW(0x01, 0xBF);                                    // clk src = BCLK (sin MCLK externo)
  { uint8_t r = esR(0x06); r &= ~0x20; esW(0x06, r); }  // SCLK no invertido
  esW(0x13, 0x10); esW(0x1B, 0x0A); esW(0x1C, 0x6A);
  esW(0x44, 0x58);                                    // referencia interna -> alimenta el DAC

  // config_sample(): BCLK*8 = DIG_MCLK
  esW(0x02, 0x18); esW(0x05, 0x00); esW(0x03, 0x10); esW(0x04, 0x20);
  { uint8_t r = esR(0x07); r &= 0xC0; esW(0x07, r); }
  esW(0x08, 0xFF);
  { uint8_t r = esR(0x06); r &= 0xE0; r |= 0x03; esW(0x06, r); }  // bclk_div=4

  // formato I2S 16-bit
  esW(0x09, 0x0C); esW(0x0A, 0x0C);

  // start() DAC esclavo
  esW(0x00, 0x80); esW(0x01, 0xBF); esW(0x09, 0x0C); esW(0x0A, 0x0C);
  esW(0x17, 0xBF); esW(0x0E, 0x02); esW(0x12, 0x00); esW(0x14, 0x1A);
  esW(0x0D, 0x01); esW(0x15, 0x40); esW(0x37, 0x08); esW(0x45, 0x00);

  // volumen + unmute
  esW(0x32, 0xBF);                                    // volumen DAC ~0 dB
  { uint8_t r = esR(0x31); r &= 0x9F; esW(0x31, r); }  // unmute
  return true;
}

// ---- sintetizador pequeño: onda, volumen, slide y ruido ----
enum Wave : uint8_t { W_SQUARE = 0, W_TRI, W_SOFT, W_NOISE };
struct Note {
  uint16_t f, ms;
  int16_t slide;
  uint8_t vol;
  uint8_t wave;
};

#define SQ(f, ms, vol)       {f, ms, 0, vol, W_SQUARE}
#define TRI(f, ms, vol)      {f, ms, 0, vol, W_TRI}
#define SOFT(f, ms, vol)     {f, ms, 0, vol, W_SOFT}
#define NS(ms, vol)          {0, ms, 0, vol, W_NOISE}
#define SL(f, ms, to, vol, w) {f, ms, (int16_t)((to) - (f)), vol, w}
#define SIL(ms)              {0, ms, 0, 0, W_SQUARE}

static const Note N_TAP[]    = {SQ(1175, 38, 78)};
static const Note N_EAT[]    = {SOFT(523, 42, 64), SIL(12), SOFT(659, 50, 70)};
static const Note N_PLAY[]   = {SL(760, 55, 1020, 84, W_TRI), SQ(1319, 45, 76)};
static const Note N_HEART[]  = {SOFT(1047, 70, 56), SIL(18), SOFT(1319, 105, 68)};
static const Note N_HATCH[]  = {TRI(523, 70, 60), TRI(659, 70, 64), TRI(784, 95, 68), SL(880, 190, 1320, 72, W_TRI)};
static const Note N_EVOLVE[] = {SL(392, 100, 560, 58, W_TRI), SL(523, 100, 740, 62, W_TRI), SL(659, 110, 960, 66, W_TRI), SL(880, 210, 1480, 76, W_SOFT)};
static const Note N_MEDAL[]  = {TRI(784, 60, 66), SIL(22), TRI(1047, 68, 72), SIL(20), SL(1175, 210, 1568, 78, W_TRI)};
static const Note N_DENY[]   = {SL(330, 120, 230, 70, W_SQUARE), SL(220, 150, 160, 64, W_SQUARE)};
static const Note N_BYE[]    = {SOFT(784, 130, 58), SOFT(659, 140, 55), SL(523, 260, 392, 54, W_SOFT)};
static const Note N_LEVEL[]  = {TRI(784, 65, 64), TRI(1047, 80, 70), SOFT(1319, 130, 70)};
static const Note N_BATTLE_WIN[]   = {TRI(659, 58, 66), TRI(784, 58, 68), TRI(988, 80, 72), SL(1175, 170, 1568, 76, W_TRI)};
static const Note N_BATTLE_LOSS[]  = {SL(392, 140, 330, 66, W_SOFT), SL(330, 140, 247, 62, W_SOFT), SOFT(196, 220, 56)};
static const Note N_CATCH_OK[]     = {TRI(784, 55, 68), TRI(988, 65, 72), SL(1175, 180, 1568, 78, W_TRI)};
static const Note N_CATCH_FAIL[]   = {NS(55, 50), SL(523, 80, 392, 64, W_SQUARE), SIL(16), SL(392, 180, 247, 62, W_SOFT)};
static const Note N_DAILY_GOAL[]   = {TRI(1175, 50, 68), SIL(22), TRI(1568, 70, 74), SOFT(1760, 95, 68)};
static const Note N_EVENT_SPARKLE[] = {NS(35, 36), TRI(1568, 42, 56), TRI(1976, 62, 60), SIL(18), TRI(1760, 56, 54)};
static const Note N_REST[]         = {SL(523, 125, 392, 48, W_SOFT), SOFT(330, 170, 42)};
static const Note N_COUNTER[]      = {SL(784, 75, 1175, 62, W_TRI), SIL(16), SQ(1568, 70, 74), NS(40, 42)};
static const Note N_MENU[]         = {TRI(988, 42, 74), SQ(1319, 50, 80)};
static const Note N_GAME_START[]   = {TRI(659, 58, 72), TRI(880, 64, 78), SQ(1175, 74, 82)};
static const Note N_BALL_BOUNCE[]  = {SL(820, 42, 520, 72, W_SQUARE)};
static const Note N_BALL_MISS[]    = {NS(55, 56), SL(360, 110, 210, 68, W_SOFT)};
static const Note N_MEMO_STEP[]    = {SQ(1047, 54, 68)};

struct SfxDef { const Note *n; uint8_t len; };
static const SfxDef SFX[SFX_COUNT] = {
  {N_TAP, 1}, {N_EAT, 3}, {N_PLAY, 2}, {N_HEART, 2}, {N_HATCH, 4},
  {N_EVOLVE, 4}, {N_MEDAL, 5}, {N_DENY, 2}, {N_BYE, 3}, {N_LEVEL, 3},
  {N_BATTLE_WIN, 4}, {N_BATTLE_LOSS, 3}, {N_CATCH_OK, 3}, {N_CATCH_FAIL, 4},
  {N_DAILY_GOAL, 4}, {N_EVENT_SPARKLE, 5}, {N_REST, 2}, {N_COUNTER, 4},
  {N_MENU, 2}, {N_GAME_START, 3}, {N_BALL_BOUNCE, 1}, {N_BALL_MISS, 2}, {N_MEMO_STEP, 1},
};

static const uint8_t SFX_MIN_MODE[SFX_COUNT] = {
  SOUND_FULL, // TAP
  SOUND_MED,  // EAT
  SOUND_FULL, // PLAY
  SOUND_MED,  // HEART
  SOUND_LOW,  // HATCH
  SOUND_LOW,  // EVOLVE
  SOUND_LOW,  // MEDAL
  SOUND_MED,  // DENY
  SOUND_LOW,  // BYE
  SOUND_LOW,  // LEVEL
  SOUND_LOW,  // BATTLE_WIN
  SOUND_LOW,  // BATTLE_LOSS
  SOUND_LOW,  // CATCH_OK
  SOUND_LOW,  // CATCH_FAIL
  SOUND_LOW,  // DAILY_GOAL
  SOUND_MED,  // EVENT_SPARKLE
  SOUND_MED,  // REST
  SOUND_MED,  // COUNTER
  SOUND_FULL, // MENU
  SOUND_MED,  // GAME_START
  SOUND_FULL, // BALL_BOUNCE
  SOUND_FULL, // BALL_MISS
  SOUND_FULL, // MEMO_STEP
};

static int16_t buf[256 * 2];  // estéreo intercalado (L=R)

static uint16_t noiseState = 0xACE1;
static int16_t nextNoise() {
  noiseState = (uint16_t)((noiseState >> 1) ^ (-(noiseState & 1u) & 0xB400u));
  return (noiseState & 1) ? 1 : -1;
}

static int16_t oscSample(uint8_t wave, int phase, int period, int16_t amp) {
  if (wave == W_NOISE) return (int16_t)(nextNoise() * amp);
  if (period <= 1) return 0;
  int p = phase % period;
  if (wave == W_TRI) {
    int half = period / 2;
    int v = (p < half) ? (-amp + (2 * amp * p) / half) : (amp - (2 * amp * (p - half)) / (period - half));
    return (int16_t)v;
  }
  if (wave == W_SOFT) {
    int half = period / 2;
    int q = (p < half) ? p : period - p;
    int v = (2 * amp * q) / half - amp;
    return (int16_t)(v * 3 / 4);
  }
  return (p < period / 2) ? amp : -amp;
}

// reproduce una nota con rampa anti-click; f==0 es silencio salvo W_NOISE.
static void playTone(const Note &note) {
  uint16_t f = note.f;
  uint16_t ms = note.ms;
  int total = SAMPLE_RATE * ms / 1000;
  int done = 0;
  int phase = 0;
  const int16_t maxAmp = (int16_t)(7600L * note.vol / 100);
  while (done < total) {
    int n = total - done; if (n > 256) n = 256;
    for (int i = 0; i < n; i++) {
      int16_t s = 0;
      int idx = done + i;
      if (note.wave == W_NOISE || f) {
        uint16_t curF = f;
        if (note.slide && total > 1) {
          int32_t v = (int32_t)note.f + ((int32_t)note.slide * idx) / total;
          curF = (v < 20) ? 20 : (uint16_t)v;
        }
        int period = curF ? (SAMPLE_RATE / curF) : 0;
        if (period < 2) period = 2;
        int16_t amp = maxAmp;
        if (idx < 64) s = (int16_t)(s * idx / 64);                 // ataque
        s = oscSample(note.wave, phase, period, amp);
        if (idx < 64) s = (int16_t)(s * idx / 64);
        else if (idx > total - 96) s = (int16_t)(s * (total - idx) / 96);
        phase++;
      }
      buf[i * 2] = s; buf[i * 2 + 1] = s;
    }
    i2s.write((uint8_t *)buf, n * 4);
    done += n;
  }
}

static void audioTask(void *) {
  uint8_t id;
  for (;;) {
    if (xQueueReceive(gQ, &id, portMAX_DELAY) && gReady && id < SFX_COUNT && gMode >= SFX_MIN_MODE[id]) {
      digitalWrite(PA, HIGH);  // enciende el amplificador
      delay(8);                // deja que arranque
      const SfxDef &d = SFX[id];
      for (uint8_t i = 0; i < d.len; i++) playTone(d.n[i]);
      delay(60);               // deja salir la cola del DMA antes de cortar
      digitalWrite(PA, LOW);   // apaga el amp entre sonidos (evita siseo)
    }
  }
}

void audioBegin() {
  // I2S primero: arranca el MCLK que necesita el códec para engancharse
  pinMode(PA, OUTPUT);
  digitalWrite(PA, LOW);   // amp apagado; la tarea lo enciende al reproducir

  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("I2S begin fallo");
    return;
  }
  if (!es8311Init()) { Serial.println("ES8311 no responde (audio off)"); return; }

  Preferences p;
  p.begin("tamapoke", true);
  if (p.isKey("sndm")) {
    gMode = p.getUChar("sndm", SOUND_FULL);
  } else {
    gMode = p.getBool("snd", true) ? SOUND_FULL : SOUND_OFF;
  }
  p.end();
  if (gMode > SOUND_FULL) gMode = SOUND_FULL;

  gReady = true;
  gQ = xQueueCreate(16, sizeof(uint8_t));
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 1, nullptr, 0);
  sfxPlay(SFX_HATCH);  // jingle de arranque (confirma que suena)
}

void sfxPlay(uint8_t id) {
  if (gReady && gMode != SOUND_OFF && gQ && id < SFX_COUNT && gMode >= SFX_MIN_MODE[id])
    xQueueSend(gQ, &id, 0);  // descarta si la cola esta llena
}

void audioSetEnabled(bool on) {
  audioSetMode(on ? SOUND_FULL : SOUND_OFF);
}

bool audioEnabled() { return gMode != SOUND_OFF; }

void audioSetMode(uint8_t mode) {
  if (mode > SOUND_FULL) mode = SOUND_FULL;
  gMode = mode;
  Preferences p;
  p.begin("tamapoke", false);
  p.putUChar("sndm", gMode);
  p.putBool("snd", gMode != SOUND_OFF);
  p.end();
}

uint8_t audioMode() { return gMode; }
