// TamaPoke - tamagotchi pixel art inspirado en la gen 1
// para Waveshare ESP32-S3-Touch-AMOLED-1.75
//
// Librerias (Library Manager o repo de Waveshare):
//   - "GFX Library for Arduino" (moononournation), con soporte CO5300 QSPI
//   - "SensorLib" (Lewis He), driver tactil CST9217
//
// Placa: ESP32S3 Dev Module | Flash 16MB | PSRAM: OPI PSRAM | USB CDC On Boot: Enabled
//
// Los sprites y la tabla de especies se generan con tools/sprites.py (emit).

#include <Arduino.h>
#include <Wire.h>
#include "Arduino_GFX_Library.h"
#include "TouchDrvCSTXXX.hpp"
#include "pin_config.h"
#include "species.h"
#include "dex.h"
#include "pet.h"
#include "sdmon.h"
#include "rtcbat.h"
#include "i18n.h"
#include "audio.h"
#include "battle.h"

// Version del firmware. Subir este numero en cada release (y manifest.json para
// el instalador web). Se muestra en la pantalla de ajustes y por serie al arrancar.
#define FW_VERSION "1.22.3-daily-catch"

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(
  bus, LCD_RESET, 0 /*rotation*/, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
// Framebuffer completo en PSRAM: dibujamos todo y hacemos flush() (sin parpadeo)
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);

TouchDrvCST92xx touch;
Pet pet;

// sprite animado de la SD para la especie actual (si existe el archivo)
SdMon mon;          // sprite B/N (respaldo y minijuego si no hay PMD)
PmdMon pmd;         // sprite PMD multi-accion (pantalla principal)
PmdMon evoPmd;      // forma anterior, solo durante el parpadeo de evolucion
PmdMon wildPmd;     // rival salvaje en la pantalla de combate
int16_t monFor = -2;
bool monShinyFor = false;

// comportamiento del bicho en pantalla
struct {
  uint8_t mode = 0;     // 0 idle, 1 paseo, 2 gesto one-shot
  uint8_t act = PMD_IDLE;
  uint32_t t0 = 0;      // inicio de la animacion en curso
  uint32_t until = 0;   // fin del estado actual
  float x = 233, targetX = 233;
} beh;
#define PET_GROUND 304  // linea de suelo de la mascota
PmdMon galleryPmd;  // sprite grande de la vista detalle de la galeria (PMD/TPK2, legal)

// galeria pokedex
bool galleryOpen = false;
bool galleryDirty = false;
int galleryPage = 0;        // 10 paginas de 16
int16_t galleryDetail = 0;  // dex en vista detalle, 0 = rejilla
uint8_t galleryFilter = 0;  // 0 todos, 1 criados, 2 capturados

bool screenOff = false;       // pulsacion corta del boton PWR
bool cardOpen = false;        // ficha del bicho (deslizar vertical)
bool kbOpen = false;          // teclado para renombrar al bicho
char nameBuf[12] = "";
uint8_t nameLen = 0;
#define CARD_COUNT 7
uint8_t cardPage = 0;         // 0 perfil, 1 personalidad, 2 diario, 3 caja, 4 combate, 5 medallas, 6 progreso
uint8_t boxPage = 0;
uint8_t boxSort = 0;          // 0 dex, 1 tipo, 2 criados primero
bool clockOpen = false;       // pantalla de ajuste de hora (deslizar abajo)
int clockH = 12, clockM = 0;  // hora en edicion

// escena de bano: espuma sobre el bicho y limpieza al reventar
uint32_t bathUntil = 0;
bool bathPending = false;
struct { int16_t x, y; uint8_t r, ph; } bubbles[14];
uint32_t feedMenuUntil = 0;   // selector de comida abierto hasta este millis
uint32_t nextAmbientSoundAt = 0;

// minijuego "toques": mantener la pokeball en el aire
bool gameOpen = false;
bool gameMenuOpen = false;
uint8_t gameMode = 0;  // 0 ball, 1 catch, 2 memo
uint32_t gameOverUntil = 0;
float ballX, ballY, ballVX, ballVY, gamePetX;
uint8_t gameScore, gameMisses;
float hitX, hitY;             // ultimo golpe (anillo de impacto)
uint32_t hitTime = 0;
uint32_t ballLastHitAt = 0;
bool gameNewHi = false;
uint8_t gameGain = 0;
uint32_t catchUntil = 0, catchTargetUntil = 0;
int16_t catchX = 0, catchY = 0;
uint8_t catchIcon = 0;
uint8_t memoSeq[14] = { 0 };
uint8_t memoLen = 0, memoShow = 0, memoInput = 0, memoRounds = 0;
uint32_t memoNextAt = 0;
bool memoShowing = false;

// saco de entrenamiento (entrena la fuerza)
bool sackOpen = false;
uint32_t sackUntil = 0, sackOverUntil = 0;
uint16_t sackHits = 0;
float sackShake = 0;
uint8_t sackGain = 0;
bool sackNewHi = false;

bool battleOpen = false;
bool battleResolved = false;
int16_t battleDex = 0;
uint8_t battleLevel = 1;
BattleStats battlePlayer = {};
BattleStats battleEnemy = {};
BattleResult battleResult = {};
BattleRuntime battleRun = {};
BattleTurnResult battleTurn = {};
BattleAction battleLastAction = BATTLE_ATTACK;
BattleReward battleReward = {};
char battleMsg[28] = "";
uint32_t battleAttackMenuUntil = 0;
bool battleCatchOffered = false;
bool battleCatchTried = false;
bool battleCatchDone = false;
bool battleCatchSuccess = false;
bool battleRespectCatch = false;
uint8_t battleCatchChance = 0;

#define WILD_COOLDOWN_MS (20UL * 60UL * 1000UL)
#define WILD_PROMPT_MS 20000UL
#define WILD_CHECK_MS 60000UL
uint32_t wildPromptUntil = 0;
uint32_t nextWildEligible = 0;
uint32_t lastWildCheck = 0;
int16_t wildPromptDex = 0;
uint8_t wildPromptLevel = 1;

#define PET_EVENT_COOLDOWN_MS (15UL * 60UL * 1000UL)
#define PET_EVENT_PROMPT_MS 18000UL
#define PET_EVENT_CHECK_MS 60000UL
uint32_t petEventUntil = 0;
uint32_t nextPetEventEligible = 0;
uint32_t lastPetEventCheck = 0;
uint8_t petEventType = PET_EVENT_BERRY;
uint32_t petEventFeedbackUntil = 0;
char petEventMsg[18] = "";

// las 9 especies con sprite propio en flash (respaldo sin SD): dex -> indice
int flashIdxForDex(int16_t dex) {
  static const int8_t IDX[10] = { -1, 3, 4, 5, 0, 1, 2, 6, 7, 8 };
  return (dex >= 1 && dex <= 9) ? IDX[dex] : -1;
}

#define CX 233  // centro de la pantalla redonda
#define CY 233
#define PET_CY 202  // centro vertical del sprite

static const uint16_t INK_K = 0x18C4;  // spriteColor('k')

// botones de icono siguiendo el arco inferior de la pantalla redonda
// (los exteriores van mas altos para no salirse del circulo)
struct Btn {
  int16_t cx, cy;
  const char *const *icon;
};
Btn buttons[4] = {
  { 140, 390, SPR_ICON_FOOD },   // comer
  { 202, 404, SPR_ICON_PLAY },   // jugar
  { 264, 404, SPR_ICON_LIGHT },  // luz
  { 326, 390, SPR_ICON_CLEAN },  // bano
};
#define BTN_HALF 26  // boton de 52x52
#define BTN_HIT 36   // radio tactil (un poco mas generoso)

// grietas del huevo (pixeles 'k' sobre el sprite)
static const uint8_t CRACK1[][2] = { {15,8},{16,9},{15,10} };
static const uint8_t CRACK2[][2] = { {11,13},{12,14},{11,15},{20,12},{19,13},{20,14} };
// estrellas del modo noche
static const uint16_t STARS[][2] = { {120,140},{330,120},{370,210},{95,230},{280,90},{160,95} };

bool wasPressed = false;
// eleccion de inicial (primera partida): Bulbasaur / Charmander / Squirtle, 3 filas
static const int16_t STARTER_DEX[3] = { 1, 4, 7 };
#define STARTER_ROW_Y 110
#define STARTER_ROW_H 70
#define STARTER_ROW_GAP 8
// boton-CTA de evolucion (centrado, mitad de pantalla)
#define EVO_BTN_W 256
#define EVO_BTN_H 64
#define EVO_BTN_X (CX - EVO_BTN_W / 2)
#define EVO_BTN_Y 172
// boton-CTA de despedida (mas ancho: lleva el nombre + frase)
#define FAR_BTN_W 408
#define FAR_BTN_H 58
#define FAR_BTN_X (CX - FAR_BTN_W / 2)
#define FAR_BTN_Y 176
// el CST9217 avisa por el pin INT cuando hay datos tactiles; lo usamos para no
// leer el bus I2C mientras el chip esta dormido (esa lectura se colgaba ~1s)
volatile bool gTouchIrq = false;
void IRAM_ATTR touchIsr() { gTouchIrq = true; }
uint32_t lastRender = 0;
// proteccion del AMOLED: atenuado por inactividad
uint32_t lastInteract = 0;
uint8_t dimStage = 0;        // 0 despierto, 1 atenuado (90s), 2 casi apagado (5min)
bool swallowGesture = false; // el toque que despierta no acciona nada
uint32_t holdStart = 0;     // pulsacion larga sobre el bicho
uint32_t confirmUntil = 0;  // dialogo "soltar?" activo hasta este millis
uint8_t choiceKind = 0;     // dialogo de decision: 0 ninguno, 1 evolucion, 2 despedida
uint32_t choiceUntil = 0;   // se cierra solo a este millis
int16_t tX0, tY0, tXl, tYl; // gesto en curso (inicio y ultima posicion)
uint32_t tStart = 0;
bool holdFired = false;

void setup() {
  Serial.setRxBufferSize(8192);  // la transferencia a SD llega en bloques de 2 KB
  Serial.begin(115200);
  // CRITICO: sin esto, Serial.print BLOQUEA el juego cuando no hay un
  // monitor serie abierto en el host (el bufer TX del USB CDC se llena
  // y nadie lo vacia) -> con timeout 0 los mensajes se descartan
  Serial.setTxTimeoutMs(0);
  Serial.printf("TamaPoke fw v%s\n", FW_VERSION);
  loadLang();  // idioma guardado (ES por defecto)
  Wire.begin(IIC_SDA, IIC_SCL);
  // CST9217 (tactil), AXP2101 (PMU) y PCF85063 (RTC) comparten este bus I2C.
  // Red de seguridad para PMU/RTC (SensorLib NO respeta este timeout en el
  // tactil; el cuelgue del tactil dormido se resuelve gateando por INT, ver
  // handleTouch).
  Wire.setTimeOut(50);

  // CRITICO: encender la alimentacion del panel (BLDO1=OLED VDD 3.3V) ANTES de
  // inicializar el display. Si el PMU se reseteo (drenaje total), este rail
  // queda OFF y la pantalla se ve negra aunque el resto de la placa funcione.
  pmuEnablePanel();

  // QSPI a 80MHz (por defecto 40): el flush del framebuffer es el cuello de
  // botella del fps (~56ms a 40MHz). Si el panel mostrara basura, bajar a 40M.
  if (!gfx->begin(80000000)) Serial.println("gfx->begin() fallo");
  panel->setBrightness(180);

  touch.setPins(TP_RESET, TP_INT);
  bool touchOk = false;
  for (int i = 0; i < 3 && !touchOk; i++) {  // a veces falla al primer intento
    touchOk = touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL);
    if (!touchOk) delay(150);
  }
  if (!touchOk) Serial.println("CST9217 no detectado");
  // begin() deja el chip en modo comando (lee la identidad y no sale);
  // hace falta un reset por hardware para que vuelva a reportar toques
  touch.reset();
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(true, true);  // el panel esta montado girado 180 grados
  // INT activo-bajo: salta cuando hay datos. Gatea las lecturas I2C (ver loop)
  pinMode(TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TP_INT), touchIsr, FALLING);

  pet.begin();
  Serial.printf("SAVE %s spec=%d lv=%u wins=%u losses=%u streak=%u\n",
                pet.saveLoadedFromNvs ? "loaded" : "created",
                pet.speciesId, pet.level(), pet.battleWins, pet.battleLosses, pet.battleStreak);
  sdBegin();
  thumbs.load();

  // reloj real: aplica el tiempo que estuvo apagado
  rtcBegin();
  batBegin();
  pwrSetup();
  uint32_t e = rtcEpoch();
  if (e == 0) {
    rtcSetEpoch(1767225600UL);  // RTC virgen: semilla (la hora absoluta da igual,
    e = rtcEpoch();             // solo importan las diferencias)
    Serial.println("RTC sin hora: sembrado, sin progresion offline esta vez");
  }
  pet.syncClock(e);

  audioBegin();  // ES8311 + I2S + amplificador (suena un jingle de arranque)

  lastInteract = millis();
}

// carga/descarga el sprite de SD cuando cambia la especie
void ensureMon() {
  if (pet.speciesId == monFor && monShinyFor == pet.shiny && !sdDirty) return;
  sdDirty = false;
  monFor = pet.speciesId;
  monShinyFor = pet.shiny;
  mon.unload();
  pmd.unload();
  beh.x = beh.targetX = 233;
  beh.mode = 0;
  beh.until = 0;
  if (pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    pmd.load(pet.speciesId, pet.shiny);          // principal: PMD
    if (!pmd.loaded) mon.load(pet.speciesId, pet.shiny);  // respaldo: B/N
  }
}

bool mainScreenReadyForAmbientSound() {
  if (audioMode() != SOUND_FULL || screenOff || dimStage > 0) return false;
  if (pet.awaitingStarter() || pet.isEgg() || pet.sleeping || pet.ceremony) return false;
  if (battleOpen || gameOpen || gameMenuOpen || sackOpen || cardOpen || galleryOpen || kbOpen || clockOpen) return false;
  if (feedMenuUntil || confirmUntil || choiceKind || bathUntil || wildPromptUntil || petEventUntil) return false;
  if (pet.evolving() || pet.wantEvolveButton() || pet.canRunawayNow() || pet.wantFarewellButton()) return false;
  return true;
}

void maybePlayAmbientSound(uint32_t now) {
  if (!mainScreenReadyForAmbientSound()) {
    nextAmbientSoundAt = now + 18000;
    return;
  }
  if (nextAmbientSoundAt == 0) nextAmbientSoundAt = now + 22000 + random(18000);
  if (now < nextAmbientSoundAt) return;
  uint8_t r = (uint8_t)random(3);
  sfxPlay(r == 0 ? SFX_HEART : (r == 1 ? SFX_EVENT_SPARKLE : SFX_MENU));
  nextAmbientSoundAt = now + 28000 + random(26000);
}

void loop() {
  uint32_t now = millis();
  pet.update(now);

  // avisa con un sonido cuando el bicho pasa a estar listo para evolucionar
  // (incluye el caso de cumplir al despertar). canEvolveNow es false durmiendo.
  static bool wasEvoReady = false;
  bool evoReady = pet.wantEvolveButton();
  if (evoReady && !wasEvoReady) sfxPlay(SFX_MEDAL);
  wasEvoReady = evoReady;
  // aviso sombrio cuando el bicho esta a punto de escaparse por abandono
  static bool wasRunReady = false;
  bool runReady = pet.canRunawayNow();
  if (runReady && !wasRunReady) sfxPlay(SFX_DENY);
  wasRunReady = runReady;

  handleTouch();
  handleSerial();
  ensureMon();
  pet.ensureDailyGoals();
  maybeOfferWildEncounter(now);
  maybeOfferPetEvent(now);
  maybePlayAmbientSound(now);

  // pulsacion corta del PWR: pantalla on/off
  static uint32_t lastPwr = 0;
  if (now - lastPwr > 250) {
    lastPwr = now;
    if (pwrShortPressed()) {
      screenOff = !screenOff;
      if (!screenOff) lastInteract = now;
    }
  }

  updateBrightness(now);

  // vuelca el autoguardado periodico SOLO con la pantalla atenuada/apagada o
  // durmiendo: la escritura a NVS congela ~1s ambos cores (caché de flash off),
  // y aqui no hay animacion que se corte ni dedo esperando respuesta. Con 90s
  // de inactividad la pantalla ya atenua, asi que se vuelca enseguida; el uso
  // activo persiste igual por los guardados de cada accion (comer/jugar/...).
  if (pet.savePending() && (screenOff || dimStage >= 1 || pet.sleeping)) {
    pet.flushSave();
  }

  // anota la hora real cada 30 s (se persiste en cada save del juego)
  static uint32_t lastClock = 0;
  if (now - lastClock > 30000) {
    lastClock = now;
    uint32_t e = rtcEpoch();
    if (e) pet.lastSeenEpoch = e;
  }

  // latido de salud cada 5 min (para el soak test; se descarta si no hay monitor)
  static uint32_t lastHealth = 0;
  if (now - lastHealth > 300000) {
    lastHealth = now;
    Serial.printf("HEALTH up=%lus heap=%u min=%u\n", (unsigned long)(now / 1000),
                  ESP.getFreeHeap(), ESP.getMinFreeHeap());
  }

  // 85 ms en juego/saco: margen seguro para que el redibujado no pise el envio
  // DMA del frame anterior (a 40-65 ms solapaba y causaba flashes negros; con
  // sprites grandes el dibujo tarda mas, asi que se deja colchon)
  if (now - lastRender >= (uint32_t)((gameOpen || sackOpen || battleOpen) ? 85 : 100)) {
    lastRender = now;
    render();
  }
}

// brillo segun sueno + inactividad (proteccion del AMOLED)
void updateBrightness(uint32_t now) {
  // los eventos visibles despiertan la pantalla solos
  if (pet.evolving() || pet.ceremony || pet.eating() || pet.showHeart()) {
    lastInteract = now;
  }
  uint32_t idle = now - lastInteract;
  dimStage = (idle > 300000) ? 2 : (idle > 90000) ? 1 : 0;
  uint8_t target = pet.sleeping ? 25 : (usbPresent() ? 180 : 145);
  if (dimStage == 1) target = pet.sleeping ? 10 : 60;
  else if (dimStage == 2) target = 8;
  if (screenOff) target = 0;
  static uint8_t current = 255;
  if (target != current) {
    current = target;
    panel->setBrightness(target);
  }
}

