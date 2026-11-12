// Politica de consumo (power.h).
#include "framework.h"
#include "../power.h"
#include "../audio.h"

static PowerCtx ctx(uint32_t idle) {
  PowerCtx c = {};
  c.idleMs = idle;
  c.usb = false;
  c.batPct = 80;
  return c;
}

TEST(power, idleSince_no_desborda_si_el_toque_es_posterior_al_reloj_leido) {
  CHECK_EQ((int)idleSince(1000, 400), 600);
  CHECK_EQ((int)idleSince(1000, 1000), 0);
  CHECK_EQ((int)idleSince(1000, 1002), 0);               // el toque se atendio 2 ms despues de leer el reloj
  CHECK_EQ((int)dimStageFor(idleSince(1000, 1002)), (int)DIM_AWAKE);
  CHECK_EQ((int)idleSince(5, 0xFFFFFFF0u), 21);
  CHECK(idleSince(0x90000000u, 0) == 0x90000000u);       // 28 dias sin tocar: no se confunde con un toque futuro          // vuelta del reloj de 49 dias
}

TEST(power, los_escalones_de_atenuacion_son_los_de_siempre) {
  CHECK_EQ((int)dimStageFor(0), (int)DIM_AWAKE);
  CHECK_EQ((int)dimStageFor(90000), (int)DIM_AWAKE);
  CHECK_EQ((int)dimStageFor(90001), (int)DIM_LOW);
  CHECK_EQ((int)dimStageFor(300000), (int)DIM_LOW);
  CHECK_EQ((int)dimStageFor(300001), (int)DIM_OFF);
}

TEST(power, el_brillo_no_cambia_con_bateria_normal) {
  PowerCtx c = ctx(0);
  CHECK_EQ((int)brightnessFor(c), 145);       // bateria
  c.usb = true;
  CHECK_EQ((int)brightnessFor(c), 180);       // USB
  c = ctx(100000);
  CHECK_EQ((int)brightnessFor(c), 60);        // atenuada
  c.sleeping = true;
  CHECK_EQ((int)brightnessFor(c), 10);
  c = ctx(400000);
  CHECK_EQ((int)brightnessFor(c), 8);
  c = ctx(0);
  c.screenOff = true;
  CHECK_EQ((int)brightnessFor(c), 0);
}

TEST(power, con_poca_bateria_el_brillo_tiene_tope_y_no_sube_nunca) {
  PowerCtx c = ctx(0);
  c.batPct = 15;
  CHECK_EQ((int)brightnessFor(c), 100);
  c.batPct = 5;
  CHECK_EQ((int)brightnessFor(c), 60);
  c.charging = true;                           // cargando: sin restricciones
  CHECK_EQ((int)brightnessFor(c), 145);
  c = ctx(100000);
  c.batPct = 5;
  CHECK_EQ((int)brightnessFor(c), 60);         // el tope no sube un brillo ya menor
  c = ctx(400000);
  c.batPct = 5;
  CHECK_EQ((int)brightnessFor(c), 8);
}

TEST(power, el_periodo_de_frame_crece_con_la_inactividad_y_nunca_es_cero_con_pantalla_encendida) {
  uint32_t prev = 0;
  for (uint32_t idle = 0; idle <= 600000; idle += 1000) {
    PowerCtx c = ctx(idle);
    uint32_t ms = frameIntervalMs(c);
    CHECK(ms >= 100);
    CHECK(ms >= prev);
    prev = ms;
  }
  CHECK_EQ((int)frameIntervalMs(ctx(0)), 100);        // activo: 10 fps como siempre
  CHECK_EQ((int)frameIntervalMs(ctx(70000)), 150);
  CHECK_EQ((int)frameIntervalMs(ctx(100000)), 250);
  CHECK_EQ((int)frameIntervalMs(ctx(400000)), 1000);
}

TEST(power, el_juego_y_la_pantalla_apagada_mandan_sobre_el_resto) {
  PowerCtx g = ctx(200000);
  g.gameOpen = true;
  CHECK_EQ((int)frameIntervalMs(g), 85);               // la fisica esta afinada a 85 ms
  g.batPct = 5;
  CHECK_EQ((int)frameIntervalMs(g), 85);
  PowerCtx off = ctx(0);
  off.screenOff = true;
  CHECK_EQ((int)frameIntervalMs(off), 0);
}

TEST(power, dormido_y_con_poca_bateria_se_dibuja_menos) {
  PowerCtx c = ctx(0);
  c.sleeping = true;
  CHECK_EQ((int)frameIntervalMs(c), 100);              // dormido pero usandolo: fluido
  c.idleMs = 70000;
  CHECK_EQ((int)frameIntervalMs(c), 500);              // dormido y sin tocar: casi nada se mueve
  c = ctx(0);
  c.batPct = 15;
  CHECK_EQ((int)frameIntervalMs(c), 200);
  c.usb = true;                                        // con USB no hay ahorro por bateria
  CHECK_EQ((int)frameIntervalMs(c), 100);
}

