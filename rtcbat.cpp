#include "batcurve.h"
#include "rtcbat.h"
#include "clockzone.h"
#include "pin_config.h"  // define XPOWERS_CHIP_AXP2101 y capacidades (HAS_RTC/HAS_PMU)
#include <Wire.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#if HAS_RTC
#include <SensorPCF85063.hpp>
static SensorPCF85063 rtc;
static bool rtcOk = false;
#endif

#if HAS_PMU
#include <XPowersLib.h>
static XPowersPMU pmu;
static bool pmuOk = false;
#endif

uint32_t rtcCivilToEpoch(int year, int mon, int day, int hour, int min, int sec) {
  return clockCivilEpoch(year, mon, day, hour, min, sec);
}

uint32_t rtcCompileFloorEpoch() {
  static uint32_t cached = 0;
  if (cached) return cached;
  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {0};
  int d = 0, y = 0, H = 0, M = 0, S = 0;
  if (sscanf(__DATE__, "%3s %d %d", mon, &d, &y) != 3) {
    cached = 1767225600UL + 30UL * 86400UL;  // fallback: a month after the seed
    return cached;
  }
  sscanf(__TIME__, "%d:%d:%d", &H, &M, &S);
  const char *p = strstr(months, mon);
  int mon1 = p ? (int)(p - months) / 3 + 1 : 1;
  int64_t sec = (int64_t)rtcCivilToEpoch(y, mon1, d, H, M, S);
  // __DATE__/__TIME__ are the build machine's local clock; treat as UTC and
  // subtract two days so a TZ skew cannot reject a valid stamp.
  sec -= (int64_t)2 * 86400;
  if (sec < 0) sec = 0;
  cached = (uint32_t)sec;
  return cached;
}

bool rtcEpochIsPlausible(uint32_t e) {
  return e >= rtcCompileFloorEpoch();
}

#if HAS_RTC
bool rtcBegin() {
  rtcOk = rtc.begin(Wire, IIC_SDA, IIC_SCL);
  if (!rtcOk) Serial.println("PCF85063 no detectado");
  return rtcOk;
}

uint32_t rtcEpoch() {
  if (!rtcOk) return 0;
  RTC_DateTime t = rtc.getDateTime();
  if (t.getYear() < 2025 || t.getYear() > 2120) return 0;  // sin hora valida
  // El PCF guarda UTC (rtcSetEpoch escribe con gmtime_r). Conversion
  // independiente de TZ: con TZ sin poner da lo mismo que el mktime de antes,
  // y con una zona horaria puesta no resta la diferencia dos veces.
  uint32_t e = rtcCivilToEpoch(t.getYear(), t.getMonth(), t.getDay(),
                               t.getHour(), t.getMinute(), t.getSecond());
  return e;
}

void rtcSetEpoch(uint32_t e) {
  if (!rtcOk) return;
  time_t tt = e;
  struct tm tmv;
  gmtime_r(&tt, &tmv);
  rtc.setDateTime(RTC_DateTime(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                               tmv.tm_hour, tmv.tm_min, tmv.tm_sec));
}
#elif BOARD_AMOLED_175C
// The 1.75C has no PCF85063. Keep a software clock (time()/settimeofday) so the
// clock screen and the scene hour still work; it is lost when the AXP cuts
// power, like the original board when its RTC loses backup power.
#include <sys/time.h>
static const uint32_t kSoftMinEpoch = 1735689600UL;  // 2025-01-01: below = no time
bool rtcBegin() { return false; }  // no PCF85063 on this board: software clock only
uint32_t rtcEpoch() {
  time_t now = time(nullptr);
  return now < (time_t)kSoftMinEpoch ? 0 : (uint32_t)now;
}
void rtcSetEpoch(uint32_t e) {
  if (e < kSoftMinEpoch) return;
  struct timeval tv = { (time_t)e, 0 };
  settimeofday(&tv, nullptr);
}
#else
// placa sin PCF85063 (p. ej. Guition 4848S040): sin hora persistente entre
// arranques; el juego degrada a reloj del ultimo guardado (sin RTC no hay
// progreso offline de mas, pero tampoco se peta nada)
bool rtcBegin() { return false; }
uint32_t rtcEpoch() { return 0; }
void rtcSetEpoch(uint32_t) {}
#endif

#if HAS_PMU
bool batBegin() {
  pmuOk = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (!pmuOk) { Serial.println("AXP2101 no detectado"); return false; }
  pmu.enableBattVoltageMeasure();
  pmu.setSysPowerDownVoltage(BAT_HW_CUTOFF_MV);  // respaldo: el PMU corta solo si el firmware no llega a tiempo
  return true;
}