// ---------- consola serie (provision de SD + depuracion) ----------

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  if (sdSerialCommand(line)) return;

  if (line == "HATCH") {
    pet.eggTap(); pet.eggTap(); pet.eggTap();
    Serial.println("DONE");
  } else if (line.startsWith("SPEC ")) {
    int n = line.substring(5).toInt();
    if (n >= 1 && n <= DEX_COUNT) {
      pet.prevSpeciesId = pet.speciesId;
      pet.speciesId = n;
      Serial.printf("especie #%d %s\n", n, DEX_TBL[n].name);
    }
    Serial.println("DONE");
  } else if (line.startsWith("LVL ")) {
    pet.ageMinutes = (uint32_t)line.substring(4).toInt() * MINUTES_PER_LEVEL;
    Serial.println("DONE");
  } else if (line.startsWith("TIME ")) {
    uint32_t e = (uint32_t)line.substring(5).toInt();
    rtcSetEpoch(e);
    pet.setClock(e);
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line.startsWith("RTCSET ")) {  // solo RTC (simular apagados en pruebas)
    rtcSetEpoch((uint32_t)line.substring(7).toInt());
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "TIME") {
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "GAL") {
    galleryOpen = !galleryOpen;
    galleryDetail = 0;
    galleryDirty = true;
    if (!galleryOpen) galleryPmd.unload();
    Serial.println("DONE");
  } else if (line == "EGGS") {
    // simula 20 tiradas de huevo (no cambia el estado del juego)
    for (int i = 0; i < 20; i++) {
      int16_t d = pet.pickEggSpecies();
      Serial.printf("%d:%s(r%u) ", d, DEX_TBL[d].name, DEX_TBL[d].rarity);
    }
    Serial.println();
    Serial.println("DONE");
  } else if (line == "SHINY") {  // alterna shiny del actual (pruebas)
    pet.shiny = !pet.shiny;
    Serial.printf("shiny=%d\n", pet.shiny);
    Serial.println("DONE");
  } else if (line.startsWith("NICK ")) {
    pet.rename(line.substring(5).c_str());
    Serial.printf("nick=%s\n", pet.nick);
    Serial.println("DONE");
  } else if (line == "CAREDAY") {  // simula un dia nuevo cuidado (pruebas)
    pet.setClock(pet.lastSeenEpoch + 86400);
    pet.caress();
    Serial.printf("streak=%u bond=%u medals=0x%X\n", pet.streak, pet.bond, pet.medals);
    Serial.println("DONE");
  } else if (line == "BYE") {
    pet.startFarewell();
    Serial.println("DONE");
  } else if (line == "RUN") {
    pet.startRunaway();
    Serial.println("DONE");
  } else if (line == "BEEP") {
    sfxPlay(SFX_HATCH);  // prueba de audio
    Serial.println("DONE");
  } else if (line == "ABANDON") {
    pet.dbgRunawayReady();  // fuerza el estado "lista para escaparse" (test del boton)
    Serial.println("DONE");
  } else if (line == "WIPE") {
    pet.factoryReset();     // borra NVS y reinicia -> partida nueva (eleccion de inicial)
    Serial.println("DONE");
    delay(100);
    ESP.restart();
  } else if (line == "REG") {
    Serial.printf("pokedex %u/151:", pet.registeredCount());
    for (int i = 1; i <= 151; i++)
      if (pet.isRegistered(i)) Serial.printf(" %d", i);
    Serial.println();
    Serial.println("DONE");
  } else if (line == "SAVEINFO") {
    Serial.printf("fw=%s save=%s createdBoot=%d spec=%d level=%u egg=%d starter=%d age=%lu\n",
                  FW_VERSION, pet.saveLoadedFromNvs ? "loaded" : "created",
                  pet.saveCreatedThisBoot, pet.speciesId, pet.level(), pet.isEgg(),
                  pet.awaitingStarter(), (unsigned long)pet.ageMinutes);
    Serial.printf("battle wins=%u losses=%u streak=%u best=%u nick=%s\n",
                  pet.battleWins, pet.battleLosses, pet.battleStreak,
                  pet.bestBattleStreak, pet.nick);
    Serial.printf("records ball=%u catch=%u memo=%u sack=%u\n",
                  pet.gameHi, pet.catchHi, pet.memoHi, pet.strHi);
    Serial.println("DONE");
  } else if (line == "HEALTH") {
    Serial.printf("up=%lus heap=%u min=%u sd=%d mon=%d\n",
                  (unsigned long)(millis() / 1000), ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(), sdReady, pmd.loaded || mon.loaded);
    Serial.println("DONE");
  } else if (line == "STATS") {
    Serial.printf("spec=%d nv=%u com=%u fel=%u ene=%u lim=%u desc=%u sd=%d mon=%d bat=%d usb=%d rtc=%u\n",
                  pet.speciesId, pet.level(), pet.fullness, pet.joy, pet.energy,
                  pet.hygiene, pet.careMistakes, sdReady, mon.loaded,
                  batPercent(), usbPresent(), rtcEpoch());
    Serial.printf("peso=%u fue=%u def=%u vel=%u genes=%u/%u/%u tr=%u/%u/%u baya=%d\n",
                  pet.weight, pet.atkStat(), pet.defStat(), pet.speStat(),
                  pet.geneAtk, pet.geneDef, pet.geneSpe,
                  pet.trAtk, pet.trDef, pet.trSpe, pet.berryKnown);
    Serial.printf("shiny=%d streak=%u/%u bond=%u medals=0x%X(%u) nick=%s\n",
                  pet.shiny, pet.streak, pet.bestStreak, pet.bond, pet.medals,
                  pet.totalMedals, pet.nick);
    Serial.println("DONE");
  }
}

// ---------- entrada tactil ----------

bool inPetZone(int16_t x, int16_t y) {
  return x > 110 && x < 356 && y > 95 && y < 310;
}

// el toque se resuelve al LEVANTAR el dedo para distinguir tap de deslizar
void handleTouch() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 20) return;  // 50 Hz le sobra a un dedo
  lastPoll = millis();
  // solo tocamos el bus si el chip aviso por INT o si el dedo sigue abajo (hay
  // que detectar el levantamiento). Leer el CST9217 dormido se colgaba ~1s y
  // congelaba el loop entero; SensorLib no respeta el timeout de Wire.
  if (!gTouchIrq && !wasPressed) return;
  gTouchIrq = false;
  int16_t x, y;
  bool pressed = touch.getPoint(&x, &y, 1) > 0;

  // saco de entrenamiento: cada toque cuenta al instante (aporrear rapido)
  if (sackOpen) {
    if (pressed && !wasPressed) {
      lastInteract = millis();
      if (y < 72) sackOpen = false;  // tocar arriba = abandonar
      else sackTap();
    }
    wasPressed = pressed;
    return;
  }

  if (pressed && !wasPressed) {  // empieza el gesto
    tX0 = tXl = x;
    tY0 = tYl = y;
    tStart = millis();
    holdFired = false;
    swallowGesture = (dimStage > 0) || screenOff;  // si estaba a oscuras, solo despierta
    screenOff = false;
    lastInteract = millis();
    if (!swallowGesture && gameOpen && gameMode == 1) {
      catchTap(x, y);        // juego de reflejos: cuenta al tocar, no al soltar
      swallowGesture = true; // evita doble tap al levantar el dedo
    }
  } else if (pressed) {  // sigue apoyado
    tXl = x;
    tYl = y;
    // pulsacion larga sin moverse sobre el bicho -> dialogo de soltar
    if (!holdFired && !swallowGesture && !galleryOpen && !cardOpen && !kbOpen && !clockOpen && millis() - tStart > 3000 &&
        abs(tXl - tX0) < 30 && abs(tYl - tY0) < 30 && inPetZone(tX0, tY0) &&
        !pet.isEgg() && !confirmUntil && !pet.ceremony) {
      confirmUntil = millis() + 10000;
      holdFired = true;
    }
  } else if (wasPressed) {  // levanta el dedo: resolver gesto
    lastInteract = millis();
    int dx = tXl - tX0, dy = tYl - tY0;
    uint32_t dt = millis() - tStart;
    if (!holdFired && !swallowGesture) {
      if (abs(dx) > 80 && abs(dy) < 70 && dt < 800) onSwipe(dx > 0 ? 1 : -1);
      else if (abs(dy) > 80 && abs(dx) < 70 && dt < 800) onSwipeV(dy > 0 ? 1 : -1);
      else if (dt < 1500 && abs(dx) < 40 && abs(dy) < 40) onTap(tX0, tY0);
    }
  }
  wasPressed = pressed;
}

// deslizar vertical: abre/cierra la ficha del bicho
void openClock();  // prototipo
int16_t boxDexAt(uint16_t index);
uint8_t boxPageCount();
uint8_t currentDayPhase();
StrId dayPhaseTextId(uint8_t phase);

void onSwipeV(int dir) {
  if (pet.awaitingStarter()) return;  // bloqueado durante la eleccion de inicial
  if (wildPromptUntil && millis() < wildPromptUntil) return;
  if (wildPromptUntil) wildPromptUntil = 0;
  if (gameMenuOpen) return;
  if (gameOpen || galleryOpen || kbOpen || sackOpen || battleOpen || pet.ceremony) return;
  if (clockOpen) { clockOpen = false; sfxPlay(SFX_TAP); return; }
  if (cardOpen) {
    if (dir < 0) { cardOpen = false; sfxPlay(SFX_TAP); }  // arriba cierra la ficha
    return;
  }
  if (dir > 0) {                    // deslizar abajo: ajustar hora
    if (!confirmUntil && !feedMenuUntil) openClock();
  } else if (!pet.isEgg() && !confirmUntil && !feedMenuUntil) {
    cardOpen = true;                // deslizar arriba: ficha
    cardPage = 0;
    sfxPlay(SFX_MENU);
  }
}

// deslizar: dir +1 = hacia la derecha
void onSwipe(int dir) {
  if (pet.awaitingStarter()) return;  // bloqueado durante la eleccion de inicial
  if (wildPromptUntil && millis() < wildPromptUntil) return;
  if (wildPromptUntil) wildPromptUntil = 0;
  if (gameMenuOpen) return;
  if (gameOpen || kbOpen || clockOpen || battleOpen) return;
  if (cardOpen) {  // dentro de la ficha: cambiar paginas
    int p = (int)cardPage + (dir > 0 ? -1 : 1);  // izquierda avanza
    uint8_t old = cardPage;
    cardPage = p < 0 ? 0 : (p >= CARD_COUNT ? CARD_COUNT - 1 : p);
    if (cardPage != old) sfxPlay(SFX_MENU);
    return;
  }
  if (!galleryOpen) {
    if (!pet.ceremony && !confirmUntil) {
      galleryOpen = true;
      galleryPage = 0;
      galleryDetail = 0;
      galleryDirty = true;
      sfxPlay(SFX_MENU);
    }
    return;
  }
  if (galleryDetail) {  // en detalle: volver a la rejilla
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    return;
  }
  int np = galleryPage - dir;  // deslizar a la izquierda avanza pagina
  int maxPage = galleryPageCount() - 1;
  if (np < 0) {                // retroceder desde la primera = salir
    galleryOpen = false;
    galleryPmd.unload();
    sfxPlay(SFX_TAP);
    return;
  }
  if (np > maxPage) np = maxPage;
  if (np != galleryPage) {
    galleryPage = np;
    galleryDirty = true;
    sfxPlay(SFX_MENU);
  }
}

void onTap(int16_t x, int16_t y) {
  // Serial.printf("TOUCH %d %d\n", x, y);  // diagnostico (silenciado: satura el log)
  if (pet.awaitingStarter()) {  // primera partida: elegir inicial
    for (int i = 0; i < 3; i++) {
      int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
      if (x >= 70 && x <= 396 && y >= ry && y <= ry + STARTER_ROW_H) {
        pet.chooseStarter(STARTER_DEX[i]);
        sfxPlay(SFX_TAP);
        break;
      }
    }
    return;
  }
  if (galleryOpen) {
    galleryTap(x, y);
    return;
  }
  if (kbOpen) {
    keyboardTap(x, y);
    return;
  }
  if (clockOpen) {
    clockTap(x, y);
    return;
  }
  if (pet.ceremony) return;  // durante la despedida no hay botones
  if (cardOpen) {
    if (cardPage == 0 && y < 84) openKeyboard();  // tocar el nombre = renombrar
    else if (cardPage == 3) {
      if (x >= 302 && x <= 408 && y >= 62 && y <= 92) {
        boxSort = (boxSort + 1) % 3;
        boxPage = 0;
        sfxPlay(SFX_TAP);
      } else if (x >= 76 && x <= 170 && y >= 348 && y <= 386) {
        if (boxPage > 0) boxPage--;
        sfxPlay(SFX_TAP);
      } else if (x >= 296 && x <= 390 && y >= 348 && y <= 386) {
        uint8_t pages = boxPageCount();
        if (boxPage + 1 < pages) boxPage++;
        sfxPlay(SFX_TAP);
      } else {
        int row = (y - 122) / 42;
        if (row >= 0 && row < 5 && x >= 58 && x <= 408 && y >= 122 + row * 42 && y <= 156 + row * 42) {
          int16_t dex = boxDexAt((uint16_t)boxPage * 5 + row);
          if (dex > 0) {
            cardOpen = false;
            galleryOpen = true;
            galleryDetail = dex;
            galleryDirty = true;
            galleryPmd.load(dex, pet.isShinyRegistered(dex));
            sfxPlay(SFX_TAP);
          }
        } else if (y >= 400) {
          cardOpen = false;
        }
      }
    } else if (cardPage == 4 && y >= 286 && y <= 336 && x >= 82 && x <= 384) {
      cardOpen = false;
      startBattle();
    } else if (cardPage == 4 && y >= 330 && y <= 388 && x >= 82 && x <= 384) {
      cardOpen = false;            // boton ENTRENAR FUERZA
      startSack();
    } else if (y >= 400) {
      cardOpen = false;
      sfxPlay(SFX_TAP);
    }
    return;
  }
  if (gameOpen) {
    gameTap(x, y);
    return;
  }
  if (battleOpen) {
    battleTap(x, y);
    return;
  }
  if (petEventUntil && millis() < petEventUntil && inPetEventHit(x, y)) {
    acceptPetEvent();
    return;
  }
  if (gameMenuOpen) {
    if (x >= 88 && x <= 378 && y >= 176 && y <= 224) startGame();
    else if (x >= 88 && x <= 378 && y >= 236 && y <= 284) startCatchGame();
    else if (x >= 88 && x <= 378 && y >= 296 && y <= 344) startMemoGame();
    else { gameMenuOpen = false; sfxPlay(SFX_TAP); }
    return;
  }
  if (wildPromptUntil) {
    if (millis() < wildPromptUntil) {
      bool fight = (x >= 93 && x <= 373 && y >= 226 && y <= 270);
      bool later = (x >= 93 && x <= 373 && y >= 278 && y <= 322);
      if (fight) {
        startBattleWith(wildPromptDex, wildPromptLevel);
      } else if (later) {
        wildPromptUntil = 0;
        scheduleNextWild(millis());
        sfxPlay(SFX_TAP);
      }
    } else {
      wildPromptUntil = 0;
    }
    return;
  }
  if (choiceKind) {          // dialogo de decision: boton accion (arriba) / mantener (abajo)
    bool b1 = (x >= 93 && x <= 373 && y >= 206 && y <= 258);  // accion
    bool b2 = (x >= 93 && x <= 373 && y >= 268 && y <= 320);  // mantener / quedaros
    if (choiceKind == 1) {                 // evolucion
      if (b1) { int16_t old = pet.speciesId; pet.evolve(); evoPmd.load(old, pet.shiny); }
      else if (b2) pet.declineEvolve();
    } else if (choiceKind == 2) {          // despedida
      if (b1) pet.startFarewell();
      else if (b2) pet.declineFarewell();
    }
    choiceKind = 0;
    return;
  }
  if (confirmUntil) {        // dialogo "soltar?": SI / NO
    if (millis() < confirmUntil && x >= 118 && x <= 218 && y >= 252 && y <= 304) {
      pet.release();
    }
    confirmUntil = 0;
    return;
  }
  if (feedMenuUntil) {       // selector de comida
    if (millis() < feedMenuUntil && y >= 288 && y <= 352 && x >= 101 && x <= 365) {
      int item = (x - 101) / 66;
      if (item == 3) pet.feedCandy();
      else pet.feedBerry(item);
      sfxPlay(SFX_EAT);
    }
    feedMenuUntil = 0;
    return;
  }
  if (pet.isEgg()) {
    pet.eggTap();
    sfxPlay(SFX_TAP);
    return;
  }
  // boton de evolucion: abre el dialogo evolucionar/mantener
  if (pet.wantEvolveButton() && x >= EVO_BTN_X && x <= EVO_BTN_X + EVO_BTN_W &&
      y >= EVO_BTN_Y && y <= EVO_BTN_Y + EVO_BTN_H) {
    choiceKind = 1; choiceUntil = millis() + 12000;
    return;
  }
  // botones de final (mismo recuadro): escapada directa; despedida abre dialogo
  if (x >= FAR_BTN_X && x <= FAR_BTN_X + FAR_BTN_W &&
      y >= FAR_BTN_Y && y <= FAR_BTN_Y + FAR_BTN_H) {
    if (pet.canRunawayNow()) { pet.startRunaway(); return; }
    if (pet.wantFarewellButton()) { choiceKind = 2; choiceUntil = millis() + 12000; return; }
  }
  for (int i = 0; i < 4; i++) {
    int dx = x - buttons[i].cx, dy = y - buttons[i].cy;
    if (dx * dx + dy * dy <= BTN_HIT * BTN_HIT) {
      Serial.printf("BTN %d\n", i);
      sfxPlay(SFX_TAP);
      if (i == 0) {
        if (!pet.sleeping) {
          feedMenuUntil = millis() + 6000;
          sfxPlay(SFX_MENU);
        }
      } else if (i == 1) {
        if (!pet.sleeping) {
          gameMenuOpen = true;
          sfxPlay(SFX_MENU);
        }
      } else if (i == 2) {
        pet.toggleLight();
      } else {
        startBath();
      }
      return;
    }
  }
  // tocar al bicho = caricia
  if (inPetZone(x, y)) {
    Serial.println("PET");
    uint8_t r = pet.interactPet(currentDayPhase() == 2);
    StrId msg = S_WAIT;
    if (r & PET_INTERACT_BOND) msg = S_BOND_GAIN;
    else if (r & PET_INTERACT_JOY) msg = S_HAPPY_FB;
    snprintf(petEventMsg, sizeof(petEventMsg), "%s", T(msg));
    petEventFeedbackUntil = millis() + 1600;
    if (!pet.sleeping) sfxPlay((r & PET_INTERACT_BOND) ? SFX_HEART : SFX_TAP);
  }
}

// ---------- render ----------

bool gNight = false;  // noche real (por hora) o durmiendo: lo fija render()
uint16_t inkColor() { return gNight ? UI_INK_NIGHT : UI_INK; }

// ---------- escena de fondo: bioma del tipo + hora real del RTC ----------

#define C565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define HORIZON 232  // linea donde el cielo se encuentra con el suelo

uint16_t lerp565(uint16_t a, uint16_t b, int i, int n) {
  if (n <= 0) return a;
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)((((ar + (br - ar) * i / n) << 11)) |
                    (((ag + (bg - ag) * i / n) << 5)) | (ab + (bb - ab) * i / n));
}

// hora del dia 0-23 (de la hora real cacheada cada 30s; 13 si no hay reloj)
int sceneHour() {
  uint32_t e = pet.lastSeenEpoch;
  return e ? (int)((e / 3600) % 24) : 13;
}

uint8_t currentDayPhase() {
  int h = sceneHour();
  if (h >= 6 && h < 12) return 0;
  if (h >= 12 && h < 18) return 1;
  if (h >= 18 && h < 22) return 2;
  return 3;
}

StrId dayPhaseTextId(uint8_t phase) {
  switch (phase) {
    case 0: return S_MORNING;
    case 2: return S_EVENING;
    case 3: return S_NIGHT;
    default: return S_DAY;
  }
}

// suelo de cada bioma de dia (de noche se mezcla hacia el azul nocturno)
static const uint16_t BIOME_SOIL[6] = {
  C565(0x7e, 0xc0, 0x7f),  // 0 pradera
  C565(0xdc, 0xca, 0x94),  // 1 playa (arena)
  C565(0x4f, 0x8a, 0x55),  // 2 bosque
  C565(0x8a, 0x55, 0x44),  // 3 volcan
  C565(0xa8, 0x90, 0x6a),  // 4 montana
  C565(0xe6, 0xee, 0xf5),  // 5 nieve
};

void drawClouds(uint32_t now, uint16_t col) {
  for (int k = 0; k < 2; k++) {
    int cx = (int)((now / 50 + k * 250) % 560) - 40;
    int cy = 70 + k * 34;
    gfx->fillCircle(cx, cy, 16, col);
    gfx->fillCircle(cx + 18, cy + 3, 13, col);
    gfx->fillCircle(cx - 15, cy + 4, 12, col);
  }
}