TEST(power, el_amplificador_solo_esta_encendido_con_el_bicho_despierto_y_activo) {
  PowerCtx c = ctx(1000);
  CHECK(amplifierWanted(c));
  c.sleeping = true;
  CHECK(!amplifierWanted(c));
  c = ctx(100000);
  CHECK(!amplifierWanted(c));
  c = ctx(0);
  c.screenOff = true;
  CHECK(!amplifierWanted(c));
}

TEST(power, light_sleep_con_la_pantalla_apagada_o_casi_apagada_y_sin_usb) {
  PowerCtx c = ctx(0);
  CHECK(!lightSleepAllowed(c));                        // en uso: no
  c.idleMs = 100000;
  CHECK(!lightSleepAllowed(c));                        // atenuada: aun no
  c.idleMs = 400000;
  CHECK(lightSleepAllowed(c));                         // casi apagada: si
  c.usb = true;
  CHECK(!lightSleepAllowed(c));                        // con USB nunca
  c = ctx(0);
  c.screenOff = true;
  CHECK(lightSleepAllowed(c));
  c.usb = true;
  CHECK(!lightSleepAllowed(c));
  CHECK_EQ((int)idleDelayMs(c), 20);
  c.screenOff = false;
  CHECK_EQ((int)idleDelayMs(c), 1);
}

TEST(power, la_bateria_se_lee_menos_veces_con_la_pantalla_apagada) {
  CHECK_EQ((int)powerCacheMs(ctx(0)), 2000);
  CHECK_EQ((int)powerCacheMs(ctx(400000)), 10000);
  PowerCtx off = ctx(0);
  off.screenOff = true;
  CHECK_EQ((int)powerCacheMs(off), 10000);
}

// Escenario: se toca el bicho y se deja 10 minutos. Se cuentan los frames volcados (cada uno
// son 466*466*2 bytes por QSPI) y los despertares del bucle con la politica de antes
// (10 fps siempre, 1 ms de pausa) y con la nueva.
TEST(power, diez_minutos_sin_tocar_vuelcan_mucho_menos_que_antes) {
  const uint32_t frameBytes = 466u * 466u * 2u;
  uint64_t before = 0, after = 0;
  uint32_t lastAfter = 0;
  for (uint32_t t = 0; t < 600000; t += 10) {
    if (t % 100 == 0) before += frameBytes;           // antes: un frame cada 100 ms
    uint32_t iv = frameIntervalMs(ctx(t));
    if (iv && t - lastAfter >= iv) { after += frameBytes; lastAfter = t; }
  }
  double ratio = (double)after / (double)before;
  CHECK(ratio < 0.35);                                 // al menos un 65 % menos de datos por QSPI (sale ~0,32)
}

TEST(power, con_la_pantalla_apagada_y_sin_usb_el_bucle_despierta_cinco_veces_por_segundo) {
  PowerCtx c = ctx(0);
  c.screenOff = true;
  CHECK(lightSleepAllowed(c));
  CHECK_EQ(1000 / POWER_SLEEP_MS, 5);                  // antes: 1000 despertares por segundo
}

TEST(power, el_nivel_de_ajustes_escala_el_brillo_y_respeta_el_orden) {
  PowerCtx c = ctx(0);
  c.userDim = 1;
  CHECK_EQ((int)brightnessFor(c), 87);         // 145 al 60 %
  c.userDim = 2;
  CHECK_EQ((int)brightnessFor(c), 43);         // 145 al 30 %
  c.usb = true;
  CHECK_EQ((int)brightnessFor(c), 54);
  // en bajo, cada escalon sigue por debajo del anterior y nunca llega a 0
  c = ctx(0);
  c.userDim = 2;
  uint8_t awake = brightnessFor(c);
  c.idleMs = 100000;
  uint8_t low = brightnessFor(c);
  c.sleeping = true;
  uint8_t lowSleep = brightnessFor(c);
  c.idleMs = 400000;
  uint8_t off = brightnessFor(c);
  CHECK(awake > low);
  CHECK(low > lowSleep);
  CHECK(lowSleep >= 2);
  CHECK(off >= 2);
  c.screenOff = true;
  CHECK_EQ((int)brightnessFor(c), 0);          // apagada con PWR sigue siendo 0
  c = ctx(0);
  c.batPct = 5;
  c.userDim = 2;
  CHECK_EQ((int)brightnessFor(c), 18);         // tope de bateria critica y luego el nivel
}

TEST(power, los_niveles_de_volumen_bajan_y_el_alto_es_el_de_siempre) {
  CHECK_EQ((int)sfxAmp(0), 5000);
  CHECK(sfxAmp(1) < sfxAmp(0));
  CHECK(sfxAmp(2) < sfxAmp(1));
  CHECK(sfxAmp(2) > 0);
  CHECK_EQ((int)sfxAmp(7), (int)sfxAmp(2));   // un valor raro guardado no sube el volumen
}
