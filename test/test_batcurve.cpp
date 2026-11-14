#include "framework.h"
#include "../batcurve.h"

TEST(batcurve, extremos) {
  CHECK_EQ(batPercentFromMv(4300), 100);
  CHECK_EQ(batPercentFromMv(4200), 100);
  CHECK_EQ(batPercentFromMv(BAT_CUTOFF_MV), 0);
  CHECK_EQ(batPercentFromMv(3000), 0);
  CHECK_EQ(batPercentFromMv(3840), 50);
}

TEST(batcurve, es_monotona_y_cubre_todo_el_rango) {
  int prev = 100;
  for (int mv = 4200; mv >= BAT_CUTOFF_MV; mv--) {
    int p = batPercentFromMv(mv);
    CHECK(p <= prev);
    CHECK(p >= 0 && p <= 100);
    prev = p;
  }
  CHECK_EQ(batPercentFromMv(3660), 8);  // entre (3610, 5) y (3690, 10)
}

// lecturas cada 2 s desde t0 con la misma tension; devuelve si hay que apagar al final
static bool lowFor(BatMonitor &m, int mv, uint32_t t0, uint32_t ms) {
  bool r = false;
  for (uint32_t t = t0; t <= t0 + ms; t += 2000) { m.sample(mv, t); r = m.shutdownNeeded(t); }
  return r;
}

TEST(batcurve, el_apagado_exige_tension_baja_sostenida) {
  BatMonitor m;
  CHECK(!lowFor(m, 3250, 100000, 20000));  // 20 s no bastan
  CHECK(lowFor(m, 3250, 120000, 12000));   // 30+ s seguidos si
}

TEST(batcurve, un_bache_por_carga_no_apaga) {
  BatMonitor m;
  for (uint32_t t = 100000; t < 130000; t += 2000) m.sample(3700, t);
  // una carga larga (sonido, radio) hunde la tension 20 s por debajo del limite y se recupera
  CHECK(!lowFor(m, 3200, 130000, 20000));
  m.sample(3700, 152000); m.sample(3700, 154000); m.sample(3700, 156000); m.sample(3700, 158000);
  CHECK(!m.shutdownNeeded(160000));
}

TEST(batcurve, tras_arrancar_no_apaga) {
  BatMonitor m;
  for (uint32_t t = 0; t <= 20000; t += 2000) m.sample(3200, t);
  CHECK(!m.shutdownNeeded(20000));
  CHECK(!m.shutdownNeeded(BAT_BOOT_GRACE_MS - 1));
  for (uint32_t t = 22000; t <= 60000; t += 2000) m.sample(3200, t);
  CHECK(m.shutdownNeeded(60000));
}

TEST(batcurve, una_lectura_fallida_corta_la_cuenta) {
  BatMonitor m;
  lowFor(m, 3250, 100000, 24000);
  CHECK_EQ(m.sample(0, 126000), -1);
  CHECK(!m.shutdownNeeded(128000));
  CHECK(!lowFor(m, 3250, 128000, 20000));
}

TEST(batcurve, el_porcentaje_solo_baja_en_descarga) {
  BatMonitor m;
  m.seed(60);                              // el PMU decia 60 al desenchufar
  CHECK_EQ(m.sample(4100, 1000), 60);      // la tension inflada por la carga no lo sube
  int a = 60;
  for (uint32_t t = 3000; t < 23000; t += 2000) a = m.sample(3700, t);  // la tension baja de verdad
  CHECK(a < 60 && a > 10);
  int b = m.sample(4000, 5000);            // un repunte tampoco lo sube
  CHECK_EQ(b, a);
  CHECK_EQ(m.sample(3840, 7000) <= a, true);
}

TEST(batcurve, sin_semilla_arranca_en_la_curva) {
  BatMonitor m;
  CHECK_EQ(m.sample(3840, 1000), 50);
  m.reset();
  CHECK_EQ(m.pct, -1);
}

// SpotPear 1.28 AI Ball: XiaoZhi's raw-ADC table (GPIO1, 12 dB, 12 bit), linear in between
TEST(batcurve, bola_tabla_adc_de_xiaozhi) {
  CHECK_EQ(ballBatPercentFromRaw(0), 0);
  CHECK_EQ(ballBatPercentFromRaw(1979), 0);
  CHECK_EQ(ballBatPercentFromRaw(1980), 0);
  CHECK_EQ(ballBatPercentFromRaw(2081), 20);
  CHECK_EQ(ballBatPercentFromRaw(2163), 40);
  CHECK_EQ(ballBatPercentFromRaw(2250), 60);
  CHECK_EQ(ballBatPercentFromRaw(2340), 80);
  CHECK_EQ(ballBatPercentFromRaw(2385), 86);  // measured on the ball while charging
  CHECK_EQ(ballBatPercentFromRaw(2480), 100);
  CHECK_EQ(ballBatPercentFromRaw(4095), 100);
}

TEST(batcurve, bola_tabla_adc_monotona) {
  int prev = 0;
  for (int raw = 1900; raw <= 2600; raw++) {
    int p = ballBatPercentFromRaw(raw);
    CHECK(p >= prev);
    CHECK(p >= 0 && p <= 100);
    prev = p;
  }
}