void drawScene(uint8_t biome, uint32_t now, bool night) {
  int h = sceneHour();
  uint16_t top, bot;
  if (night)            { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (h < 8)       { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }  // amanecer
  else if (h < 18)      { top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }  // dia
  else                  { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }  // atardecer

  // cielo en bandas
  for (int y = 0; y < HORIZON; y += 8)
    gfx->fillRect(0, y, 466, 8, lerp565(top, bot, y, HORIZON));

  // sol o luna
  if (night) {
    gfx->fillCircle(360, 78, 24, C565(0xe8, 0xee, 0xf5));
    gfx->fillCircle(370, 72, 22, lerp565(top, bot, 78, HORIZON));  // creciente
    for (auto &st : STARS) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  } else if (h < 18) {
    gfx->fillCircle(360, 84, 26, h < 8 ? C565(0xff, 0xd9, 0x8a) : C565(0xff, 0xe7, 0x9f));
    drawClouds(now, C565(0xff, 0xff, 0xff));
  } else {
    gfx->fillCircle(233, HORIZON - 6, 34, C565(0xff, 0xf1, 0xc8));  // sol poniente
  }

  // mar de la playa: una franja de agua sobre la arena
  uint16_t soil = BIOME_SOIL[biome < 6 ? biome : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  if (biome == 1) {
    uint16_t sea = night ? C565(0x1c, 0x34, 0x52) : C565(0x4f, 0x96, 0xc4);
    gfx->fillRect(0, HORIZON - 26, 466, 26, sea);
    for (int i = 0; i < 3; i++) {
      int wy = HORIZON - 22 + i * 7;
      uint16_t fc = night ? C565(0x3a, 0x58, 0x78) : C565(0xbf, 0xe6, 0xf5);
      gfx->fillRect(60 + ((now / 60 + i * 30) % 60), wy, 26, 2, fc);
      gfx->fillRect(300 - ((now / 60 + i * 20) % 60), wy, 26, 2, fc);
    }
  }

  // suelo
  gfx->fillRect(0, HORIZON, 466, 466 - HORIZON, soil);
  uint16_t hill = lerp565(soil, night ? C565(0x0c, 0x12, 0x24) : C565(0xff, 0xff, 0xff), 3, 16);
  gfx->fillRoundRect(-60, HORIZON - 14, 586, 60, 30, hill);

  // detalles del bioma
  uint16_t dk = lerp565(soil, C565(0x10, 0x18, 0x20), night ? 11 : 7, 16);
  if (biome == 2) {  // bosque: coniferas en silueta
    for (int tx : { 60, 150, 360, 416 }) {
      gfx->fillTriangle(tx, HORIZON - 46, tx - 16, HORIZON, tx + 16, HORIZON, dk);
      gfx->fillTriangle(tx, HORIZON - 60, tx - 12, HORIZON - 28, tx + 12, HORIZON - 28, dk);
    }
  } else if (biome == 3) {  // volcan: rocas y brasas
    gfx->fillTriangle(70, HORIZON, 40, HORIZON + 30, 100, HORIZON + 30, dk);
    gfx->fillTriangle(400, HORIZON + 4, 372, HORIZON + 30, 430, HORIZON + 30, dk);
    if (!night)
      for (int e = 0; e < 4; e++)
        gfx->fillRect(120 + e * 70, HORIZON + 8 + (e % 2) * 6, 4, 4, C565(0xff, 0x9b, 0x3a));
  } else if (biome == 4) {  // montana: cumbres al fondo
    gfx->fillTriangle(140, HORIZON - 50, 60, HORIZON, 220, HORIZON, dk);
    gfx->fillTriangle(330, HORIZON - 38, 250, HORIZON, 410, HORIZON, dk);
  } else if (biome == 5 && !night) {  // nieve: copos cayendo
    for (int f = 0; f < 10; f++) {
      int fx = (f * 53 + now / 40) % 466;
      int fy = (f * 90 + now / 18) % HORIZON;
      gfx->fillRect(fx, fy, 3, 3, UI_WHITE);
    }
  } else if (biome == 0) {  // pradera: matas de hierba
    for (int gx : { 80, 175, 300, 395 })
      for (int b = -1; b <= 1; b++)
        gfx->fillRect(gx + b * 5, HORIZON + 6, 2, 8 + (b == 0 ? 4 : 0), dk);
  }
}

// primera partida: elige inicial entre Bulbasaur / Charmander / Squirtle
void renderStarterSelect() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  const char *t = T(S_CHOOSE_STARTER);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(t) * 6, 68);
  gfx->print(t);
  for (int i = 0; i < 3; i++) {
    int16_t d = STARTER_DEX[i];
    const DexEntry &de = DEX_TBL[d];
    int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
    gfx->fillRoundRect(70, ry, 326, STARTER_ROW_H, 14, lerp565(de.accent, UI_WHITE, 6, 8));
    gfx->drawRoundRect(70, ry, 326, STARTER_ROW_H, 14, de.accent);
    const uint8_t *th = thumbs.get(d);     // miniatura del inicial (si la SD esta lista)
    if (th) drawThumb(th, 76, ry - 5, 3, false);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(3);
    gfx->setCursor(178, ry + 24);
    gfx->print(dexName(d));
  }
  gfx->flush();
}

void render() {
  if (pet.awaitingStarter()) {  // primera partida: elegir inicial (prioridad total)
    renderStarterSelect();
    return;
  }
  if (galleryOpen) {
    renderGallery();
    return;
  }
  if (gameOpen) {
    renderGame();
    return;
  }
  if (sackOpen) {
    renderSack();
    return;
  }
  if (battleOpen) {
    renderBattle();
    return;
  }
  if (kbOpen) {
    renderKeyboard();
    return;
  }
  if (clockOpen) {
    renderClock();
    return;
  }
  if (cardOpen) {
    renderCard();
    return;
  }
  int h = sceneHour();
  gNight = pet.sleeping || h < 6 || h >= 20;
  // drawScene cubre los 466x466 completos: sin fillScreen(NEGRO) previo para
  // que un flush DMA solapado nunca capture negro a medias (anti-parpadeo)
  drawScene(pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome, millis(), gNight);

  if (pet.ceremony) {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    const char *msg = (pet.ceremony == CER_FAREWELL) ? T(S_FAREWELL)
                      : (pet.ceremony == CER_RUNAWAY) ? T(S_RUNAWAY)
                                                      : T(S_GOODBYE);
    drawHeader(dexName(pet.speciesId), d.accent, msg);
    drawCeremony();
    gfx->flush();
    return;
  }

  if (pet.isEgg()) {
    drawHeader(T(S_EGG_HDR), inkColor(), eggMsg());
    int s = 5, x = CX - 16 * s, y = PET_CY - 16 * s;
    drawMap(SPR_EGG, SPRITE_H, x, y, s, false);
    if (pet.eggCracks() >= 1)
      for (auto &c : CRACK1) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggCracks() >= 2)
      for (auto &c : CRACK2) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
    if (pet.eggRarity() >= R_RARO) {
      const char *rar = (pet.eggRarity() == R_LEGENDARIO) ? T(S_EGG_LEGEND) : T(S_EGG_RARE);
      gfx->setTextColor(pet.eggRarity() == R_LEGENDARIO ? UI_BAR_WARN : 0x4C98);
      gfx->setTextSize(2);
      gfx->setCursor(CX - strlen(rar) * 6, 316);
      gfx->print(rar);
    }
    char reg[24];
    snprintf(reg, sizeof(reg), T(S_POKEDEX_FMT), pet.registeredCount());
    gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
    gfx->setTextColor(inkColor());
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(reg) * 6, 348);
    gfx->print(reg);
  } else {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    char name[28];
    const char *base = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
    snprintf(name, sizeof(name), T(S_NAME_FMT), pet.shiny ? "*" : "", base, pet.level());
    drawHeader(name, gNight ? UI_INK_NIGHT : d.accent, statusMsg());
    drawStreakBadge();
    drawPet();
    drawBath();
    drawPoops();
    // panel inferior: base limpia para barras y botones sobre el paisaje
    gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
    drawBars();
    drawButtons();
    drawCelebration();
    drawPetEvent();
    if (gameMenuOpen) drawGameMenu();
    if (pet.wantEvolveButton()) drawEvolveButton();        // CTA rojo: evolucionar
    else if (pet.canRunawayNow()) drawRunawayButton();     // CTA sombrio: escapada (abandono)
    else if (pet.wantFarewellButton()) drawFarewellButton();  // CTA dorado: despedida
  }

  if (pet.sleeping) {
    gfx->setTextColor(UI_INK_NIGHT);
    gfx->setTextSize(3);
    gfx->setCursor(320, 130);
    gfx->print("Zz");
  }

  if (wildPromptUntil) {
    if (millis() > wildPromptUntil) wildPromptUntil = 0;
    else drawWildPrompt();
  }

  // selector de comida
  if (feedMenuUntil) {
    if (millis() > feedMenuUntil) {
      feedMenuUntil = 0;
    } else {
      gfx->fillRoundRect(101, 288, 264, 64, 14, UI_WHITE);
      gfx->drawRoundRect(101, 288, 264, 64, 14, inkColor());
      drawMap(SPR_ICON_FOOD, 16, 110, 296, 3, false);
      drawMap(SPR_ICON_BERRY_B, 16, 176, 296, 3, false);
      drawMap(SPR_ICON_BERRY_G, 16, 242, 296, 3, false);
      drawMap(SPR_ICON_CANDY, 16, 308, 296, 3, false);
    }
  }

  // dialogo "soltar?" (pulsacion larga sobre el bicho)
  if (confirmUntil) {
    if (millis() > confirmUntil) {
      confirmUntil = 0;
    } else {
      gfx->fillRoundRect(94, 168, 278, 152, 16, UI_WHITE);
      gfx->drawRoundRect(94, 168, 278, 152, 16, UI_INK);
      char q[28];
      snprintf(q, sizeof(q), T(S_RELEASE_FMT), dexName(pet.speciesId));
      gfx->setTextColor(UI_INK);
      gfx->setTextSize(2);
      gfx->setCursor(CX - strlen(q) * 6, 196);
      gfx->print(q);
      gfx->fillRoundRect(118, 252, 100, 52, 12, UI_BAR_OK);
      gfx->setTextColor(UI_WHITE);
      gfx->setCursor(118 + (100 - (int)strlen(T(S_YES)) * 12) / 2, 270);
      gfx->print(T(S_YES));
      gfx->fillRoundRect(248, 252, 100, 52, 12, UI_BAR_BAD);
      gfx->setCursor(248 + (100 - (int)strlen(T(S_NO)) * 12) / 2, 270);
      gfx->print(T(S_NO));
    }
  }

  // dialogo de decision (evolucionar/mantener, despedirse/quedaros)
  if (choiceKind) {
    if (millis() > choiceUntil) choiceKind = 0;
    else drawChoiceDialog();
  }

  gfx->flush();
}

// ---------- minijuego: toques con la pokeball ----------

void drawGameMenu() {
  gfx->fillRoundRect(78, 144, 310, 218, 18, UI_WHITE);
  gfx->drawRoundRect(78, 144, 310, 218, 18, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  const char *title = "PLAY";
  gfx->setCursor(CX - strlen(title) * 9, 160);
  gfx->print(title);
  const char *labels[3] = { T(S_GAME_BALL), T(S_GAME_CATCH), T(S_GAME_MEMO) };
  uint16_t cols[3] = { UI_BAR_BAD, UI_BAR_WARN, 0x4C98 };
  for (int i = 0; i < 3; i++) {
    int y = 176 + i * 60;
    gfx->fillRoundRect(88, y, 290, 48, 12, cols[i]);
    gfx->setTextColor(i == 1 ? UI_INK : UI_BG_DAY);
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(labels[i]) * 6, y + 16);
    gfx->print(labels[i]);
  }
}

void startGame() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  gameMenuOpen = false;
  gameOpen = true;
  gameMode = 0;
  gameOverUntil = 0;
  gameScore = 0;
  gameMisses = 0;
  gameNewHi = false;
  gameGain = 0;
  hitTime = 0;
  ballLastHitAt = 0;
  gamePetX = 233;
  respawnBall();
  sfxPlay(SFX_GAME_START);
}

void spawnCatchTarget() {
  catchX = 86 + random(294);
  catchY = 118 + random(206);
  catchIcon = random(3);
  uint32_t life = 980;
  uint32_t speedup = (uint32_t)gameScore * 35;
  if (speedup > 530) speedup = 530;
  life -= speedup;
  catchTargetUntil = millis() + life;
}

void startCatchGame() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  gameMenuOpen = false;
  gameOpen = true;
  gameMode = 1;
  gameOverUntil = 0;
  gameScore = 0;
  gameMisses = 0;
  gameNewHi = false;
  gameGain = 0;
  catchUntil = millis() + 20000;
  spawnCatchTarget();
  sfxPlay(SFX_GAME_START);
}

void startMemoRound() {
  if (memoLen < 14) memoSeq[memoLen++] = random(4);
  memoShow = 0;
  memoInput = 0;
  memoShowing = true;
  memoNextAt = millis() + 350;
}

void startMemoGame() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  gameMenuOpen = false;
  gameOpen = true;
  gameMode = 2;
  gameOverUntil = 0;
  gameScore = 0;
  gameNewHi = false;
  gameGain = 0;
  memoLen = 0;
  memoRounds = 0;
  startMemoRound();
  sfxPlay(SFX_GAME_START);
}

void respawnBall() {
  ballX = 130 + random(206);
  ballY = 96;
  float sp = 2.35f + gameScore * 0.105f;  // exigente desde el inicio
  if (sp > 6.4f) sp = 6.4f;
  ballVX = random(2) ? sp : -sp;
  ballVY = 1.15f;             // empieza cayendo: menos margen para taps gratis
}

int ballHitRadius() {
  if (gameScore >= 20) return 44;
  if (gameScore >= 8) return 50;
  return 58;
}

void gameTap(int16_t x, int16_t y) {
  if (gameMode == 1) {
    catchTap(x, y);
    return;
  }
  if (gameMode == 2) {
    memoTap(x, y);
    return;
  }
  if (gameOverUntil) return;
  if (y < 72) {  // tocar la cabecera = abandonar sin premio
    gameOpen = false;
    sfxPlay(SFX_TAP);
    return;
  }
  float dx = ballX - x, dy = ballY - y;
  int hitRadius = ballHitRadius();
  if (dx * dx + dy * dy < hitRadius * hitRadius) {  // toque a la bola!
    uint32_t now = millis();
    if (now - ballLastHitAt < 240 || ballVY < -0.8f) return;
    gameScore++;
    sfxPlay(SFX_PLAY);
    // impulso moderado; el bloqueo evita encadenar rebotes al inicio
    float lift = 5.85f + (gameScore > 14 ? 3.0f : gameScore * 0.18f);
    ballVY = -lift;
    float drift = 0.22f + (gameScore >= 8 ? 0.04f : 0.0f) + (gameScore >= 18 ? 0.04f : 0.0f);
    ballVX += dx * drift;
    if (ballVX > 9.0f) ballVX = 9.0f;
    if (ballVX < -9.0f) ballVX = -9.0f;
    hitX = ballX;
    hitY = ballY;
    hitTime = now;
    ballLastHitAt = now;
  }
}

void finishCatchGame() {
  gameNewHi = (gameScore > pet.catchHi);
  gameGain = pet.applyCatchResult(gameScore);
  sfxPlay(gameNewHi && gameScore > 0 ? SFX_MEDAL : SFX_LEVEL);
  gameOverUntil = millis() + 4000;
}

void catchTap(int16_t x, int16_t y) {
  if (gameOverUntil) return;
  if (y < 72) { gameOpen = false; sfxPlay(SFX_TAP); return; }
  int dx = x - catchX, dy = y - catchY;
  if (dx * dx + dy * dy <= 52 * 52) {
    gameScore++;
    hitX = catchX;
    hitY = catchY;
    hitTime = millis();
    sfxPlay(SFX_PLAY);
    spawnCatchTarget();
  } else if (++gameMisses >= 3) {
    finishCatchGame();
  } else {
    sfxPlay(SFX_DENY);
  }
}

void finishMemoGame() {
  gameScore = memoRounds;
  gameNewHi = (memoRounds > pet.memoHi);
  gameGain = pet.applyMemoResult(memoRounds);
  sfxPlay(gameNewHi && memoRounds > 0 ? SFX_MEDAL : SFX_LEVEL);
  gameOverUntil = millis() + 4000;
}

int memoPadAt(int16_t x, int16_t y) {
  const int16_t px[4] = { 142, 324, 142, 324 };
  const int16_t py[4] = { 164, 164, 318, 318 };
  for (int i = 0; i < 4; i++) {
    int dx = x - px[i], dy = y - py[i];
    if (dx * dx + dy * dy <= 54 * 54) return i;
  }
  return -1;
}

void memoTap(int16_t x, int16_t y) {
  if (gameOverUntil) return;
  if (y < 72) { gameOpen = false; sfxPlay(SFX_TAP); return; }
  if (memoShowing) return;
  int pad = memoPadAt(x, y);
  if (pad < 0) return;
  if (pad != memoSeq[memoInput]) {
    sfxPlay(SFX_DENY);
    finishMemoGame();
    return;
  }
  sfxPlay(SFX_PLAY);
  memoInput++;
  if (memoInput >= memoLen) {
    memoRounds++;
    if (memoLen >= 14) finishMemoGame();
    else startMemoRound();
  }
}

void stepGame() {
  float grav = 0.82f + gameScore * 0.032f;  // cae exigente desde el inicio
  if (gameScore >= 8) grav += 0.10f;
  if (gameScore >= 18) grav += 0.12f;
  if (grav > 1.65f) grav = 1.65f;
  ballVY += grav;
  ballX += ballVX;
  ballY += ballVY;
  // rebote en la pared circular
  float dx = ballX - CX, dy = ballY - CY;
  float d = sqrtf(dx * dx + dy * dy);
  if (d > 205) {
    float nx = dx / d, ny = dy / d;
    float dot = ballVX * nx + ballVY * ny;
    if (dot > 0) {
      ballVX = (ballVX - 2 * dot * nx) * 0.94f;
      ballVY = (ballVY - 2 * dot * ny) * 0.94f;
      sfxPlay(SFX_BALL_BOUNCE);
    }
    ballX = CX + nx * 205;
    ballY = CY + ny * 205;
  }
  if (ballY > 384) {  // al suelo
    if (++gameMisses >= 3) {
      gameNewHi = (gameScore > pet.gameHi);
      pet.playResult(gameScore);  // actualiza el record y da felicidad
      sfxPlay(gameNewHi && gameScore > 0 ? SFX_MEDAL : SFX_LEVEL);
      gameOverUntil = millis() + 4000;
    } else {
      respawnBall();
      sfxPlay(SFX_BALL_MISS);
    }
  }
  // el bicho la sigue por abajo
  float chase = (ballX - gamePetX) * 0.12f;
  if (chase > 7) chase = 7;
  if (chase < -7) chase = -7;
  gamePetX += chase;
}

// ---------- saco de entrenamiento (entrena la fuerza) ----------

void startSack() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  sackOpen = true;
  sackUntil = millis() + 10000;
  sackOverUntil = 0;
  sackHits = 0;
  sackShake = 0;
  sackNewHi = false;
  sfxPlay(SFX_GAME_START);
}

void sackTap() {
  if (millis() >= sackUntil) return;  // ya termino el tiempo
  sackHits++;
  sackShake = 16;  // sacude el saco
  if ((sackHits & 1) == 1) sfxPlay(SFX_PLAY);
}

void drawGameScene();  // prototipo (definida mas abajo)

void renderSack() {
  uint32_t now = millis();
  drawGameScene();  // fondo del habitat
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  // pantalla de resultado
  if (sackOverUntil) {
    if (now > sackOverUntil) { sackOpen = false; return; }
    char b[20];
    snprintf(b, sizeof(b), T(S_HITS_FMT), sackHits);
    gfx->setTextColor(ink);
    gfx->setTextSize(4);
    gfx->setCursor(CX - strlen(b) * 12, 150);
    gfx->print(b);
    char g[18];
    snprintf(g, sizeof(g), T(S_STR_GAIN_FMT), sackGain);
    gfx->setTextColor(UI_BAR_BAD);
    gfx->setTextSize(3);
    gfx->setCursor(CX - strlen(g) * 9, 210);
    gfx->print(g);
    gfx->setTextSize(2);
    if (sackNewHi && sackHits > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      gfx->setCursor(CX - strlen(T(S_NEW_RECORD)) * 6, 256);
      gfx->print(T(S_NEW_RECORD));
    } else {
      char r[18];
      snprintf(r, sizeof(r), T(S_RECORD_FMT), pet.strHi);
      gfx->setTextColor(ink);
      gfx->setCursor(CX - strlen(r) * 6, 256);
      gfx->print(r);
    }
    gfx->flush();
    return;
  }

  // se acabaron los 10 s: aplicar entrenamiento
  if (now >= sackUntil) {
    sackNewHi = (sackHits > pet.strHi);
    sackGain = pet.trainStrength(sackHits);
    sfxPlay(sackNewHi ? SFX_MEDAL : SFX_PLAY);
    sackOverUntil = now + 3500;
    gfx->flush();
    return;
  }

  // aporreo activo
  sackShake *= 0.84f;
  int off = (int)(sackShake * sinf(now * 0.05f));
  int sx = CX + off, top = 86, sy = 150;
  gfx->fillRect(CX - 3, 56, 6, top - 56, ink);          // gancho/cuerda
  gfx->fillRect(sx - 4, top - 30, 8, 34, ink);          // cadena
  gfx->fillRoundRect(sx - 42, top, 84, 150, 26, C565(0xb5, 0x3a, 0x3a));  // saco
  gfx->fillRoundRect(sx - 42, top, 84, 22, 18, C565(0x7e, 0x28, 0x28));   // tapa
  gfx->drawRoundRect(sx - 42, top, 84, 150, 26, ink);
  gfx->fillRect(sx - 42, top + 70, 84, 4, C565(0x7e, 0x28, 0x28));        // costura

  // contador de golpes
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", sackHits);
  gfx->setTextColor(ink);
  gfx->setTextSize(6);
  gfx->setCursor(CX - strlen(buf) * 18, 268);
  gfx->print(buf);

  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_HIT_FAST)) * 6, 322);
  gfx->print(T(S_HIT_FAST));

  // barra de tiempo
  uint32_t left = sackUntil - now;
  int bw = 280, fw = (int)((uint32_t)bw * left / 10000);
  gfx->fillRoundRect(CX - bw / 2, 350, bw, 16, 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 350, fw, 16, 5, UI_BAR_OK);

  gfx->flush();
}

