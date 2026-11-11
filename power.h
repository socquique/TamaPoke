#pragma once
#include <stdint.h>

// Politica de consumo. Todo lo que decide cuanto trabaja la placa vive aqui como
// funciones puras (sin hardware), para poder probarlas en el PC (test/test_power.cpp).
//
// Donde se gasta la bateria: la pantalla AMOLED (brillo y contenido), la CPU mientras
// dibuja y vuelca el lienzo por QSPI (~70 ms de cada 100 a 10 fps), el amplificador del
// altavoz si esta encendido sin sonar, y los despertares del bucle principal.

// Escalones de inactividad (ms desde el ultimo toque o evento visible)
#define POWER_CALM_MS 60000UL   // pasado este tiempo, se dibuja algo menos
#define POWER_DIM1_MS 90000UL   // pantalla atenuada
#define POWER_DIM2_MS 300000UL  // pantalla casi apagada

enum DimStage : uint8_t { DIM_AWAKE = 0, DIM_LOW = 1, DIM_OFF = 2 };

struct PowerCtx {
  uint32_t idleMs;    // ms sin interaccion
  bool screenOff;     // apagada con el boton PWR
  bool sleeping;      // el bicho duerme
  bool gameOpen;      // minijuego o saco: la fisica esta afinada a 85 ms por frame
  bool usb;           // alimentacion externa
  bool charging;
  int batPct;         // 0-100, -1 si no hay bateria
  uint8_t userDim;    // nivel de brillo elegido en ajustes: 0 = alto (el de siempre), 1 = medio, 2 = bajo
};

// niveles de brillo de ajustes (#34): porcentaje que se aplica a todos los brillos de abajo
#define BRIGHT_LEVELS 3
inline uint8_t userBrightPct(uint8_t userDim) { return userDim == 1 ? 60 : userDim >= 2 ? 30 : 100; }

// ms desde `last`, sin desbordar si `last` es posterior a `now` (un toque atendido despues de
// leer el reloj al principio del bucle): se tomaba como 49 dias de inactividad y el panel
// pasaba un instante a brillo 8 en cada toque
inline uint32_t idleSince(uint32_t now, uint32_t last) {
  int32_t d = (int32_t)(now - last);
  return d < 0 && d > -1000 ? 0 : now - last;  // solo un adelanto de unos ms cuenta como "ahora"; 25 dias sin tocar siguen siendo 25 dias
}

inline uint8_t dimStageFor(uint32_t idleMs) {
  return idleMs > POWER_DIM2_MS ? DIM_OFF : idleMs > POWER_DIM1_MS ? DIM_LOW : DIM_AWAKE;
}

// bateria baja: se ahorra mas (solo si de verdad depende de la bateria)
inline bool batteryLow(const PowerCtx &c) { return c.batPct >= 0 && c.batPct < 20 && !c.charging && !c.usb; }
inline bool batteryCritical(const PowerCtx &c) { return c.batPct >= 0 && c.batPct < 10 && !c.charging && !c.usb; }

// brillo del panel (0-255): el de siempre, con un tope cuando queda poca bateria y escalado
// por el nivel elegido en ajustes
inline uint8_t brightnessFor(const PowerCtx &c) {
  if (c.screenOff) return 0;
  uint8_t stage = dimStageFor(c.idleMs);
  uint8_t t = c.sleeping ? 25 : (c.usb ? 180 : 145);
  if (stage == DIM_LOW) t = c.sleeping ? 10 : 60;
  else if (stage == DIM_OFF) t = 8;
  if (batteryCritical(c) && t > 60) t = 60;
  else if (batteryLow(c) && t > 100) t = 100;
  if (c.userDim) {  // el nivel elegido escala todo, sin bajar de 2 (0 seria pantalla apagada)
    t = (uint8_t)((uint16_t)t * userBrightPct(c.userDim) / 100);
    if (t < 2) t = 2;
  }
  return t;
}

// ms entre frames; 0 = no se dibuja (pantalla apagada). Cada frame cuesta un render
// completo y el volcado del lienzo por QSPI, y el panel solo cambia si se redibuja.
inline uint32_t frameIntervalMs(const PowerCtx &c) {
  if (c.screenOff) return 0;
  if (c.gameOpen) return 85;  // el juego manda: su fisica esta afinada a este periodo
  uint32_t ms;
  uint8_t stage = dimStageFor(c.idleMs);
  if (stage == DIM_OFF) ms = 1000;
  else if (c.sleeping && c.idleMs > POWER_CALM_MS) ms = 500;  // dormido y sin tocar: casi nada se mueve
  else if (stage == DIM_LOW) ms = 250;
  else ms = c.idleMs > POWER_CALM_MS ? 150 : 100;
  if (batteryLow(c)) ms *= 2;
  return ms;
}

// el amplificador del altavoz consume en reposo: solo se tiene encendido mientras
// alguien puede estar jugando
inline bool amplifierWanted(const PowerCtx &c) {
  return !c.screenOff && !c.sleeping && dimStageFor(c.idleMs) == DIM_AWAKE;
}

// pausa del bucle principal en un pase que no dibuja
inline uint32_t idleDelayMs(const PowerCtx &c) { return c.screenOff ? 20 : 1; }

// con la pantalla apagada, o casi apagada (5 min sin tocar), y sin USB se duerme la CPU (light
// sleep) entre frames y sondeos; con USB no: se perderia el puerto serie de la tarjeta SD
inline bool lightSleepAllowed(const PowerCtx &c) {
  return !c.usb && (c.screenOff || dimStageFor(c.idleMs) == DIM_OFF);
}
#define POWER_SLEEP_MS 200  // duracion de cada sueno; el toque despierta antes

// cada cuanto se lee la bateria por I2C
inline uint32_t powerCacheMs(const PowerCtx &c) {
  return (c.screenOff || dimStageFor(c.idleMs) == DIM_OFF) ? 10000 : 2000;
}