// Enciende la alimentacion de la AMOLED. Hay que llamarla ANTES de gfx->begin().
// 1.75: el panel (OLED VDD) cuelga del rail BLDO1 a 3.3V del AXP2101. Si el PMU
// se resetea (drenaje total), BLDO1 queda OFF y la pantalla se ve negra.
// 1.75C: el conector J3 del panel se alimenta de VCC3V3 (AXP DCDC1), que ya esta
// encendido cuando arranca el ESP32. BLDO1 en esa placa es 1.2V y BLDO2 es 2.8V
// — no hay que reprogramarlos a 3.3V.
void pmuEnablePanel() {
  if (!pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("AXP2101 no detectado (pmuEnablePanel)");
    return;
  }
#if BOARD_AMOLED_175C
  // Panel on VCC3V3 (DCDC1). Leave the AXP LDO voltages at board defaults.
  (void)0;
#else
  pmu.setBLDO1Voltage(3300);   // OLED VDD
  pmu.enableBLDO1();
#endif
}

// el estado de energia (I2C) se cachea ~2 s: leerlo en cada frame del loop
// metia trafico I2C inutil y podia oscilar (parpadeo de brillo)
static uint32_t powerCacheT = 0;
static int cachedPct = -1;
static bool cachedCharging = false, cachedUsb = true;

static BatMonitor mon;
static bool monDischarging = false;
static uint32_t pollMs = 2000;
void batSetPollMs(uint32_t ms) { pollMs = ms; }

static void refreshPower() {
  uint32_t now = millis();
  if (powerCacheT && now - powerCacheT < pollMs) return;
  powerCacheT = now ? now : 1;
  if (!pmuOk) { cachedPct = -1; cachedCharging = false; cachedUsb = true; return; }
  bool conn = pmu.isBatteryConnect();
  cachedCharging = pmu.isCharging();
  cachedUsb = pmu.isVbusIn();
  // En descarga el porcentaje sale de la curva de una celda LiPo (batcurve.h); con USB o cargando
  // la tension sube por la carga y no vale, y se usa la del PMU.
  bool discharging = conn && !cachedUsb && !cachedCharging;
  if (discharging != monDischarging) {
    mon.reset();
    mon.seed(cachedPct);  // al desenchufar el porcentaje no sube (la tension sigue inflada por la carga)
    monDischarging = discharging;
  }
  if (discharging) cachedPct = mon.sample((int)pmu.getBattVoltage(), now);
  else cachedPct = conn ? pmu.getBatteryPercent() : -1;
  if (cachedPct < 0 && conn) cachedPct = pmu.getBatteryPercent();  // sin lectura de tension
}

int batPercent() { refreshPower(); return cachedPct; }
bool batCharging() { refreshPower(); return cachedCharging; }
bool batShouldShutdown() { refreshPower(); return monDischarging && mon.shutdownNeeded(millis()); }
void batPowerOff() {
  if (pmuOk) pmu.shutdown();
  mon.reset();  // si sigue vivo (p. ej. se enchufo USB justo ahora) tiene que volver a cumplirse todo el tiempo
}
int batMillivolts() { return pmuOk ? (int)pmu.getBattVoltage() : 0; }
bool usbPresent() { refreshPower(); return cachedUsb; }
bool usbPresentNow() { return pmuOk ? pmu.isVbusIn() : true; }

void pwrSetup() {
#if BOARD_AMOLED_175C
  if (pmuOk) {
    // AXP2101 register 0x27 bits [3:2] = 00: keep the 4 s hardware power-off.
    // No PEK IRQ here: the triple press is read raw from GPIO3 (screenlock.h).
    if (!pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S))
      Serial.println("PWR: could not set AXP 4s shutdown; check PMU");
    pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    pmu.clearIrqStatus();
  }
  // SYS_OUT is already pulled up on the board (R9). High = PWR held.
  pinMode(PWR_SENSE_PIN, INPUT);
#else
  if (!pmuOk) return;
  pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  pmu.clearIrqStatus();
#endif
}

bool pwrShortPressed() {
#if BOARD_AMOLED_175C
  return false;  // raw GPIO3 goes to PwrButton in the sketch (triple press)
#else
  if (!pmuOk) return false;
  pmu.getIrqStatus();
  bool hit = pmu.isPekeyShortPressIrq();
  if (hit) pmu.clearIrqStatus();
  return hit;
#endif
}
#else
// placa sin AXP2101 ni bateria (Guition 4848S040: alimentacion USB-C): toda la
// gestion de energia es no-op; se comporta como siempre enchufado
bool batBegin() { return false; }
void pmuEnablePanel() {}
void batSetPollMs(uint32_t) {}
int batPercent() { return -1; }
bool batCharging() { return false; }
int batMillivolts() { return 0; }
bool batShouldShutdown() { return false; }
void batPowerOff() {}
bool usbPresent() { return true; }
bool usbPresentNow() { return true; }
void pwrSetup() {}
bool pwrShortPressed() { return false; }
#endif