// fondo del minijuego: hatibat del bicho (cielo por hora + suelo del bioma)
void drawGameScene() {
  int hh = sceneHour();
  bool night = hh < 6 || hh >= 20;
  uint16_t top, bot;
  if (night)       { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (hh < 8) { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }
  else if (hh < 18){ top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }
  else             { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }
  int hor = 376;
  for (int y = 0; y < hor; y += 8)
    gfx->fillRect(0, y, 466, 8, lerp565(top, bot, y, hor));
  if (night)
    for (auto &st : STARS) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  uint8_t bio = pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;
  uint16_t soil = BIOME_SOIL[bio < 6 ? bio : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  gfx->fillRect(0, hor, 466, 466 - hor, soil);
}

void drawGameResult(const char *recordFmt, uint16_t record, StrId gainFmt) {
  drawGameScene();
  if (millis() > gameOverUntil) {
    gameOpen = false;
    return;
  }
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  char buf[22];
  snprintf(buf, sizeof(buf), T(S_SCORE_FMT), gameScore);
  gfx->setTextColor(ink);
  gfx->setTextSize(4);
  gfx->setCursor(CX - strlen(buf) * 12, 148);
  gfx->print(buf);
  char gain[18];
  snprintf(gain, sizeof(gain), T(gainFmt), gameGain);
  gfx->setTextColor(gainFmt == S_DEF_GAIN_FMT ? 0x4C98 : UI_BAR_WARN);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(gain) * 9, 204);
  gfx->print(gain);
  gfx->setTextSize(2);
  if (gameNewHi && gameScore > 0) {
    gfx->setTextColor(UI_BAR_WARN);
    gfx->setCursor(CX - strlen(T(S_NEW_RECORD)) * 6, 256);
    gfx->print(T(S_NEW_RECORD));
  } else {
    char rec[20];
    snprintf(rec, sizeof(rec), recordFmt, record);
    gfx->setTextColor(ink);
    gfx->setCursor(CX - strlen(rec) * 6, 256);
    gfx->print(rec);
  }
  gfx->flush();
}

void renderCatchGame() {
  uint32_t now = millis();
  if (gameOverUntil) {
    drawGameResult(T(S_RECORD_FMT), pet.catchHi, S_SPD_GAIN_FMT);
    return;
  }
  if (now >= catchUntil || gameMisses >= 3) {
    finishCatchGame();
    return;
  }
  if (now > catchTargetUntil) {
    if (++gameMisses >= 3) {
      finishCatchGame();
      return;
    }
    spawnCatchTarget();
  }
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  gfx->setTextColor(ink);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_CATCH_TITLE)) * 9, 32);
  gfx->print(T(S_CATCH_TITLE));
  char score[16], rec[16];
  snprintf(score, sizeof(score), T(S_SCORE_FMT), gameScore);
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.catchHi);
  gfx->setTextSize(2);
  gfx->setCursor(50, 78);
  gfx->print(score);
  gfx->setCursor(294, 78);
  gfx->print(rec);
  for (int i = 0; i < 3; i++) {
    if (i < 3 - gameMisses) gfx->fillCircle(180 + i * 28, 104, 6, UI_BAR_BAD);
    else gfx->drawCircle(180 + i * 28, 104, 6, UI_TRACK);
  }
  const char *const *icon = catchIcon == 0 ? SPR_ICON_FOOD : (catchIcon == 1 ? SPR_ICON_BERRY_B : SPR_ICON_BERRY_G);
  gfx->fillCircle(catchX, catchY, 34, UI_WHITE);
  gfx->drawCircle(catchX, catchY, 36, UI_BAR_WARN);
  drawMap(icon, 16, catchX - 24, catchY - 24, 3, false);
  int bw = 280;
  int fw = (int)((uint32_t)bw * (catchUntil - now) / 20000);
  if (fw < 0) fw = 0;
  gfx->fillRoundRect(CX - bw / 2, 362, bw, 16, 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 362, fw, 16, 5, UI_BAR_OK);
  uint32_t ht = millis() - hitTime;
  if (hitTime && ht < 220) gfx->drawCircle((int)hitX, (int)hitY, 42 + ht / 8, UI_BAR_WARN);
  gfx->flush();
}

void stepMemoGame() {
  if (!memoShowing || millis() < memoNextAt) return;
  sfxPlay(SFX_MEMO_STEP);
  memoShow++;
  if (memoShow >= memoLen) {
    memoShowing = false;
    memoInput = 0;
  } else {
    memoNextAt = millis() + 520;
  }
}

void renderMemoGame() {
  if (gameOverUntil) {
    drawGameResult(T(S_RECORD_FMT), pet.memoHi, S_DEF_GAIN_FMT);
    return;
  }
  stepMemoGame();
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  char roundBuf[18], rec[16];
  snprintf(roundBuf, sizeof(roundBuf), T(S_ROUND_FMT), memoRounds + 1);
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.memoHi);
  gfx->setTextColor(ink);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_GAME_MEMO)) * 9, 34);
  gfx->print(T(S_GAME_MEMO));
  gfx->setTextSize(2);
  gfx->setCursor(60, 82);
  gfx->print(roundBuf);
  gfx->setCursor(310, 82);
  gfx->print(rec);
  const int16_t px[4] = { 142, 324, 142, 324 };
  const int16_t py[4] = { 164, 164, 318, 318 };
  const uint16_t col[4] = { UI_BAR_BAD, UI_BAR_WARN, 0x4C98, UI_BAR_OK };
  int active = memoShowing ? memoSeq[memoShow] : -1;
  for (int i = 0; i < 4; i++) {
    uint16_t fill = i == active ? UI_WHITE : col[i];
    gfx->fillCircle(px[i], py[i], 48, fill);
    gfx->drawCircle(px[i], py[i], 52, ink);
  }
  gfx->flush();
}

void renderGame() {
  // sin fillScreen(NEGRO): drawGameScene cubre los 466x466 completos. Si el
  // DMA del flush anterior aun lee el buffer, vera contenido valido (no negro
  // a medio pintar), que era el parpadeo a 25 fps.
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  if (gameMode == 1) {
    renderCatchGame();
    return;
  }
  if (gameMode == 2) {
    renderMemoGame();
    return;
  }

  if (gameOverUntil) {
    drawGameScene();
    if (millis() > gameOverUntil) {
      gameOpen = false;
      return;
    }
    char buf[22];
    snprintf(buf, sizeof(buf), T(S_SCORE_FMT), gameScore);
    gfx->setTextColor(ink);
    gfx->setTextSize(4);
    gfx->setCursor(CX - strlen(buf) * 12, 160);
    gfx->print(buf);
    gfx->setTextSize(2);
    if (gameNewHi && gameScore > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      gfx->setCursor(CX - strlen(T(S_NEW_RECORD)) * 6, 214);
      gfx->print(T(S_NEW_RECORD));
    } else {
      char rec[20];
      snprintf(rec, sizeof(rec), T(S_RECORD_FMT), pet.gameHi);
      gfx->setTextColor(ink);
      gfx->setCursor(CX - strlen(rec) * 6, 214);
      gfx->print(rec);
    }
    const char *msg = gameScore >= 10 ? T(S_GREAT_JOY) : T(S_PLUS_JOY);
    gfx->setTextColor(ink);
    gfx->setCursor(CX - strlen(msg) * 6, 250);
    gfx->print(msg);
    gfx->flush();
    return;
  }

  drawGameScene();
  stepGame();

  // marcador, record y vidas
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", gameScore);
  gfx->setTextColor(ink);
  gfx->setTextSize(4);
  gfx->setCursor(CX - strlen(buf) * 12, 30);
  gfx->print(buf);
  char rec[12];
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.gameHi);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(rec) * 6, 76);
  gfx->print(rec);
  for (int i = 0; i < 3; i++) {
    if (i < 3 - gameMisses) gfx->fillCircle(180 + i * 28, 104, 6, UI_BAR_BAD);
    else gfx->drawCircle(180 + i * 28, 104, 6, UI_TRACK);
  }

  if (pmd.loaded) {
    uint8_t act = (ballX > gamePetX + 4) ? PMD_WALKR : (ballX < gamePetX - 4) ? PMD_WALKL : PMD_IDLE;
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdAct(act, (int)gamePetX, 394, millis(), true, false, 3);
  } else if (mon.loaded) {
    int s = (mon.h * 2 > 130) ? 1 : 2;
    int w = mon.w * s, h = mon.h * s;
    uint16_t fm = mon.frameMs ? mon.frameMs : 100;
    uint16_t fi = (millis() / fm) % mon.frames;
    const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
    int px = (int)gamePetX - w / 2, py = 394 - h;
    for (int r = 0; r < mon.h; r++)
      for (int c = 0; c < mon.w; c++) {
        uint8_t idx = fr[r * mon.w + c];
        if (idx == 0xFF) continue;
        gfx->fillRect(px + c * s, py + r * s, s, s, mon.pal[idx]);
      }
  }

  // anillo de impacto que se expande y desvanece (feedback suave del golpe)
  uint32_t ht = millis() - hitTime;
  if (hitTime && ht < 260) {
    int rad = 22 + (int)(ht / 6);
    gfx->drawCircle((int)hitX, (int)hitY, rad, C565(0xff, 0xe7, 0x9f));
    gfx->drawCircle((int)hitX, (int)hitY, rad - 2, C565(0xff, 0xd9, 0x8a));
  }

  // la pokeball
  drawMap(SPR_ICON_PLAY, 16, (int)ballX - 24, (int)ballY - 24, 3, false);

  gfx->flush();
}

// ---------- combate salvaje manual ----------

uint16_t battleHpFor(const BattleStats &stats) {
  return stats.hp ? stats.hp : (uint16_t)(30 + stats.level * 5 + stats.def);
}

BattleStats petBattleStats() {
  BattleStats stats = {};
  stats.level = pet.level();
  stats.atk = pet.atkStat();
  stats.def = pet.defStat();
  stats.spe = pet.speStat();
  stats.hp = 0;
  if (!pet.isEgg() && pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    stats.type1 = DEX_TBL[pet.speciesId].type1;
    stats.type2 = DEX_TBL[pet.speciesId].type2;
  }
  return stats;
}

void scheduleNextWild(uint32_t now) {
  nextWildEligible = now + WILD_COOLDOWN_MS;
}

bool mainScreenReadyForWild() {
  if (screenOff || pet.awaitingStarter() || pet.isEgg() || pet.sleeping || pet.ceremony) return false;
  if (battleOpen || gameOpen || gameMenuOpen || sackOpen || cardOpen || galleryOpen || kbOpen || clockOpen) return false;
  if (feedMenuUntil || confirmUntil || choiceKind || bathUntil || petEventUntil) return false;
  if (pet.evolving() || pet.wantEvolveButton() || pet.canRunawayNow() || pet.wantFarewellButton()) return false;
  return true;
}

void maybeOfferWildEncounter(uint32_t now) {
  if (nextWildEligible == 0) {
    scheduleNextWild(now);
    return;
  }
  if (wildPromptUntil) return;
  if (now - lastWildCheck < WILD_CHECK_MS) return;
  lastWildCheck = now;
  if (now < nextWildEligible) return;
  if (!mainScreenReadyForWild()) return;
  int16_t cand = pickWildSpecies((uint8_t)random(100));
  uint8_t phase = currentDayPhase();
  uint8_t chance = (phase == 3) ? 4 : (phase == 0 ? 7 : 8);
  if (phase == 3 && cand >= 1 && cand <= DEX_COUNT) {
    const DexEntry &d = DEX_TBL[cand];
    if (d.type1 == TYPE_GHOST || d.type2 == TYPE_GHOST ||
        d.type1 == TYPE_POISON || d.type2 == TYPE_POISON) chance = 8;
  }
  if ((uint8_t)random(100) >= chance) return;

  wildPromptDex = cand;
  wildPromptLevel = wildLevelFor(pet.level(), (uint8_t)random(100));
  wildPromptUntil = now + WILD_PROMPT_MS;
  scheduleNextWild(now);
  sfxPlay(SFX_MENU);
}

void scheduleNextPetEvent(uint32_t now) {
  nextPetEventEligible = now + PET_EVENT_COOLDOWN_MS;
}

bool mainScreenReadyForPetEvent() {
  if (screenOff || pet.awaitingStarter() || pet.isEgg() || pet.sleeping || pet.ceremony) return false;
  if (battleOpen || gameOpen || gameMenuOpen || sackOpen || cardOpen || galleryOpen || kbOpen || clockOpen) return false;
  if (feedMenuUntil || confirmUntil || choiceKind || bathUntil || wildPromptUntil) return false;
  if (pet.evolving() || pet.wantEvolveButton() || pet.canRunawayNow() || pet.wantFarewellButton()) return false;
  return true;
}

void maybeOfferPetEvent(uint32_t now) {
  if (nextPetEventEligible == 0) {
    scheduleNextPetEvent(now);
    return;
  }
  if (petEventUntil) {
    if (now > petEventUntil) petEventUntil = 0;
    return;
  }
  if (now - lastPetEventCheck < PET_EVENT_CHECK_MS) return;
  lastPetEventCheck = now;
  if (now < nextPetEventEligible) return;
  if (!mainScreenReadyForPetEvent()) return;
  uint8_t phase = currentDayPhase();
  uint8_t chance = (phase == 0) ? 14 : (phase == 3 ? 8 : 10);
  if ((uint8_t)random(100) >= chance) return;

  petEventType = (uint8_t)random(3);
  petEventUntil = now + PET_EVENT_PROMPT_MS;
  scheduleNextPetEvent(now);
  sfxPlay(SFX_EVENT_SPARKLE);
}

bool inPetEventHit(int16_t x, int16_t y) {
  int16_t ex = 366, ey = 286;
  int dx = x - ex, dy = y - ey;
  return dx * dx + dy * dy <= 44 * 44;
}

void acceptPetEvent() {
  uint8_t type = petEventType;
  petEventUntil = 0;
  scheduleNextPetEvent(millis());
  if (!pet.applyPetEvent(type)) return;
  StrId msg = S_EVENT_FOUND;
  if (type == PET_EVENT_HEART) msg = S_EVENT_PET;
  else if (type == PET_EVENT_SPARKLE) msg = S_EVENT_LUCKY;
  snprintf(petEventMsg, sizeof(petEventMsg), "%s", T(msg));
  petEventFeedbackUntil = millis() + 1800;
  sfxPlay(type == PET_EVENT_BERRY ? SFX_EAT : (type == PET_EVENT_SPARKLE ? SFX_EVENT_SPARKLE : SFX_HEART));
}

void closeBattle() {
  battleOpen = false;
  battleResolved = false;
  battleAttackMenuUntil = 0;
  battleCatchOffered = false;
  battleCatchTried = false;
  battleCatchDone = false;
  battleCatchSuccess = false;
  battleRespectCatch = false;
  battleCatchChance = 0;
  wildPmd.unload();
}

void startBattleWith(int16_t forcedDex, uint8_t forcedLevel) {
  if (!canStartWildBattle(pet.isEgg(), pet.sleeping, pet.ceremony)) return;
  wildPromptUntil = 0;
  scheduleNextWild(millis());
  if (forcedDex >= 1 && forcedDex <= DEX_COUNT) {
    battleDex = forcedDex;
    battleLevel = forcedLevel ? forcedLevel : wildLevelFor(pet.level(), (uint8_t)random(100));
  } else {
    uint8_t speciesRoll = random(100);
    uint8_t levelRoll = random(100);
    battleDex = pickWildSpecies(speciesRoll);
    battleLevel = wildLevelFor(pet.level(), levelRoll);
  }
  battlePlayer = petBattleStats();
  battleEnemy = wildBattleStats(battleDex, battleLevel);
  battleEnemy.hp = 0;
  battleResult = {};
  battleRun = beginBattleRuntime(battlePlayer, battleEnemy);
  battleTurn = {};
  battleReward = {};
  battleMsg[0] = 0;
  battleAttackMenuUntil = 0;
  battleCatchOffered = false;
  battleCatchTried = false;
  battleCatchDone = false;
  battleCatchSuccess = false;
  battleRespectCatch = false;
  battleCatchChance = 0;
  battleResolved = false;
  battleOpen = true;
  wildPmd.unload();
  wildPmd.load(battleDex, false);
  sfxPlay(SFX_TAP);
}

void startBattle() {
  startBattleWith(0, 0);
}

void finishBattle() {
  if (battleResolved) return;
  battleResolved = true;
  if (battleTurn.playerWon) {
    bool closeWin = battleRun.playerHp <= battleRun.playerMaxHp / 3;
    battleReward = pet.applyBattleWin(battleDex, closeWin);
    battleCatchOffered = true;
    battleRespectCatch = false;
    battleCatchChance = pet.catchChanceForWild(battleDex, battleLevel, battlePlayer.level, closeWin);
    sfxPlay(SFX_BATTLE_WIN);
  } else {
    battleReward = {};
    pet.applyBattleLoss();
    bool closeLoss = battleRun.enemyHp > 0 && battleRun.enemyHp * 100UL <= battleRun.enemyMaxHp * 30UL;
    battleCatchChance = closeLoss ? pet.respectCatchChanceForWild(battleDex, battleLevel, battlePlayer.level) : 0;
    battleCatchOffered = battleCatchChance > 0;
    battleRespectCatch = battleCatchOffered;
    sfxPlay(SFX_BATTLE_LOSS);
  }
}

void performBattleAction(BattleAction action) {
  if (battleResolved) return;
  battleAttackMenuUntil = 0;
  battleLastAction = action;
  battleTurn = stepBattle(battleRun, action, (uint8_t)random(100));
  if (battleTurn.restFailed) {
    snprintf(battleMsg, sizeof(battleMsg), T(S_NO_REST));
  } else if (battleTurn.counterReady) {
    snprintf(battleMsg, sizeof(battleMsg), T(S_COUNTER_READY));
  } else if (battleTurn.playerRested) {
    snprintf(battleMsg, sizeof(battleMsg), T(S_RESTED_FMT), battleTurn.playerHeal);
  } else if (battleTurn.playerDamage > 0) {
    if (battleTurn.playerTypePct > 100) snprintf(battleMsg, sizeof(battleMsg), "%s %u", T(S_EFFECTIVE), battleTurn.playerDamage);
    else if (battleTurn.playerTypePct < 100) snprintf(battleMsg, sizeof(battleMsg), "%s %u", T(S_NOT_EFFECTIVE), battleTurn.playerDamage);
    else snprintf(battleMsg, sizeof(battleMsg), T(S_HIT_FMT), battleTurn.playerDamage);
  } else if (battleTurn.enemyDodged) {
    snprintf(battleMsg, sizeof(battleMsg), T(S_ENEMY_DODGED));
  } else if (battleTurn.playerDodged) {
    snprintf(battleMsg, sizeof(battleMsg), T(S_DODGED));
  } else {
    snprintf(battleMsg, sizeof(battleMsg), T(S_MISSED));
  }
  if (battleTurn.battleEnded) {
    finishBattle();
    return;
  }
  if (battleTurn.restFailed) sfxPlay(SFX_DENY);
  else if (battleTurn.counterReady) sfxPlay(SFX_COUNTER);
  else if (battleTurn.playerRested) sfxPlay(SFX_REST);
  else sfxPlay(battleTurn.playerDamage > 0 ? SFX_PLAY : SFX_TAP);
}

void battleTap(int16_t x, int16_t y) {
  if (battleResolved) {
    if (battleCatchOffered && !battleCatchDone) {
      if (x >= 76 && x <= 224 && y >= 392 && y <= 448) {
        bool closeWin = battleRun.playerHp <= battleRun.playerMaxHp / 3;
        battleCatchTried = true;
        battleCatchDone = true;
        if (battleRespectCatch) {
          battleCatchSuccess = pet.tryRespectCatchWild(battleDex, battleLevel, battlePlayer.level, (uint8_t)random(100));
          battleCatchChance = pet.respectCatchChanceForWild(battleDex, battleLevel, battlePlayer.level);
        } else {
          battleCatchSuccess = pet.tryCatchWild(battleDex, battleLevel, battlePlayer.level, closeWin, (uint8_t)random(100));
          battleCatchChance = pet.catchChanceForWild(battleDex, battleLevel, battlePlayer.level, closeWin);
        }
        sfxPlay(battleCatchSuccess ? SFX_CATCH_OK : SFX_CATCH_FAIL);
        galleryDirty = true;
        return;
      }
      if (x >= 242 && x <= 390 && y >= 392 && y <= 448) {
        battleCatchDone = true;
        battleCatchTried = false;
        sfxPlay(SFX_TAP);
        return;
      }
      return;
    }
    if (x >= 118 && x <= 348 && y >= 392 && y <= 454) closeBattle();
    return;
  }
  if (battleAttackMenuUntil) {
    if (x >= 66 && x <= 232 && y >= 292 && y <= 352) {
      performBattleAction(BATTLE_ATTACK_QUICK);
      return;
    }
    if (x >= 234 && x <= 400 && y >= 292 && y <= 352) {
      performBattleAction(BATTLE_ATTACK_HEAVY);
      return;
    }
    battleAttackMenuUntil = 0;
    sfxPlay(SFX_TAP);
    return;
  }
  if (x >= 184 && x <= 282 && y >= 100 && y <= 136) {
    closeBattle();
    sfxPlay(SFX_TAP);
    return;
  }
  if (x >= 46 && x <= 174 && y >= 344 && y <= 428) {
    battleAttackMenuUntil = 1;
    sfxPlay(SFX_TAP);
  } else if (x >= 169 && x <= 297 && y >= 344 && y <= 428) {
    performBattleAction(BATTLE_DODGE);
  } else if (x >= 292 && x <= 420 && y >= 344 && y <= 428) {
    performBattleAction(BATTLE_REST);
  }
}

void drawBattleHpBar(int x, int y, uint16_t cur, uint16_t maxHp, uint16_t color) {
  if (maxHp == 0) maxHp = 1;
  int w = 150;
  int fw = (int)((uint32_t)cur * w / maxHp);
  if (fw > w) fw = w;
  gfx->fillRoundRect(x, y, w, 14, 4, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(x, y, fw, 14, 4, color);
}

void battleRewardText(char *buf, size_t len) {
  if (battleReward.amount == 0) { buf[0] = 0; return; }
  StrId fmt = S_SPD_GAIN_FMT;
  if (battleReward.stat == BATTLE_REWARD_ATK) fmt = S_ATK_GAIN_FMT;
  else if (battleReward.stat == BATTLE_REWARD_DEF) fmt = S_DEF_GAIN_FMT;
  snprintf(buf, len, T(fmt), battleReward.amount);
}

void drawBattleButtonLabel(int x, int y, int w, const char *label) {
  int px = (int)strlen(label) * 12;
  if (px <= w - 8) {
    gfx->setTextSize(2);
    gfx->setCursor(x + (w - px) / 2, y);
    gfx->print(label);
  } else {
    gfx->setTextSize(1);
    gfx->setCursor(x + (w - (int)strlen(label) * 6) / 2, y + 3);
    gfx->print(label);
    gfx->setTextSize(2);
  }
}

const char *battleTypeName(uint8_t type) {
  switch (type) {
    case TYPE_NORMAL: return "NORMAL";
    case TYPE_FIRE: return "FIRE";
    case TYPE_WATER: return "WATER";
    case TYPE_ELECTRIC: return "ELEC";
    case TYPE_GRASS: return "GRASS";
    case TYPE_ICE: return "ICE";
    case TYPE_FIGHTING: return "FIGHT";
    case TYPE_POISON: return "POISON";
    case TYPE_GROUND: return "GROUND";
    case TYPE_FLYING: return "FLY";
    case TYPE_PSYCHIC: return "PSY";
    case TYPE_BUG: return "BUG";
    case TYPE_ROCK: return "ROCK";
    case TYPE_GHOST: return "GHOST";
    case TYPE_DRAGON: return "DRAGON";
    case TYPE_DARK: return "DARK";
    case TYPE_STEEL: return "STEEL";
    case TYPE_FAIRY: return "FAIRY";
  }
  return "";
}

uint16_t battleTypeColor(uint8_t type) {
  switch (type) {
    case TYPE_FIRE: return 0xEA87;
    case TYPE_WATER: return 0x4C98;
    case TYPE_ELECTRIC: return 0xBCA1;
    case TYPE_GRASS: return 0x3C49;
    case TYPE_ICE: return 0x5D99;
    case TYPE_FIGHTING: return 0xA2A5;
    case TYPE_POISON: return 0x8A73;
    case TYPE_GROUND: return 0xB447;
    case TYPE_FLYING: return 0x8D7F;
    case TYPE_PSYCHIC: return 0xD28F;
    case TYPE_BUG: return 0x7CC4;
    case TYPE_ROCK: return 0x9407;
    case TYPE_GHOST: return 0x6B33;
    case TYPE_DRAGON: return 0x5A5F;
    case TYPE_DARK: return 0x5ACB;
    case TYPE_STEEL: return 0xA534;
    case TYPE_FAIRY: return 0xF3B7;
    default: return 0x8C4D;
  }
}

void typeText(char *buf, size_t len, const DexEntry &d) {
  if (d.type2 == TYPE_NONE) snprintf(buf, len, "%s", battleTypeName(d.type1));
  else snprintf(buf, len, "%s %s", battleTypeName(d.type1), battleTypeName(d.type2));
}

int typeChipWidth(uint8_t type) {
  return (int)strlen(battleTypeName(type)) * 6 + 14;
}

void drawTypeChip(int x, int y, uint8_t type) {
  if (type == TYPE_NONE) return;
  const char *label = battleTypeName(type);
  int w = typeChipWidth(type);
  gfx->fillRoundRect(x, y, w, 16, 5, lerp565(battleTypeColor(type), UI_WHITE, 5, 8));
  gfx->drawRoundRect(x, y, w, 16, 5, UI_INK);
  gfx->setTextSize(1);
  gfx->setTextColor(UI_INK);
  gfx->setCursor(x + 7, y + 5);
  gfx->print(label);
}

void drawTypeChips(int x, int y, const DexEntry &d, bool alignRight) {
  int w1 = typeChipWidth(d.type1);
  int w2 = d.type2 == TYPE_NONE ? 0 : typeChipWidth(d.type2);
  int total = w1 + (w2 ? 4 + w2 : 0);
  int sx = alignRight ? x - total : x;
  drawTypeChip(sx, y, d.type1);
  if (d.type2 != TYPE_NONE) drawTypeChip(sx + w1 + 4, y, d.type2);
  gfx->setTextSize(2);
}

void drawWildPrompt() {
  gfx->fillRoundRect(82, 156, 302, 178, 18, UI_WHITE);
  gfx->drawRoundRect(82, 156, 302, 178, 18, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_WILD_Q)) * 9, 176);
  gfx->print(T(S_WILD_Q));
  char name[28];
  snprintf(name, sizeof(name), "%s Lv.%u", dexName(wildPromptDex), wildPromptLevel);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(name) * 6, 206);
  gfx->print(name);

  gfx->fillRoundRect(93, 226, 280, 44, 12, UI_BAR_BAD);
  gfx->fillRoundRect(93, 278, 280, 44, 12, UI_TRACK);
  gfx->setTextColor(UI_WHITE);
  gfx->setCursor(CX - strlen(T(S_FIGHT)) * 6, 240);
  gfx->print(T(S_FIGHT));
  gfx->setTextColor(UI_BG_DAY);
  gfx->setCursor(CX - strlen(T(S_LATER)) * 6, 292);
  gfx->print(T(S_LATER));
}

void drawPetEvent() {
  uint32_t now = millis();
  if (petEventUntil && now > petEventUntil) petEventUntil = 0;
  if (!petEventUntil && (!petEventFeedbackUntil || now > petEventFeedbackUntil)) return;

  int16_t ex = 366, ey = 286;
  if (petEventUntil) {
    gfx->fillCircle(ex, ey, 32, UI_WHITE);
    gfx->drawCircle(ex, ey, 34, UI_BAR_WARN);
    if (petEventType == PET_EVENT_BERRY) {
      drawMap(SPR_ICON_BERRY_G, 16, ex - 24, ey - 24, 3, false);
    } else if (petEventType == PET_EVENT_HEART) {
      drawMap(SPR_HEART, 32, ex - 32, ey - 32, 2, false);
    } else {
      gfx->setTextColor(UI_BAR_WARN);
      gfx->setTextSize(4);
      gfx->setCursor(ex - 12, ey - 18);
      gfx->print("*");
      gfx->fillCircle(ex - 14, ey + 12, 4, UI_WHITE);
      gfx->fillCircle(ex + 16, ey + 10, 4, UI_WHITE);
    }
  }
  if (petEventFeedbackUntil && now <= petEventFeedbackUntil) {
    gfx->setTextColor(UI_BAR_WARN);
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(petEventMsg) * 6, 292);
    gfx->print(petEventMsg);
  }
}

void renderBattle() {
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  const DexEntry &mine = DEX_TBL[pet.speciesId];
  const DexEntry &wild = DEX_TBL[battleDex];

  gfx->setTextColor(ink);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_WILD_BATTLE)) * 9, 34);
  gfx->print(T(S_WILD_BATTLE));

  char left[24], right[24];
  snprintf(left, sizeof(left), "%s Lv.%u", pet.nick[0] ? pet.nick : dexName(pet.speciesId), battlePlayer.level);
  snprintf(right, sizeof(right), "%s Lv.%u", dexName(battleDex), battleLevel);
  gfx->setTextSize(2);
  gfx->setCursor(28, 82);
  gfx->print(left);
  int rightLen = strlen(right);
  int rightTextSize = rightLen <= 12 ? 2 : 1;
  int rightTextW = rightLen * (rightTextSize == 2 ? 12 : 6);
  int rightX = 408 - rightTextW;
  if (rightX < 246) rightX = 246;
  gfx->setTextSize(rightTextSize);
  gfx->setCursor(rightX, rightTextSize == 2 ? 82 : 88);
  gfx->print(right);
  gfx->setTextSize(2);

  uint16_t playerMax = battleRun.playerMaxHp;
  uint16_t enemyMax = battleRun.enemyMaxHp;
  uint16_t playerCur = battleRun.playerHp;
  uint16_t enemyCur = battleRun.enemyHp;
  drawBattleHpBar(28, 110, playerCur, playerMax, UI_BAR_OK);
  drawBattleHpBar(288, 110, enemyCur, enemyMax, UI_BAR_BAD);
  drawTypeChips(28, 130, mine, false);
  drawTypeChips(438, 130, wild, true);

  if (!battleResolved) {
    gfx->fillRoundRect(188, 102, 90, 32, 9, UI_TRACK);
    gfx->setTextColor(UI_BG_DAY);
    gfx->setTextSize(2);
    gfx->setCursor(188 + (90 - (int)strlen(T(S_RUN_BATTLE)) * 12) / 2, 111);
    gfx->print(T(S_RUN_BATTLE));
  }

  if (pmd.loaded) drawPmdAct(PMD_IDLE, 142, 286, millis(), true, false, 3);
  else {
    const uint8_t *th = thumbs.get(pet.speciesId);
    if (th) drawThumb(th, 94, 166, 3, false);
  }
  if (wildPmd.loaded) drawPmdActM(wildPmd, PMD_IDLE, 328, 286, millis(), true, false, 3);
  else {
    const uint8_t *th = thumbs.get(battleDex);
    if (th) drawThumb(th, 280, 166, 3, false);
  }

  if (battleResolved) {
    const char *res = battleTurn.playerWon ? T(S_WIN) : T(S_LOSS);
    gfx->setTextColor(battleTurn.playerWon ? UI_BAR_OK : UI_BAR_BAD);
    gfx->setTextSize(4);
    gfx->setCursor(CX - strlen(res) * 12, 300);
    gfx->print(res);
    char rounds[20], damage[28];
    snprintf(rounds, sizeof(rounds), T(S_ROUNDS_FMT), battleRun.round);
    snprintf(damage, sizeof(damage), T(S_DAMAGE_FMT), battleRun.playerDamageTotal, battleRun.enemyDamageTotal);
    gfx->setTextColor(ink);
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(rounds) * 6, 334);
    gfx->print(rounds);
    gfx->setCursor(CX - strlen(damage) * 6, 356);
    gfx->print(damage);
    if (battleTurn.playerWon) {
      char reward[20];
      battleRewardText(reward, sizeof(reward));
      if (reward[0]) {
        gfx->setTextColor(UI_BAR_WARN);
        gfx->setCursor(CX - strlen(reward) * 6, 378);
        gfx->print(reward);
      }
    } else if (battleRespectCatch && battleCatchOffered && !battleCatchDone) {
      gfx->setTextColor(UI_BAR_WARN);
      gfx->setTextSize(2);
      gfx->setCursor(CX - strlen(T(S_CLOSE_CHANCE)) * 6, 378);
      gfx->print(T(S_CLOSE_CHANCE));
    }
    if (battleCatchOffered && !battleCatchDone) {
      gfx->fillRoundRect(76, 396, 148, 52, 14, UI_BAR_OK);
      gfx->fillRoundRect(242, 396, 148, 52, 14, UI_TRACK);
      gfx->setTextColor(UI_BG_DAY);
      drawBattleButtonLabel(76, 414, 148, T(S_CATCH_WILD));
      drawBattleButtonLabel(242, 414, 148, T(S_LEAVE_WILD));
    } else {
      if (battleCatchDone && battleCatchTried) {
        const char *catchMsg = battleCatchSuccess ? T(S_CAUGHT_OK) : T(S_ESCAPED);
        gfx->setTextColor(battleCatchSuccess ? UI_BAR_OK : UI_BAR_BAD);
        gfx->setTextSize(2);
        gfx->setCursor(CX - strlen(catchMsg) * 6, 378);
        gfx->print(catchMsg);
      }
      gfx->fillRoundRect(118, 396, 230, 52, 14, UI_BAR_OK);
      gfx->setTextColor(UI_BG_DAY);
      gfx->setTextSize(3);
      gfx->setCursor(CX - strlen(T(S_OK)) * 9, 413);
      gfx->print(T(S_OK));
    }
  } else {
    char roundBuf[14];
    snprintf(roundBuf, sizeof(roundBuf), "R%u", battleRun.round + 1);
    gfx->setTextColor(ink);
    gfx->setTextSize(2);
    gfx->setCursor(32, 318);
    gfx->print(roundBuf);
    if (battleMsg[0]) {
      gfx->setCursor(CX - strlen(battleMsg) * 6, 318);
      gfx->print(battleMsg);
      if (battleTurn.enemyDamage > 0) {
        char eb[16];
        snprintf(eb, sizeof(eb), "-%u", battleTurn.enemyDamage);
        gfx->setTextColor(UI_BAR_BAD);
        gfx->setCursor(CX - strlen(eb) * 6, 340);
        gfx->print(eb);
      }
    }

    if (battleAttackMenuUntil) {
      gfx->fillRoundRect(74, 298, 150, 46, 12, UI_BAR_BAD);
      gfx->fillRoundRect(242, 298, 150, 46, 12, UI_BAR_WARN);
      gfx->setTextColor(UI_BG_DAY);
      drawBattleButtonLabel(74, 312, 150, T(S_QUICK_ATTACK));
      drawBattleButtonLabel(242, 312, 150, T(S_HEAVY_ATTACK));
    }

    gfx->fillRoundRect(58, 358, 108, 58, 13, UI_BAR_BAD);
    gfx->fillRoundRect(179, 358, 108, 58, 13, 0x4C98);
    gfx->fillRoundRect(300, 358, 108, 58, 13, UI_BAR_OK);
    gfx->setTextColor(UI_BG_DAY);
    drawBattleButtonLabel(58, 380, 108, T(S_ATTACK));
    drawBattleButtonLabel(179, 380, 108, T(S_DODGE));
    char restLabel[18];
    snprintf(restLabel, sizeof(restLabel), "%s %u", T(S_REST), battleRun.restUsesLeft);
    drawBattleButtonLabel(300, 380, 108, restLabel);
  }

  gfx->flush();
}

// ---------- ficha del bicho (deslizar vertical) ----------

void drawCardStat(int y, const char *label, uint16_t val, uint16_t maxBar, uint16_t color) {
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(96, y);
  gfx->print(label);
  char num[8];
  snprintf(num, sizeof(num), "%u", val);
  gfx->setCursor(330, y);
  gfx->print(num);
  int bw = 160;
  int fw = (int)val * bw / maxBar;
  if (fw > bw) fw = bw;
  gfx->fillRoundRect(150, y + 2, bw, 11, 3, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(150, y + 2, fw, 11, 3, color);
}

// ---------- ajuste de hora en pantalla (deslizar abajo) ----------
// El usuario pone su hora LOCAL a ojo; el firmware la usa tal cual, asi que
// no hay que gestionar zona horaria. Preserva el dia (no rompe racha/edad).

void openClock() {
  uint32_t e = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  clockH = (e / 3600) % 24;
  clockM = (e / 60) % 60;
  clockOpen = true;
  sfxPlay(SFX_MENU);
}

void applyClock() {
  uint32_t base = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  uint32_t e = (base / 86400) * 86400 + (uint32_t)clockH * 3600 + (uint32_t)clockM * 60;
  rtcSetEpoch(e);
  pet.setClock(e);
  clockOpen = false;
}

void drawClockBtn(int x, int y, const char *l) {
  gfx->fillRoundRect(x, y, 58, 58, 12, UI_WHITE);
  gfx->drawRoundRect(x, y, 58, 58, 12, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(4);
  gfx->setCursor(x + 17, y + 15);
  gfx->print(l);
}

// pildoras de idioma centradas en y; rellena la activa
#define LANG_PILL_Y 296
#define LANG_PILL_H 30
#define LANG_PILL_X 336          // pildora de idioma (cicla los 6 al tocar)
#define LANG_PILL_W 96
#define SOUND_PILL_X 24
#define SOUND_PILL_W 116

const char *soundModeLabel() {
  switch (audioMode()) {
    case SOUND_FULL: return T(S_SND_FULL);
    case SOUND_MED: return T(S_SND_MED);
    case SOUND_LOW: return T(S_SND_LOW);
    default: return T(S_SND_OFF);
  }
}

uint8_t nextSoundMode() {
  switch (audioMode()) {
    case SOUND_FULL: return SOUND_MED;
    case SOUND_MED: return SOUND_LOW;
    case SOUND_LOW: return SOUND_OFF;
    default: return SOUND_FULL;
  }
}
static const char *const LANG_CODES[LANG_COUNT] = { "ES", "EN", "FR", "DE", "IT", "PT" };

void drawStatusLine(int y, const char *label, const char *value, uint16_t valueColor) {
  gfx->setTextSize(1);
  gfx->setTextColor(UI_TRACK);
  gfx->setCursor(94, y);
  gfx->print(label);
  gfx->setTextColor(valueColor);
  gfx->setCursor(172, y);
  gfx->print(value);
}

void renderClock() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_SET_TIME)) * 9, 44);
  gfx->print(T(S_SET_TIME));

  char t[8];
  snprintf(t, sizeof(t), "%02d:%02d", clockH, clockM);
  gfx->setTextSize(7);
  gfx->setCursor(CX - 105, 108);
  gfx->print(t);

  drawClockBtn(104, 190, "-");  // hora -
  drawClockBtn(170, 190, "+");  // hora +
  drawClockBtn(252, 190, "-");  // min -
  drawClockBtn(318, 190, "+");  // min +
  gfx->setTextSize(2);
  gfx->setTextColor(UI_TRACK);
  gfx->setCursor(120, 256);
  gfx->print(T(S_HOUR));
  gfx->setCursor(276, 256);
  gfx->print(T(S_MIN));

  // selector de sonido: mucho / medio / poco / apagado
  uint8_t sndMode = audioMode();
  const char *sl = soundModeLabel();
  gfx->fillRoundRect(SOUND_PILL_X, LANG_PILL_Y, SOUND_PILL_W, LANG_PILL_H, 8, sndMode ? UI_BAR_OK : UI_WHITE);
  gfx->drawRoundRect(SOUND_PILL_X, LANG_PILL_Y, SOUND_PILL_W, LANG_PILL_H, 8, UI_INK);
  gfx->setTextColor(sndMode ? UI_BG_DAY : UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(SOUND_PILL_X + (SOUND_PILL_W - (int)strlen(sl) * 12) / 2, LANG_PILL_Y + 8);
  gfx->print(sl);

  // selector de idioma: una pildora que cicla los 6 idiomas al tocar
  gfx->fillRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_WHITE);
  gfx->drawRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_INK);
  char lp[10];
  snprintf(lp, sizeof(lp), "%s >", LANG_CODES[gLang]);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(LANG_PILL_X + (LANG_PILL_W - (int)strlen(lp) * 12) / 2, LANG_PILL_Y + 8);
  gfx->print(lp);

  char fwLine[34];
  snprintf(fwLine, sizeof(fwLine), "v%s", FW_VERSION);
  drawStatusLine(334, "FW", fwLine, UI_INK);
  drawStatusLine(348, "SAVE", pet.saveLoadedFromNvs ? "OK" : "NEW", pet.saveLoadedFromNvs ? UI_BAR_OK : UI_BAR_WARN);
  drawStatusLine(362, "SD", sdReady ? "OK" : "NO", sdReady ? UI_BAR_OK : UI_BAR_WARN);
  drawStatusLine(376, "SPR", (pmd.loaded || mon.loaded) ? "OK" : "FLASH/NO", (pmd.loaded || mon.loaded) ? UI_BAR_OK : UI_BAR_WARN);
  char petLine[32];
  if (pet.isEgg()) snprintf(petLine, sizeof(petLine), "EGG");
  else snprintf(petLine, sizeof(petLine), "#%d LV%u", pet.speciesId, pet.level());
  drawStatusLine(390, "PET", petLine, UI_INK);

  gfx->fillRoundRect(133, 404, 200, 40, 13, UI_BAR_OK);
  gfx->setTextColor(UI_BG_DAY);
  gfx->setTextSize(3);
  gfx->setCursor(CX - 18, 414);
  gfx->print("OK");
  gfx->flush();
}

void clockTap(int16_t x, int16_t y) {
  if (y >= 190 && y <= 248) {  // fila de botones +/-
    if (x >= 104 && x < 162) clockH = (clockH + 23) % 24;
    else if (x >= 170 && x < 228) clockH = (clockH + 1) % 24;
    else if (x >= 252 && x < 310) clockM = (clockM + 59) % 60;
    else if (x >= 318 && x < 376) clockM = (clockM + 1) % 60;
    return;
  }
  if (y >= LANG_PILL_Y && y <= LANG_PILL_Y + LANG_PILL_H) {
    if (x >= SOUND_PILL_X && x < SOUND_PILL_X + SOUND_PILL_W) {
      audioSetMode(nextSoundMode());
      if (audioEnabled()) sfxPlay(SFX_LEVEL);  // confirma el nuevo modo si no esta apagado
      return;
    }
    if (x >= LANG_PILL_X && x < LANG_PILL_X + LANG_PILL_W) {  // cicla idioma
      setLang((Lang)((gLang + 1) % LANG_COUNT));
      sfxPlay(SFX_TAP);
      return;
    }
  }
  if (y >= 404 && y <= 444 && x >= 133 && x <= 333) { applyClock(); return; }
}

// llama + numero de racha arriba a la izquierda
void drawStreakBadge() {
  if (pet.streak < 1) return;
  int x = 26, y = 16;
  gfx->fillTriangle(x + 8, y, x + 1, y + 17, x + 15, y + 17, UI_BAR_BAD);
  gfx->fillTriangle(x + 8, y + 7, x + 4, y + 17, x + 12, y + 17, UI_BAR_WARN);
  char s[6];
  snprintf(s, sizeof(s), "%u", pet.streak);
  gfx->setTextColor(inkColor());
  gfx->setTextSize(2);
  gfx->setCursor(x + 22, y + 2);
  gfx->print(s);
}

// banner temporal: medalla nueva o hito de racha
void drawCelebration() {
  const char *l1 = nullptr, *l2 = nullptr;
  char buf[20];
  if (pet.showMedal()) {
    for (int i = 0; i < MED_COUNT; i++)
      if (pet.newMedal & (1 << i)) { l2 = medalName(i); break; }
    l1 = T(S_MEDAL_BANNER);
  } else if (pet.showDexReward()) {
    snprintf(buf, sizeof(buf), T(S_DEX_GOAL_FMT), pet.lastDexRewardGoal());
    l1 = T(S_DEX_REWARD);
    l2 = buf;
  } else if (pet.showMilestone()) {
    snprintf(buf, sizeof(buf), T(S_STREAK_DAYS_FMT), pet.streak);
    l1 = T(S_GREAT);
    l2 = buf;
  }
  if (!l1) return;
  gfx->fillRoundRect(73, 150, 320, 96, 16, UI_BAR_WARN);
  gfx->drawRoundRect(73, 150, 320, 96, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(l1) * 9, 176);
  gfx->print(l1);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(l2) * 6, 212);
  gfx->print(l2);
}

// medallas en la ficha: badge con etiqueta, color si conseguida
void drawMedalBadge(int x, int y, int i) {
  bool got = pet.hasMedal(1 << i);
  gfx->fillRoundRect(x, y, 100, 24, 6, got ? UI_BAR_OK : UI_TRACK);
  if (!got) gfx->drawRoundRect(x, y, 100, 24, 6, UI_TRACK);
  gfx->setTextColor(got ? UI_BG_DAY : 0x9492);
  gfx->setTextSize(2);
  gfx->setCursor(x + (100 - (int)strlen(medalLabel(i)) * 12) / 2, y + 5);
  gfx->print(medalLabel(i));
}

// pagina 0: perfil (retrato grande, identidad, racha, vinculo, baya)
void renderCardProfile() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  char head[26];
  snprintf(head, sizeof(head), T(S_NAME_FMT), pet.shiny ? "*" : "", nm, pet.level());
  gfx->setTextColor(d.accent);
  // auto-encoge: a tamano 3 los nombres largos no caben en la franja estrecha de
  // arriba de la pantalla redonda, asi que se cortaban por el borde
  int hlen = strlen(head);
  int hts = (hlen <= 11) ? 3 : 2;
  gfx->setTextSize(hts);
  gfx->setCursor(CX - hlen * (hts == 3 ? 9 : 6), hts == 3 ? 34 : 40);
  gfx->print(head);
  if (pet.nick[0]) {  // especie real bajo el apodo
    gfx->setTextColor(UI_TRACK);
    gfx->setTextSize(2);
    const char *speciesName = dexName(pet.speciesId);
    gfx->setCursor(CX - (strlen(speciesName) + 2) * 6, 64);
    gfx->printf("(%s)", speciesName);
  }

  // retrato grande animado
  if (pmd.loaded) drawPmdAct(PMD_IDLE, CX, 206, millis(), true, false, 4);

  // racha con llama
  int sx = 138, sy = 224;
  gfx->fillTriangle(sx + 8, sy, sx + 1, sy + 18, sx + 15, sy + 18, UI_BAR_BAD);
  gfx->fillTriangle(sx + 8, sy + 7, sx + 4, sy + 18, sx + 12, sy + 18, UI_BAR_WARN);
  char rl[30];
  snprintf(rl, sizeof(rl), T(S_STREAK_FMT), pet.streak, pet.bestStreak);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(sx + 24, sy + 2);
  gfx->print(rl);

  drawCardStat(258, T(S_VIN), pet.bond, 100, C565(0xd4, 0x52, 0x7e));

  const char *berry = !pet.berryKnown ? T(S_BERRY_UNK)
                      : pet.lovesBerry(0) ? T(S_BERRY_RED)
                      : pet.lovesBerry(1) ? T(S_BERRY_BLUE)
                                          : T(S_BERRY_GREEN);
  char info[40];
  snprintf(info, sizeof(info), T(S_INFO_FMT), berry,
           (unsigned long)(pet.ageMinutes / 1440));
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(info) * 6, 296);
  gfx->print(info);

  gfx->setTextColor(UI_TRACK);
  gfx->setCursor(CX - strlen(T(S_RENAME_HINT)) * 6, 332);
  gfx->print(T(S_RENAME_HINT));
}

StrId personalityNameId(PetPersonality p) {
  switch (p) {
    case PERS_PLAYFUL: return S_PERS_PLAYFUL;
    case PERS_BRAVE: return S_PERS_BRAVE;
    case PERS_CALM: return S_PERS_CALM;
    case PERS_LAZY: return S_PERS_LAZY;
    default: return S_PERS_BALANCED;
  }
}

StrId personalityHintId(PetPersonality p) {
  switch (p) {
    case PERS_PLAYFUL: return S_PERS_PLAYFUL_HINT;
    case PERS_BRAVE: return S_PERS_BRAVE_HINT;
    case PERS_CALM: return S_PERS_CALM_HINT;
    case PERS_LAZY: return S_PERS_LAZY_HINT;
    default: return S_PERS_BALANCED_HINT;
  }
}

uint16_t personalityColor(PetPersonality p) {
  switch (p) {
    case PERS_PLAYFUL: return UI_BAR_WARN;
    case PERS_BRAVE: return UI_BAR_BAD;
    case PERS_CALM: return 0x4C98;
    case PERS_LAZY: return 0xB3C8;
    default: return UI_BAR_OK;
  }
}

void drawPersonalityRecord(int x, int y, const char *label, uint16_t val, uint16_t color) {
  gfx->fillRoundRect(x, y, 118, 34, 8, UI_WHITE);
  gfx->drawRoundRect(x, y, 118, 34, 8, color);
  gfx->setTextColor(color);
  gfx->setTextSize(1);
  gfx->setCursor(x + 10, y + 6);
  gfx->print(label);
  char num[8];
  snprintf(num, sizeof(num), "%u", val);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(x + 118 - 12 - (int)strlen(num) * 12, y + 14);
  gfx->print(num);
}

// pagina 1: personalidad calculada + records largos, sin tocar balance
void renderCardPersonality() {
  PetPersonality pers = pet.personality();
  const char *title = T(S_PERSONALITY);
  const char *name = T(personalityNameId(pers));
  const char *hint = T(personalityHintId(pers));
  uint16_t col = personalityColor(pers);

  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(title) * 9, 44);
  gfx->print(title);

  gfx->fillRoundRect(62, 86, 342, 70, 16, col);
  gfx->setTextColor(UI_BG_DAY);
  int nts = (strlen(name) <= 10) ? 3 : 2;
  gfx->setTextSize(nts);
  gfx->setCursor(CX - strlen(name) * (nts == 3 ? 9 : 6), nts == 3 ? 104 : 111);
  gfx->print(name);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(hint) * 6, 136);
  gfx->print(hint);

  drawCardStat(182, T(S_VIN), pet.bond, 100, C565(0xd4, 0x52, 0x7e));
  drawCardStat(220, T(S_BAR_JOY), pet.joy, 100, UI_BAR_WARN);

  char age[20];
  snprintf(age, sizeof(age), T(S_AGE_DAYS_FMT), (unsigned long)(pet.ageMinutes / 1440));
  gfx->setTextColor(UI_TRACK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(age) * 6, 258);
  gfx->print(age);

  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_RECORDS)) * 6, 286);
  gfx->print(T(S_RECORDS));
  drawPersonalityRecord(70, 310, T(S_GAME_BALL), pet.gameHi, UI_BAR_OK);
  drawPersonalityRecord(198, 310, T(S_GAME_CATCH), pet.catchHi, UI_BAR_WARN);
  drawPersonalityRecord(70, 350, T(S_GAME_MEMO), pet.memoHi, 0x4C98);
  drawPersonalityRecord(198, 350, T(S_BATTLE), pet.bestBattleStreak, UI_BAR_BAD);
}

StrId dailyGoalLabelId(uint8_t goalType) {
  switch (goalType) {
    case DAILY_GOAL_CARE: return S_GOAL_CARE;
    case DAILY_GOAL_PLAY: return S_GOAL_PLAY;
    case DAILY_GOAL_BATTLE: return S_GOAL_BATTLE;
    case DAILY_GOAL_CATCH: return S_GOAL_CATCH;
    case DAILY_GOAL_MEMO: return S_GOAL_MEMO;
    default: return S_GOAL_CARE;
  }
}

uint16_t dailyGoalColor(uint8_t goalType) {
  switch (goalType) {
    case DAILY_GOAL_CARE: return C565(0xd4, 0x52, 0x7e);
    case DAILY_GOAL_PLAY: return UI_BAR_WARN;
    case DAILY_GOAL_BATTLE: return UI_BAR_BAD;
    case DAILY_GOAL_CATCH: return UI_BAR_OK;
    case DAILY_GOAL_MEMO: return 0x4C98;
    default: return UI_INK;
  }
}

void drawDailyGoalRow(int y, uint8_t idx) {
  uint8_t type = pet.dailyGoalType[idx];
  uint8_t target = pet.dailyGoalTarget(type);
  uint8_t progress = pet.dailyGoalProgress[idx] > target ? target : pet.dailyGoalProgress[idx];
  bool done = pet.dailyGoalComplete(idx);
  uint16_t col = dailyGoalColor(type);
  gfx->fillRoundRect(58, y, 350, 52, 12, done ? col : UI_WHITE);
  gfx->drawRoundRect(58, y, 350, 52, 12, col);
  gfx->setTextSize(2);
  gfx->setTextColor(done ? UI_BG_DAY : UI_INK);
  gfx->setCursor(82, y + 18);
  gfx->print(T(dailyGoalLabelId(type)));

  char prog[12];
  snprintf(prog, sizeof(prog), "%u/%u", progress, target);
  gfx->setCursor(286, y + 18);
  gfx->print(done ? T(S_DONE) : prog);
  if (done) {
    gfx->fillCircle(374, y + 26, 12, UI_BG_DAY);
    gfx->setTextColor(col);
    gfx->setCursor(368, y + 18);
    gfx->print("v");
  }
}

// pagina 2: objetivos diarios
void renderCardDaily() {
  pet.ensureDailyGoals();
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_DAILY)) * 9, 44);
  gfx->print(T(S_DAILY));
  const char *phase = T(dayPhaseTextId(currentDayPhase()));
  gfx->setTextColor(UI_TRACK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(phase) * 6, 74);
  gfx->print(phase);

  uint8_t done = 0;
  for (uint8_t i = 0; i < DAILY_GOAL_COUNT; i++) {
    if (pet.dailyGoalComplete(i)) done++;
    drawDailyGoalRow(104 + i * 70, i);
  }

  char bonus[24];
  snprintf(bonus, sizeof(bonus), "%s %u/%u", T(S_REWARD), done, DAILY_GOAL_COUNT);
  gfx->setTextColor(done == DAILY_GOAL_COUNT ? UI_BAR_OK : UI_TRACK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(bonus) * 6, 324);
  gfx->print(bonus);
}

#define BOX_ROWS 5

bool boxComesBefore(int16_t a, int16_t b) {
  if (boxSort == 1) {
    const DexEntry &da = DEX_TBL[a];
    const DexEntry &db = DEX_TBL[b];
    if (da.type1 != db.type1) return da.type1 < db.type1;
    if (da.type2 != db.type2) return da.type2 < db.type2;
  } else if (boxSort == 2) {
    bool ra = pet.isRegistered(a);
    bool rb = pet.isRegistered(b);
    if (ra != rb) return ra;
  }
  return a < b;
}

uint16_t boxBuildList(int16_t *out) {
  uint16_t n = 0;
  for (int16_t dex = 1; dex <= DEX_COUNT; dex++)
    if (pet.isCaught(dex)) out[n++] = dex;
  for (uint16_t i = 1; i < n; i++) {
    int16_t v = out[i];
    int j = i - 1;
    while (j >= 0 && boxComesBefore(v, out[j])) {
      out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = v;
  }
  return n;
}

uint8_t boxPageCount() {
  uint16_t count = pet.caughtCount();
  uint8_t pages = (count + BOX_ROWS - 1) / BOX_ROWS;
  return pages > 0 ? pages : 1;
}

int16_t boxDexAt(uint16_t index) {
  int16_t list[DEX_COUNT];
  uint16_t n = boxBuildList(list);
  return index < n ? list[index] : 0;
}

const char *boxSortLabel() {
  if (boxSort == 1) return T(S_SORT_TYPE);
  if (boxSort == 2) return T(S_SORT_RAISED);
  return T(S_SORT_DEX);
}

void renderCardBox() {
  uint8_t pages = boxPageCount();
  if (boxPage >= pages) boxPage = pages - 1;

  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_BOX)) * 9, 42);
  gfx->print(T(S_BOX));

  const char *sort = boxSortLabel();
  gfx->fillRoundRect(302, 62, 106, 28, 9, UI_WHITE);
  gfx->drawRoundRect(302, 62, 106, 28, 9, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(1);
  gfx->setCursor(302 + (106 - (int)strlen(sort) * 6) / 2, 73);
  gfx->print(sort);

  char caught[24], known[24], goal[22];
  snprintf(caught, sizeof(caught), T(S_CAUGHT_COUNT_FMT), pet.caughtCount());
  snprintf(known, sizeof(known), T(S_KNOWN_FMT), pet.knownDexCount());
  snprintf(goal, sizeof(goal), T(S_DEX_GOAL_FMT), pet.nextDexGoal());
  gfx->setTextSize(2);
  gfx->setTextColor(UI_INK);
  gfx->setCursor(72, 70);
  gfx->print(caught);
  gfx->setTextColor(UI_TRACK);
  gfx->setCursor(72, 92);
  gfx->print(known);
  gfx->setCursor(258, 92);
  gfx->print(goal);

  if (pet.caughtCount() == 0) {
    gfx->fillRoundRect(82, 178, 302, 72, 16, UI_WHITE);
    gfx->drawRoundRect(82, 178, 302, 72, 16, UI_TRACK);
    gfx->setTextColor(UI_TRACK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(T(S_NO_CATCHES)) * 6, 207);
    gfx->print(T(S_NO_CATCHES));
    return;
  }

  for (uint8_t i = 0; i < BOX_ROWS; i++) {
    int16_t dex = boxDexAt((uint16_t)boxPage * BOX_ROWS + i);
    if (dex <= 0) break;
    const DexEntry &d = DEX_TBL[dex];
    int y = 122 + i * 42;
    bool raised = pet.isRegistered(dex);
    gfx->fillRoundRect(58, y, 350, 34, 9, UI_WHITE);
    gfx->drawRoundRect(58, y, 350, 34, 9, d.accent);
    char name[24];
    snprintf(name, sizeof(name), "#%03d %s", dex, dexName(dex));
    int ts = strlen(name) <= 16 ? 2 : 1;
    gfx->setTextSize(ts);
    gfx->setTextColor(UI_INK);
    gfx->setCursor(72, y + (ts == 2 ? 7 : 5));
    gfx->print(name);
    char types[22];
    typeText(types, sizeof(types), d);
    gfx->setTextSize(1);
    gfx->setTextColor(battleTypeColor(d.type1));
    gfx->setCursor(72, y + 24);
    gfx->print(types);
    if (raised) {
      gfx->setTextColor(UI_BAR_OK);
      gfx->setCursor(330, y + 15);
      gfx->print(T(S_RAISED_MARK));
    }
  }
  gfx->fillRoundRect(76, 348, 94, 38, 11, boxPage > 0 ? UI_TRACK : C565(0xe4, 0xe8, 0xee));
  gfx->fillRoundRect(296, 348, 94, 38, 11, boxPage + 1 < pages ? UI_TRACK : C565(0xe4, 0xe8, 0xee));
  gfx->setTextColor(UI_BG_DAY);
  gfx->setTextSize(3);
  gfx->setCursor(111, 357);
  gfx->print("<");
  gfx->setCursor(331, 357);
  gfx->print(">");
  char pg[12];
  snprintf(pg, sizeof(pg), T(S_PAGE_FMT), boxPage + 1, pages);
  gfx->setTextColor(UI_TRACK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(pg) * 6, 360);
  gfx->print(pg);
}

// pagina 3: combate (4 barras + botones)
void renderCardStats() {
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_BATTLE)) * 9, 48);
  gfx->print(T(S_BATTLE));

  drawCardStat(112, T(S_STAT_ATK), pet.atkStat(), 260, UI_BAR_BAD);
  drawCardStat(150, T(S_STAT_DEF), pet.defStat(), 260, 0x4C98);
  drawCardStat(188, T(S_STAT_SPE), pet.speStat(), 260, UI_BAR_WARN);
  drawCardStat(226, T(S_STAT_WGT), pet.weight, 100, 0xB3C8);

  char wl[20], bs[18], bb[16];
  snprintf(wl, sizeof(wl), T(S_WL_FMT), pet.battleWins, pet.battleLosses);
  snprintf(bs, sizeof(bs), T(S_BSTREAK_FMT), pet.battleStreak);
  snprintf(bb, sizeof(bb), T(S_BBEST_FMT), pet.bestBattleStreak);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(74, 266);
  gfx->print(wl);
  gfx->setCursor(210, 266);
  gfx->print(bs);
  gfx->setCursor(334, 266);
  gfx->print(bb);

  gfx->fillRoundRect(96, 294, 274, 36, 11, 0x4C98);
  gfx->setTextColor(UI_BG_DAY);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_WILD_BATTLE)) * 6, 305);
  gfx->print(T(S_WILD_BATTLE));

  // boton: saco de entrenamiento de fuerza
  gfx->fillRoundRect(96, 338, 274, 36, 11, UI_BAR_BAD);
  gfx->setTextColor(UI_BG_DAY);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_TRAIN_STR)) * 6, 349);
  gfx->print(T(S_TRAIN_STR));
}

// pagina 2: medallas con etiqueta descriptiva
void renderCardMedals() {
  int got = 0;
  for (int i = 0; i < MED_COUNT; i++)
    if (pet.hasMedal(1 << i)) got++;
  char head[20];
  snprintf(head, sizeof(head), T(S_MEDALS_FMT), got, MED_COUNT);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(head) * 9, 48);
  gfx->print(head);

  for (int i = 0; i < MED_COUNT; i++) {
    int x = 28 + (i % 2) * 206, y = 104 + (i / 2) * 54;
    bool g = pet.hasMedal(1 << i);
    gfx->fillRoundRect(x, y, 196, 44, 10, g ? UI_BAR_OK : UI_TRACK);
    if (g) {  // marca de conseguida
      gfx->fillCircle(x + 22, y + 22, 11, UI_BG_DAY);
      gfx->setTextColor(UI_BAR_OK);
      gfx->setTextSize(2);
      gfx->setCursor(x + 16, y + 13);
      gfx->print("v");
    }
    gfx->setTextColor(g ? UI_BG_DAY : 0x8410);
    gfx->setTextSize(2);
    gfx->setCursor(x + 44, y + 14);
    gfx->print(medalDesc(i));
  }
}

// pagina 3: progreso (nivel, evolucion, descuidos) — saca a la luz mecanicas
// que antes eran invisibles (cuanto falta para subir/evolucionar y por que)
void renderCardProgress() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(T(S_PROGRESS)) * 9, 44);
  gfx->print(T(S_PROGRESS));

  // nivel grande
  char lv[10];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), pet.level());
  gfx->setTextSize(5);
  gfx->setCursor(CX - strlen(lv) * 15, 86);
  gfx->print(lv);

  // barra de progreso al siguiente nivel (1 nivel = 60 min de juego)
  uint8_t into = pet.ageMinutes % MINUTES_PER_LEVEL;
  int bx = 93, bw = 280, by = 158, bh = 22;
  gfx->fillRoundRect(bx, by, bw, bh, 6, UI_TRACK);
  int fw = (bw - 4) * into / MINUTES_PER_LEVEL;
  if (fw > 0) gfx->fillRoundRect(bx + 2, by + 2, fw, bh - 4, 5, UI_BAR_OK);
  char nx[26];
  snprintf(nx, sizeof(nx), T(S_NEXT_LVL_FMT), MINUTES_PER_LEVEL - into, pet.level() + 1);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(nx) * 6, by + 32);
  gfx->print(nx);

  // estado de evolucion
  gfx->setTextColor(UI_TRACK);
  gfx->setCursor(CX - strlen(T(S_EVO_LABEL)) * 6, 230);
  gfx->print(T(S_EVO_LABEL));
  char evoBuf[28];
  const char *evo;
  uint16_t evoCol = UI_INK;
  if (d.evolvesTo == 0) {
    evo = T(S_FINAL_FORM);
  } else {
    int needed = d.evolveLevel + pet.careMistakes;
    if (pet.level() >= needed) {
      if (pet.lowestStat() >= 40) { evo = T(S_EVO_READY); evoCol = UI_BAR_OK; }
      else { evo = T(S_EVO_BLOCKED); evoCol = UI_BAR_BAD; }
    } else {
      snprintf(evoBuf, sizeof(evoBuf), T(S_EVO_IN_FMT), needed - pet.level());
      evo = evoBuf;
    }
  }
  gfx->setTextColor(evoCol);
  gfx->setCursor(CX - strlen(evo) * 6, 256);
  gfx->print(evo);

  // descuidos (retrasan la evolucion)
  char ms[24];
  snprintf(ms, sizeof(ms), T(S_MISTAKES_FMT), pet.careMistakes);
  gfx->setTextColor(pet.careMistakes > 0 ? UI_BAR_BAD : UI_INK);
  gfx->setCursor(CX - strlen(ms) * 6, 312);
  gfx->print(ms);
}

void renderCard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  if (cardPage == 0) renderCardProfile();
  else if (cardPage == 1) renderCardPersonality();
  else if (cardPage == 2) renderCardDaily();
  else if (cardPage == 3) renderCardBox();
  else if (cardPage == 4) renderCardStats();
  else if (cardPage == 5) renderCardMedals();
  else renderCardProgress();

  // indicador de paginas + ayuda
  int dotsX = CX - ((CARD_COUNT - 1) * 24) / 2;
  for (int i = 0; i < CARD_COUNT; i++) {
    if (i == cardPage) gfx->fillCircle(dotsX + i * 24, 400, 5, UI_INK);
    else gfx->drawCircle(dotsX + i * 24, 400, 4, UI_INK);
  }
  gfx->setTextColor(UI_TRACK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_BACK)) * 6, 420);
  gfx->print(T(S_BACK));
  gfx->flush();
}

// ---------- teclado para renombrar ----------

static const char KB_KEYS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-";  // 28 + DEL + OK = 30
#define KB_COLS 6
#define KB_X 40
#define KB_Y 150
#define KB_W 64
#define KB_H 52

void openKeyboard() {
  kbOpen = true;
  strncpy(nameBuf, pet.nick, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = 0;
  nameLen = strlen(nameBuf);
  sfxPlay(SFX_MENU);
}

void renderKeyboard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(T(S_NAME)) * 6, 56);
  gfx->print(T(S_NAME));
  // buffer actual
  gfx->fillRoundRect(83, 84, 300, 40, 8, UI_WHITE);
  gfx->drawRoundRect(83, 84, 300, 40, 8, UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(95, 94);
  gfx->print(nameLen ? nameBuf : "_");

  for (int i = 0; i < 30; i++) {
    int x = KB_X + (i % KB_COLS) * KB_W, y = KB_Y + (i / KB_COLS) * KB_H;
    bool special = (i >= 28);
    gfx->fillRoundRect(x, y, KB_W - 6, KB_H - 6, 6, special ? UI_BAR_WARN : UI_WHITE);
    gfx->drawRoundRect(x, y, KB_W - 6, KB_H - 6, 6, UI_INK);
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    if (i < 28) {
      gfx->setCursor(x + KB_W / 2 - 9, y + KB_H / 2 - 10);
      gfx->print(KB_KEYS[i]);
    } else {
      const char *lab = (i == 28) ? "<-" : "OK";
      gfx->setCursor(x + KB_W / 2 - 15, y + KB_H / 2 - 10);
      gfx->print(lab);
    }
  }
  gfx->flush();
}

void keyboardTap(int16_t x, int16_t y) {
  int col = (x - KB_X) / KB_W, row = (y - KB_Y) / KB_H;
  if (col < 0 || col >= KB_COLS || row < 0 || row >= 5) return;
  int i = row * KB_COLS + col;
  if (i >= 30) return;
  sfxPlay(SFX_TAP);
  if (i == 28) {  // borrar
    if (nameLen) nameBuf[--nameLen] = 0;
  } else if (i == 29) {  // OK
    pet.rename(nameBuf);
    kbOpen = false;
  } else if (nameLen < sizeof(nameBuf) - 1) {
    nameBuf[nameLen++] = KB_KEYS[i];
    nameBuf[nameLen] = 0;
  }
}

// ---------- galeria pokedex ----------

#define GAL_X 73
#define GAL_Y 92
#define GAL_CELL 80

bool galleryDexVisible(int16_t dex) {
  if (dex < 1 || dex > 151) return false;
  if (galleryFilter == 1) return pet.isRegistered(dex);
  if (galleryFilter == 2) return pet.isCaught(dex);
  return true;
}

uint16_t galleryFilteredCount() {
  if (galleryFilter == 0) return 151;
  uint16_t count = 0;
  for (int16_t dex = 1; dex <= 151; dex++)
    if (galleryDexVisible(dex)) count++;
  return count;
}

int galleryPageCount() {
  uint16_t count = galleryFilteredCount();
  int pages = (count + 15) / 16;
  return pages > 0 ? pages : 1;
}

int16_t galleryDexAt(uint16_t index) {
  for (int16_t dex = 1; dex <= 151; dex++) {
    if (!galleryDexVisible(dex)) continue;
    if (index == 0) return dex;
    index--;
  }
  return 0;
}

// dibuja una miniatura centrada en su celda; sil=true la pinta en tinta
void drawThumb(const uint8_t *b, int x, int y, int s, bool sil) {
  uint8_t w = b[0], h = b[1], n = b[2];
  const uint8_t *pal = b + 3;
  const uint8_t *d = pal + n * 2;
  int ox = x + (GAL_CELL - w * s) / 2;
  int oy = y + (GAL_CELL - h * s) / 2;
  for (int r = 0; r < h; r++) {
    for (int c = 0; c < w; c++) {
      uint8_t idx = d[r * w + c];
      if (idx == 0xFF) continue;
      uint16_t col = sil ? INK_K : (uint16_t)(pal[idx * 2] | (pal[idx * 2 + 1] << 8));
      gfx->fillRect(ox + c * s, oy + r * s, s, s, col);
    }
  }
}

void renderGallery() {
  if (galleryDetail) {  // vista detalle: se redibuja siempre (animada)
    gfx->fillScreen(RGB565_BLACK);
    gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
    const DexEntry &d = DEX_TBL[galleryDetail];
    bool reg = pet.isRegistered(galleryDetail);
    bool caught = pet.isCaught(galleryDetail);
    bool known = reg || caught;
    char head[24];
    snprintf(head, sizeof(head), "N.%03d %s%s", galleryDetail,
             pet.isShinyRegistered(galleryDetail) ? "*" : "", known ? dexName(galleryDetail) : "???");
    gfx->setTextColor(known ? d.accent : UI_INK);
    int glen = strlen(head);
    int gts = (glen <= 13) ? 3 : 2;  // auto-encoge nombres largos (no caben a t3)
    gfx->setTextSize(gts);
    gfx->setCursor(CX - glen * (gts == 3 ? 9 : 6), gts == 3 ? 56 : 60);
    gfx->print(head);
    if (known) {
      char types[24];
      typeText(types, sizeof(types), d);
      gfx->setTextColor(battleTypeColor(d.type1));
      gfx->setTextSize(2);
      gfx->setCursor(CX - strlen(types) * 6, 94);
      gfx->print(types);
    }
    if (galleryPmd.loaded) {
      // animado y a color si se conoce; silueta estatica si no (estilo "?")
      drawPmdActM(galleryPmd, PMD_IDLE, CX, 300, known ? millis() : 0, true, !known, 6);
    } else {
      const uint8_t *t = thumbs.get(galleryDetail);
      if (t) drawThumb(t, CX - GAL_CELL, 135, 4, !known);
    }
    if (reg) {
      const char *mark = T(S_RAISED_MARK);
      gfx->setTextColor(UI_BAR_OK);
      gfx->setTextSize(2);
      gfx->setCursor(CX - strlen(mark) * 6, caught ? 354 : 366);
      gfx->print(mark);
    }
    if (caught) {
      const char *mark = T(S_CAUGHT_MARK);
      gfx->setTextColor(UI_BAR_WARN);
      gfx->setTextSize(2);
      gfx->setCursor(CX - strlen(mark) * 6, reg ? 376 : 366);
      gfx->print(mark);
    }
    gfx->setTextColor(UI_INK);
    gfx->setTextSize(2);
    gfx->setCursor(CX - strlen(T(S_DETAIL_BACK)) * 6, 408);
    gfx->print(T(S_DETAIL_BACK));
    gfx->flush();
    return;
  }

  if (!galleryDirty) return;  // la rejilla es estatica
  galleryDirty = false;

  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(3);
  gfx->setCursor(CX - 7 * 9, 28);
  gfx->print("POKEDEX");

  char head[28];
  snprintf(head, sizeof(head), "R:%u C:%u", pet.registeredCount(), pet.caughtCount());
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(head) * 6, 54);
  gfx->print(head);

  const char *filters[3] = { T(S_FILTER_ALL), T(S_RAISED_MARK), T(S_CAUGHT_MARK) };
  for (int i = 0; i < 3; i++) {
    int fx = 74 + i * 106;
    uint16_t fill = (galleryFilter == i) ? UI_INK : UI_WHITE;
    uint16_t text = (galleryFilter == i) ? UI_BG_DAY : UI_INK;
    gfx->fillRoundRect(fx, 74, 96, 18, 6, fill);
    gfx->drawRoundRect(fx, 74, 96, 18, 6, UI_INK);
    gfx->setTextColor(text);
    gfx->setTextSize(1);
    gfx->setCursor(fx + (96 - (int)strlen(filters[i]) * 6) / 2, 80);
    gfx->print(filters[i]);
  }

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int16_t dex = galleryDexAt(galleryPage * 16 + r * 4 + c);
      if (dex <= 0) continue;
      int x = GAL_X + c * GAL_CELL, y = GAL_Y + r * GAL_CELL;
      const uint8_t *t = thumbs.get(dex);
      if (t) {
        bool reg = pet.isRegistered(dex);
        bool caught = pet.isCaught(dex);
        bool known = reg || caught;
        drawThumb(t, x, y, 2, !known);
        if (pet.isShinyRegistered(dex)) {
          gfx->setTextColor(UI_BAR_WARN);
          gfx->setTextSize(2);
          gfx->setCursor(x + 62, y + 4);
          gfx->print("*");
        } else if (caught && !reg) {
          gfx->setTextColor(UI_BAR_WARN);
          gfx->setTextSize(1);
          gfx->setCursor(x + 60, y + 6);
          gfx->print("C");
        }
      } else {
        char num[6];
        snprintf(num, sizeof(num), "%d", dex);
        gfx->setTextColor(UI_TRACK);
        gfx->setTextSize(2);
        gfx->setCursor(x + 24, y + 32);
        gfx->print(num);
      }
    }
  }
  // puntos de pagina
  int pages = galleryPageCount();
  int dotX = CX - (pages - 1) * 7;
  for (int i = 0; i < pages; i++) {
    if (i == galleryPage) gfx->fillCircle(dotX + i * 14, 436, 4, UI_INK);
    else gfx->drawCircle(dotX + i * 14, 436, 3, UI_INK);
  }
  gfx->flush();
}

void galleryTap(int16_t x, int16_t y) {
  if (galleryDetail) {  // volver a la rejilla
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    sfxPlay(SFX_TAP);
    return;
  }
  if (y < 46) {  // tocar la cabecera = salir
    galleryOpen = false;
    galleryPmd.unload();
    sfxPlay(SFX_TAP);
    return;
  }
  if (y >= 68 && y < GAL_Y) {
    int f = (x - 74) / 106;
    if (f >= 0 && f < 3 && x >= 74 + f * 106 && x <= 170 + f * 106) {
      galleryFilter = (uint8_t)f;
      galleryPage = 0;
      galleryDirty = true;
      sfxPlay(SFX_TAP);
      return;
    }
  }
  if (x < GAL_X || y < GAL_Y) return;
  int c = (x - GAL_X) / GAL_CELL, r = (y - GAL_Y) / GAL_CELL;
  if (c < 0 || c > 3 || r < 0 || r > 3) return;
  int16_t dex = galleryDexAt(galleryPage * 16 + r * 4 + c);
  if (dex <= 0) return;
  galleryDetail = dex;
  galleryPmd.load(dex, pet.isShinyRegistered(dex));
  sfxPlay(SFX_MENU);
}

void drawBattery() {
  int pc = batPercent();
  if (pc < 0) return;  // sin bateria conectada
  int x = CX - 14, y = 12, w = 24, h = 11;
  bool charging = batCharging();
  uint16_t col = charging ? UI_BAR_OK
                 : (pc >= 40) ? inkColor()
                 : (pc >= 15) ? UI_BAR_WARN
                              : UI_BAR_BAD;
  gfx->drawRoundRect(x, y, w, h, 2, col);
  gfx->fillRect(x + w, y + 3, 3, 5, col);  // borne
  if (charging) {
    // rayo de carga (zigzag) en vez de la barra de nivel
    uint16_t bolt = C565(0xff, 0xd9, 0x4a);
    int bx = x + w / 2;
    gfx->fillTriangle(bx + 3, y + 1, bx - 4, y + 6, bx + 1, y + 6, bolt);
    gfx->fillTriangle(bx - 1, y + 5, bx + 4, y + 5, bx - 3, y + 10, bolt);
  } else {
    int fw = (w - 4) * pc / 100;
    if (fw > 0) gfx->fillRect(x + 2, y + 2, fw, h - 4, col);
  }
}

void drawHeader(const char *name, uint16_t nameColor, const char *msg) {
  drawBattery();
  gfx->setTextColor(nameColor);
  gfx->setTextSize(3);
  gfx->setCursor(CX - strlen(name) * 9, 52);
  gfx->print(name);
  gfx->setTextColor(inkColor());
  gfx->setTextSize(2);
  gfx->setCursor(CX - strlen(msg) * 6, 90);
  gfx->print(msg);
}

// animacion de la ceremonia (10s): despedida = reverencia con corazones y se
// aleja caminando; escapada = se asusta y sale corriendo. Sustituye al idle.
void drawCeremony() {
  if (!pmd.loaded) { drawPet(); return; }  // respaldo si no hay sprite PMD
  uint32_t now = millis();
  float t = pet.ceremonyT();               // 0..1 a lo largo de los 10s
  bool panic = (pet.ceremony == CER_RUNAWAY);
  int x = CX, y = PET_GROUND;
  uint8_t act = PMD_IDLE;

  if (panic) {
    // final triste: penumbra azulada + lluvia
    for (int i = 0; i < 46; i++) {
      int rx = (i * 47 + now / 3) % 466;
      int ry = (i * 91 + now / 2) % 470;
      gfx->drawLine(rx, ry, rx - 3, ry + 12, C565(0x6a, 0x84, 0xb0));
    }
    bool fade = false;
    if (t < 0.30f) {                       // cabizbajo, temblando
      act = pmd.has(PMD_HURT) ? PMD_HURT : PMD_IDLE;
      x = CX + (int)(4 * sinf(now * 0.04f));
    } else {                               // se aleja despacio y se desvanece
      act = pmd.has(PMD_WALKL) ? PMD_WALKL : PMD_IDLE;
      x = CX - (int)(((t - 0.30f) / 0.70f) * (CX + 120));
      fade = (t > 0.6f) && ((now / 160) % 2 == 0);  // parpadea hacia la silueta
    }
    drawPmdAct(act, x, y, now, true, fade, 5);  // fade=silueta: se difumina al irse
    // lagrima cayendo del bicho
    if (t < 0.55f) {
      int ty = y - 150 + (int)((now / 6) % 40);
      gfx->fillRect(x + 6, ty, 3, 6, C565(0x9a, 0xc4, 0xe8));
    }
    return;
  }

  // despedida epica: halo dorado pulsante + chispas y corazones que ascienden
  int gcy = PET_GROUND - 96;
  for (int k = 0; k < 4; k++) {
    int r = 60 + k * 34 + (int)(10 * sinf(now * 0.02f));
    gfx->drawCircle(CX, gcy, r, C565(0xff, 0xdf, 0x8a));
  }
  for (int i = 0; i < 16; i++) {
    int px = (i * 71 + 28) % 466;
    int py = 410 - (int)((now / 8 + i * 70) % 360);   // suben y reaparecen abajo
    if (py < 30) continue;
    if (i % 4 == 0) drawMap(SPR_HEART, 32, px - 8, py - 8, 1, false);  // corazoncito
    else gfx->fillRect(px, py, 4, 4, (i % 2) ? C565(0xff, 0xe7, 0x9f) : C565(0xff, 0x9a, 0xc0));
  }

  if (t < 0.45f) {                         // reverencia / pose de despedida
    act = pmd.has(PMD_POSE) ? PMD_POSE : (pmd.has(PMD_NOD) ? PMD_NOD : PMD_IDLE);
  } else {                                 // se aleja por la derecha
    act = pmd.has(PMD_WALKR) ? PMD_WALKR : PMD_IDLE;
    x = CX + (int)(((t - 0.45f) / 0.55f) * (CX + 140));
  }
  drawPmdAct(act, x, y, now, true, false, 5);
  if (pet.showHeart())                     // corazon grande siguiendo al bicho
    drawMap(SPR_HEART, 32, x + 50, y - 190, 2, false);
}

// dialogo de decision (2 botones apilados): evolucionar/mantener o despedirse/quedaros
void drawChoiceDialog() {
  const char *q, *o1, *o2;
  uint16_t c1, c2, t1, t2;
  if (choiceKind == 1) {  // evolucion
    q = T(S_EVO_Q); o1 = T(S_EVO_TAP); o2 = T(S_EVO_KEEP);
    c1 = UI_BAR_BAD; t1 = UI_WHITE; c2 = UI_TRACK; t2 = UI_INK;
  } else {                // despedida
    q = T(S_FAR_Q); o1 = T(S_FAR_GO); o2 = T(S_FAR_STAY);
    c1 = UI_BAR_WARN; t1 = UI_INK; c2 = UI_BAR_OK; t2 = UI_WHITE;
  }
  gfx->fillRoundRect(73, 156, 320, 188, 16, UI_WHITE);
  gfx->drawRoundRect(73, 156, 320, 188, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(q) * 6, 176);
  gfx->print(q);
  gfx->fillRoundRect(93, 206, 280, 52, 12, c1);     // boton accion
  gfx->setTextColor(t1);
  gfx->setCursor(CX - (int)strlen(o1) * 6, 224);
  gfx->print(o1);
  gfx->fillRoundRect(93, 268, 280, 52, 12, c2);     // boton mantener/quedaros
  gfx->setTextColor(t2);
  gfx->setCursor(CX - (int)strlen(o2) * 6, 286);
  gfx->print(o2);
}

// boton-CTA rojo y grande para evolucionar (pulsa para llamar la atencion)
void drawEvolveButton() {
  uint32_t now = millis();
  int p = (int)(5 * sinf(now * 0.006f));  // late: -5..5
  int x = EVO_BTN_X - p, y = EVO_BTN_Y - p, w = EVO_BTN_W + 2 * p, h = EVO_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 18, UI_BAR_BAD);
  gfx->drawRoundRect(x, y, w, h, 18, UI_WHITE);
  gfx->drawRoundRect(x + 2, y + 2, w - 4, h - 4, 16, UI_WHITE);
  gfx->setTextColor(UI_WHITE);
  gfx->setTextSize(3);
  const char *t = T(S_EVO_TAP);
  gfx->setCursor(CX - (int)strlen(t) * 9, y + h / 2 - 11);
  gfx->print(t);
}

// boton-CTA dorado de despedida: "<nombre> quiere decirte algo..."
void drawFarewellButton() {
  uint32_t now = millis();
  int p = (int)(4 * sinf(now * 0.005f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, UI_BAR_WARN);
  gfx->drawRoundRect(x, y, w, h, 16, UI_INK);
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_FAREWELL_BTN), nm);
  gfx->setTextColor(UI_INK);
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(buf) * 6, y + h / 2 - 8);
  gfx->print(buf);
}

// boton-CTA sombrio de escapada por abandono: "<nombre> se siente abandonado..."
// (final triste: azul-gris oscuro, latido lento y apagado)
void drawRunawayButton() {
  uint32_t now = millis();
  int p = (int)(3 * sinf(now * 0.003f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, C565(0x3a, 0x44, 0x5a));
  gfx->drawRoundRect(x, y, w, h, 16, C565(0x70, 0x80, 0x98));
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_RUNAWAY_BTN), nm);
  gfx->setTextColor(C565(0xc8, 0xd2, 0xe0));
  gfx->setTextSize(2);
  gfx->setCursor(CX - (int)strlen(buf) * 6, y + h / 2 - 8);
  gfx->print(buf);
}

// animacion epica de evolucion: halo radial + rayos giratorios + parpadeo del
// sprite acelerando + chispas que salen disparadas + fogonazo final
void drawEvolveFX(uint32_t now) {
  float t = pet.evolveT();          // 0..1
  int cx = CX, cy = PET_GROUND - 96;

  // halo radial que crece y pulsa
  int halo = 36 + (int)(t * 150) + (int)(8 * sinf(now * 0.02f));
  for (int k = 0; k < 4; k++) {
    int r = halo - k * 7;
    if (r > 0) gfx->drawCircle(cx, cy, r, UI_WHITE);
  }
  // rayos giratorios desde el centro del bicho
  float base = now * 0.004f;
  for (int i = 0; i < 12; i++) {
    float a = base + i * (float)(PI / 6);
    int len = 90 + (int)(70 * (0.5f + 0.5f * sinf(now * 0.012f + i)));
    gfx->drawLine(cx, cy, cx + (int)(cosf(a) * len), cy + (int)(sinf(a) * len), UI_WHITE);
  }
  // parpadeo entre la forma ANTERIOR y la NUEVA (siluetas), acelerando; al
  // final (t>0.9) se queda fija en la nueva para el fogonazo de revelado
  int period = 60 + (int)(220 * (1.0f - t));
  bool showOld = t < 0.9f && evoPmd.loaded && ((now / period) % 2) == 0;
  if (showOld) drawPmdActM(evoPmd, PMD_IDLE, cx, PET_GROUND, 0, true, true, 5);
  else drawPmdAct(PMD_IDLE, cx, PET_GROUND, 0, true, true, 5);
  // chispas que salen disparadas
  for (int i = 0; i < 10; i++) {
    float a = i * (float)(PI / 5) + t * 4.0f;
    int d = (int)((now / 14 + i * 33) % 200);
    int sx = cx + (int)(cosf(a) * d), sy = cy + (int)(sinf(a) * d);
    gfx->fillRect(sx - 2, sy - 2, 5, 5, (i & 1) ? C565(0xff, 0xe0, 0x70) : UI_WHITE);
  }
  // fogonazo final antes de revelar la forma nueva
  if (t > 0.9f) gfx->fillCircle(cx, cy, (int)(300 * (t - 0.9f) / 0.1f), UI_WHITE);
}

void drawPet() {
  if (pmd.loaded) {
    drawPetPMD();
    return;
  }
  if (mon.loaded) {
    drawPetSD();
    return;
  }
  int fi = flashIdxForDex(pet.speciesId);
  if (fi < 0) {
    // sin SD y sin sprite de flash: aviso claro de que faltan sprites
    gfx->setTextColor(inkColor());
    gfx->setTextSize(6);
    gfx->setCursor(CX - 18, PET_CY - 80);
    gfx->print("?");
    gfx->setTextSize(2);
    const char *l1 = T(S_NO_SPRITES);
    gfx->setCursor(CX - (int)strlen(l1) * 6, PET_CY - 4);
    gfx->print(l1);
    const char *l2 = T(S_LOAD_SPRITES);
    gfx->setCursor(CX - (int)strlen(l2) * 6, PET_CY + 20);
    gfx->print(l2);
    return;
  }
  const Species &sp = SPECIES[fi];
  int s = sp.scale;
  int x = CX - 16 * s;
  int y = PET_CY - 16 * s;

  // animacion de evolucion: alterna la silueta de la forma anterior y la nueva
  if (pet.evolving()) {
    bool flash = (millis() / 300) % 2;
    int16_t showDex = (flash && pet.prevSpeciesId >= 0) ? pet.prevSpeciesId : pet.speciesId;
    int sfi = flashIdxForDex(showDex);
    if (sfi >= 0) {
      const Species &show = SPECIES[sfi];
      drawMap(show.sprite, SPRITE_H, CX - 16 * show.scale, PET_CY - 16 * show.scale, show.scale, flash);
    }
    return;
  }

  PetMood m = pet.mood();
  if (m == MOOD_HAPPY && (millis() / 500) % 2) y -= 6;  // saltito

  drawMap(sp.sprite, SPRITE_H, x, y, s, false);

  // expresiones superpuestas usando las anclas de la especie
  bool blink = (millis() % 3500 < 300);
  if (m == MOOD_SLEEPING || blink) {
    overlayEye(sp, x, y, s, sp.eyeColL);
    overlayEye(sp, x, y, s, sp.eyeColR);
  }
  if (m == MOOD_EATING) overlayMouth(sp, x, y, s, true);
  else if (m == MOOD_SAD) overlayMouth(sp, x, y, s, false);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + 20 * s, y - 2 * s, 2, false);
}

// ---------- escena de bano ----------

void startBath() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathUntil) return;
  bathUntil = millis() + 3000;
  bathPending = true;
  sfxPlay(SFX_EVENT_SPARKLE);
  int cx = (int)beh.x;
  for (auto &b : bubbles) {
    b.x = cx - 70 + random(140);
    b.y = PET_GROUND - random(150);
    b.r = 8 + random(16);
    b.ph = random(64);
  }
}

void drawBath() {
  uint32_t now = millis();
  if (now > bathUntil) {
    bathUntil = 0;
    if (bathPending) {
      bathPending = false;
      pet.clean();
      // pose de alegria al quedar limpio
      if (pmd.has(PMD_POSE)) {
        beh.mode = 2;
        beh.act = PMD_POSE;
        beh.t0 = now;
        beh.until = now + pmdActTotalMs(pmd.acts[PMD_POSE]) * 2;
      }
    }
    return;
  }
  uint32_t left = bathUntil - now;
  if (left > 800) {
    // espuma: pompas meciendose y subiendo poco a poco
    float t = now / 220.0f;
    for (auto &b : bubbles) {
      int bx = b.x + (int)(sinf(t + b.ph) * 6);
      int by = b.y - (int)((3000 - left) / 90);
      gfx->fillCircle(bx, by, b.r, UI_WHITE);
      gfx->drawCircle(bx, by, b.r, 0x7E3D);
      gfx->fillCircle(bx - b.r / 3, by - b.r / 3, b.r / 4, UI_BG_DAY);
    }
  } else {
    // las pompas revientan: destellos
    for (int i = 0; i < 8; i++) {
      auto &b = bubbles[i];
      int sx = b.x + (i % 3) * 6 - 6, sy = b.y - 18;
      uint16_t col = (i % 2) ? UI_BAR_WARN : UI_WHITE;
      gfx->fillRect(sx - 6, sy - 1, 13, 3, col);
      gfx->fillRect(sx - 1, sy - 6, 3, 13, col);
    }
  }
}

// ---------- mascota PMD: comportamiento ----------

uint32_t pmdActTotalMs(const PmdAct &a) {
  uint32_t t = 0;
  for (uint8_t i = 0; i < a.frames; i++) t += a.ms[i];
  return t ? t : 100;
}

uint8_t pmdFrameAt(const PmdAct &a, uint32_t t, bool loop) {
  uint32_t total = pmdActTotalMs(a);
  if (!loop && t >= total) return a.frames - 1;
  t %= total;
  uint8_t i = 0;
  while (t >= a.ms[i]) {
    t -= a.ms[i];
    i = (i + 1) % a.frames;
  }
  return i;
}

// dibuja una accion anclada por la base (centro-x, suelo) y devuelve su escala
// dibuja una accion de un PmdMon concreto (m); drawPmdAct usa el global pmd
void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  const PmdAct &a = m.acts[actId];
  if (!a.frames) return;
  uint8_t sBase = m.acts[PMD_IDLE].h ? 170 / m.acts[PMD_IDLE].h : 5;
  if (sBase < 2) sBase = 2;
  if (sBase > maxS) sBase = maxS;
  uint8_t s = sBase;
  while (s > 2 && a.h * s > 250) s--;  // acciones con frame grande (ataque)
  uint8_t fi = pmdFrameAt(a, t, loop);
  const uint8_t *fr = a.data + (uint32_t)fi * a.w * a.h;
  // anclar por los pies (a.base), no por el alto del lienzo: asi las acciones
  // con padding distinto (Hurt, Eat...) quedan todas a la misma altura de suelo
  int x0 = cx - a.w * s / 2, y0 = groundY - (a.base ? a.base : a.h) * s;
  for (int r = 0; r < a.h; r++) {
    const uint8_t *row = fr + r * a.w;
    for (int c = 0; c < a.w; c++) {
      uint8_t idx = row[c];
      if (idx == 0xFF) continue;
      gfx->fillRect(x0 + c * s, y0 + r * s, s, s, sil ? INK_K : m.pal[idx]);
    }
  }
}
void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  drawPmdActM(pmd, actId, cx, groundY, t, loop, sil, maxS);
}

// elige el siguiente capricho del bicho cuando esta contento
void behNext() {
  uint32_t now = millis();
  beh.t0 = now;
  int r = random(100);
  if (r < 35 && (pmd.has(PMD_WALKL) || pmd.has(PMD_WALKR))) {
    beh.mode = 1;  // paseo
    beh.targetX = 150 + random(176);
    beh.until = now + 15000;
  } else if (r < 60) {
    // gesto aleatorio entre los disponibles
    // (Hop fuera: salta demasiado alto; Sit fuera: mira hacia atras)
    static const uint8_t flair[] = { PMD_POSE, PMD_NOD, PMD_BREATH };
    uint8_t pick[3], n = 0;
    for (uint8_t f : flair)
      if (pmd.has(f)) pick[n++] = f;
    if (n) {
      beh.mode = 2;
      beh.act = pick[random(n)];
      beh.until = now + pmdActTotalMs(pmd.acts[beh.act]);
      return;
    }
    beh.mode = 0;
    beh.until = now + 2000 + random(3000);
  } else {
    beh.mode = 0;  // mirar al frente
    beh.until = now + 2000 + random(3000);
  }
}

void drawPetPMD() {
  uint32_t now = millis();

  if (pet.evolving()) {
    drawEvolveFX(now);
    return;
  }
  if (evoPmd.loaded) evoPmd.unload();  // termino la evolucion: libera la forma anterior

  PetMood m = pet.mood();
  uint8_t act;
  bool loop = true;
  if (m == MOOD_SLEEPING && pmd.has(PMD_SLEEP)) {
    act = PMD_SLEEP;
    beh.mode = 0;
  } else if (m == MOOD_EATING && pmd.has(PMD_EAT)) {
    act = PMD_EAT;
    beh.t0 = 0;
  } else if (m == MOOD_SAD && pmd.has(PMD_HURT)) {
    act = PMD_HURT;
  } else {
    // contento: el planificador decide (idle / paseo / gesto)
    if (now > beh.until) behNext();
    if (beh.mode == 1) {
      float d = beh.targetX - beh.x;
      if (fabsf(d) < 4) {
        behNext();
        act = PMD_IDLE;
      } else {
        beh.x += (d > 0 ? 3.0f : -3.0f);
        act = (d > 0) ? PMD_WALKR : PMD_WALKL;
      }
    } else {
      act = (beh.mode == 2) ? beh.act : PMD_IDLE;
      loop = false;
    }
    if (!pmd.has(act)) act = PMD_IDLE;
  }

  drawPmdAct(act, (int)beh.x, PET_GROUND, now - beh.t0, loop || act == PMD_IDLE, false, 5);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, (int)beh.x + 50, PET_GROUND - 190, 2, false);
}

// sprite animado desde la SD: zoom entero por pixel, frames a su ritmo
void drawPetSD() {
  int s = mon.scale;
  int w = mon.w * s, h = mon.h * s;
  int x = CX - w / 2;
  int y = PET_CY - h / 2;

  bool sil = false;
  if (pet.evolving()) {
    sil = (millis() / 300) % 2;
  } else if (pet.mood() == MOOD_HAPPY && (millis() / 500) % 2) {
    y -= 6;  // saltito
  }

  uint16_t fm = mon.frameMs ? mon.frameMs : 100;
  uint16_t fi = pet.sleeping ? 0 : (millis() / fm) % mon.frames;
  const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
  for (int r = 0; r < mon.h; r++) {
    const uint8_t *row = fr + r * mon.w;
    for (int c = 0; c < mon.w; c++) {
      uint8_t idx = row[c];
      if (idx == 0xFF) continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, sil ? INK_K : mon.pal[idx]);
    }
  }

  // emotes en vez de expresiones (los sprites importados no tienen anclas)
  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + w - 30, y - 50, 2, false);
}

// ojo cerrado: borra el ojo 3x4 y dibuja el parpado
void overlayEye(const Species &sp, int x, int y, int s, int col) {
  gfx->fillRect(x + col * s, y + sp.eyeRow * s, 3 * s, 4 * s, sp.bodyColor);
  gfx->fillRect(x + col * s, y + (sp.eyeRow + 2) * s, 3 * s, s, INK_K);
}

// borra la sonrisa base y pinta boca abierta (comer) o ceno (triste)
void overlayMouth(const Species &sp, int x, int y, int s, bool open) {
  int mc = sp.mouthCol, mr = sp.mouthRow;
  gfx->fillRect(x + (mc - 3) * s, y + mr * s, 7 * s, 2 * s, sp.bodyColor);
  if (open) {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, 2 * s, INK_K);
  } else {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, s, INK_K);
    gfx->fillRect(x + (mc - 3) * s, y + (mr + 1) * s, s, s, INK_K);
    gfx->fillRect(x + (mc + 3) * s, y + (mr + 1) * s, s, s, INK_K);
  }
}

void drawPoops() {
  for (int i = 0; i < pet.poops; i++) {
    drawMap(SPR_POOP, 32, 36 + i * 46, 244, 2, false);
  }
}

void drawBars() {
  drawBar(78, 318, T(S_BAR_FOOD), pet.fullness);
  drawBar(244, 318, T(S_BAR_JOY), pet.joy);
  drawBar(78, 346, T(S_BAR_ENE), pet.energy);
  drawBar(244, 346, T(S_BAR_HYG), pet.hygiene);
}

void drawBar(int x, int y, const char *label, uint8_t val) {
  gfx->setTextColor(inkColor());
  gfx->setTextSize(2);
  gfx->setCursor(x, y);
  gfx->print(label);
  int bx = x + 48, bw = 100, bh = 15;  // +48: deja sitio a etiquetas de 4 letras (EN)
  uint16_t fill = (val >= 50) ? UI_BAR_OK : (val >= 25) ? UI_BAR_WARN : UI_BAR_BAD;
  gfx->fillRoundRect(bx, y, bw, bh, 4, UI_TRACK);
  int fw = (bw - 4) * val / 100;
  if (fw > 0) gfx->fillRoundRect(bx + 2, y + 2, fw, bh - 4, 3, fill);
}

void drawButtons() {
  for (int i = 0; i < 4; i++) {
    bool off = pet.sleeping && i != 2;  // durmiendo solo funciona LUZ
    int bx = buttons[i].cx - BTN_HALF, by = buttons[i].cy - BTN_HALF;
    if (!pet.sleeping) gfx->fillRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, UI_WHITE);
    gfx->drawRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, inkColor());
    if (!off) drawMap(buttons[i].icon, 16, buttons[i].cx - 16, buttons[i].cy - 16, 2, false);
  }
}

const char *eggMsg() {
  switch (pet.eggCracks()) {
    case 0: return T(S_EGG_TOUCH);
    case 1: return T(S_EGG_MOVES);
    default: return T(S_EGG_ALMOST);
  }
}

const char *statusMsg() {
  if (pet.evolving()) return T(S_EVOLVING);
  if (bathUntil) return "Splish splash!";  // onomatopeya universal
  if (pet.sleeping) return "Zzz...";
  if (pet.eating()) return T(S_EATING);
  if (pet.showHeart()) return T(S_LIKES);
  if (pet.fullness < 25) return T(S_HUNGRY);
  if (pet.hygiene < 25) return T(S_NEEDS_BATH);
  if (pet.energy < 25) return T(S_EXHAUSTED);
  if (pet.joy < 25) return T(S_SAD);
  if (pet.weight > 60) return T(S_CHUBBY);
  if (pet.shiny && pet.ageMinutes < 15) return T(S_IS_SHINY);
  return T(S_HAPPY);
}

// dibuja un mapa de n x n pixeles escalado; silhouette=true lo pinta en tinta
void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette) {
  for (int r = 0; r < n; r++) {
    for (int c = 0; c < n; c++) {
      char ch = map[r][c];
      if (ch == '.') continue;
      gfx->fillRect(x + c * s, y + r * s, s, s, silhouette ? INK_K : spriteColor(ch));
    }
  }
}
