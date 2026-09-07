// Integridad de la tabla de la Pokedex (dex.h, generada por tools/gen_dex.py).
#include "framework.h"
#include "shim/Arduino.h"
#include "../dex.h"
#include "../i18n.h"
#include "../pet.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <set>

static const int N = DEX_COUNT;

TEST(dex, la_tabla_tiene_152_entradas) {
  CHECK_EQ((int)(sizeof(DEX_TBL) / sizeof(DEX_TBL[0])), N + 1);
  CHECK_EQ((int)(sizeof(DEX_NAME_FR) / sizeof(DEX_NAME_FR[0])), N + 1);
  CHECK_EQ((int)(sizeof(DEX_NAME_DE) / sizeof(DEX_NAME_DE[0])), N + 1);
}

TEST(dex, todos_los_nombres_son_ascii_en_mayusculas) {
  for (int d = 1; d <= N; d++) {
    const char *n = DEX_TBL[d].name;
    CHECK_MSG(n != NULL && n[0] != 0, "entrada sin nombre");
    if (!n || !n[0]) continue;
    for (const char *c = n; *c; c++) {
      // la fuente GFX del firmware es ASCII y sin minusculas
      CHECK_MSG((unsigned char)*c < 128, std::string("caracter no ASCII en ") + n);
      CHECK_MSG(!islower((unsigned char)*c), std::string("minuscula en ") + n);
    }
  }
}

TEST(dex, los_nombres_traducidos_tambien_son_ascii) {
  const char *const *tables[2] = { DEX_NAME_FR, DEX_NAME_DE };
  for (int t = 0; t < 2; t++) {
    for (int d = 1; d <= N; d++) {
      const char *n = tables[t][d];
      if (!n) continue;  // sin nombre propio: cae al de DEX_TBL
      CHECK_MSG(n[0] != 0, "nombre traducido vacio");
      for (const char *c = n; *c; c++) {
        CHECK_MSG((unsigned char)*c < 128, std::string("caracter no ASCII en ") + n);
        CHECK_MSG(!islower((unsigned char)*c), std::string("minuscula en ") + n);
      }
    }
  }
}

// la cabecera de la ficha es char[26] con "%s%s Lv.%u": ningun nombre en ningun
// idioma puede desbordarla (el nombre se cortaria en pantalla)
TEST(dex, ningun_nombre_desborda_la_cabecera) {
  char head[26];
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    for (int d = 1; d <= N; d++) {
      int need = snprintf(head, sizeof(head), T(S_NAME_FMT), "*", dexName(d), 255);
      CHECK_MSG(need >= 0 && need < (int)sizeof(head),
                std::string("cabecera cortada: ") + dexName(d));
    }
  }
  gLang = LANG_DEFAULT;
}

TEST(dex, las_evoluciones_apuntan_a_entradas_validas) {
  for (int d = 1; d <= N; d++) {
    uint8_t to = DEX_TBL[d].evolvesTo;
    CHECK_MSG(to <= N, std::string("evolvesTo fuera de rango en ") + DEX_TBL[d].name);
    CHECK_MSG(to != d, std::string("se evoluciona a si mismo: ") + DEX_TBL[d].name);
  }
}

TEST(dex, evolucionar_y_nivel_van_de_la_mano) {
  for (int d = 1; d <= N; d++) {
    bool evolves = DEX_TBL[d].evolvesTo != 0;
    bool hasLevel = DEX_TBL[d].evolveLevel > 0;
    CHECK_MSG(evolves == hasLevel,
              std::string("evolvesTo y evolveLevel no cuadran en ") + DEX_TBL[d].name);
  }
}

TEST(dex, ninguna_cadena_evolutiva_hace_bucle) {
  for (int d = 1; d <= N; d++) {
    std::set<int> seen;
    int cur = d;
    int guard = 0;
    while (cur >= 1 && cur <= N && DEX_TBL[cur].evolvesTo != 0 && guard++ < N) {
      CHECK_MSG(seen.insert(cur).second, std::string("bucle evolutivo en ") + DEX_TBL[d].name);
      if (!seen.count(cur)) break;
      cur = DEX_TBL[cur].evolvesTo;
    }
    CHECK_MSG(guard < 6, std::string("cadena demasiado larga desde ") + DEX_TBL[d].name);
  }
}

TEST(dex, los_niveles_de_evolucion_suben_a_lo_largo_de_la_cadena) {
  for (int d = 1; d <= N; d++) {
    int cur = d, prev = 0, guard = 0;
    while (DEX_TBL[cur].evolvesTo != 0 && guard++ < 6) {
      CHECK_MSG(DEX_TBL[cur].evolveLevel > prev,
                std::string("nivel de evolucion no creciente en ") + DEX_TBL[cur].name);
      prev = DEX_TBL[cur].evolveLevel;
      cur = DEX_TBL[cur].evolvesTo;
    }
  }
}

// una especie que solo sale por evolucion tiene que ser alcanzable desde algun
// huevo, o seria imposible de registrar en la pokedex
TEST(dex, toda_especie_es_alcanzable) {
  bool reach[DEX_COUNT + 1] = { false };
  for (int d = 1; d <= N; d++) {
    if (DEX_TBL[d].rarity == R_EVO) continue;  // no sale de huevo
    int cur = d, guard = 0;
    while (cur >= 1 && cur <= N && guard++ < 6) {
      reach[cur] = true;
      if (cur == DEX_EEVEE) {  // rama especial del codigo del juego
        reach[134] = reach[135] = reach[136] = true;
        break;
      }
      if (DEX_TBL[cur].evolvesTo == 0) break;
      cur = DEX_TBL[cur].evolvesTo;
    }
  }
  for (int d = 1; d <= N; d++)
    CHECK_MSG(reach[d], std::string("inalcanzable: ") + DEX_TBL[d].name);
}

// con la despedida a los 3 dias (nivel 72), toda linea tiene que poder llegar a
// su forma final antes de que la mascota cumpla el ciclo
TEST(dex, toda_linea_se_completa_antes_de_la_despedida) {
  const uint32_t maxLevel = FAREWELL_AGE_MIN / MINUTES_PER_LEVEL;
  for (int d = 1; d <= N; d++) {
    if (DEX_TBL[d].rarity == R_EVO) continue;
    int cur = d, guard = 0;
    uint32_t need = 0;
    while (DEX_TBL[cur].evolvesTo != 0 && guard++ < 6) {
      if (DEX_TBL[cur].evolveLevel > need) need = DEX_TBL[cur].evolveLevel;
      cur = DEX_TBL[cur].evolvesTo;
    }
    CHECK_MSG(need <= maxLevel,
              std::string("la linea de ") + DEX_TBL[d].name + " no llega a forma final a tiempo");
  }
}

TEST(dex, las_stats_base_son_plausibles) {
  for (int d = 1; d <= N; d++) {
    const DexEntry &e = DEX_TBL[d];
    CHECK_MSG(e.bHp > 0 && e.bAtk > 0 && e.bDef > 0 && e.bSpe > 0,
              std::string("stat base a cero en ") + e.name);
    CHECK_MSG(e.biome <= 5, std::string("bioma desconocido en ") + e.name);
    CHECK_MSG(e.rarity <= R_LEGENDARIO, std::string("rareza desconocida en ") + e.name);
  }
}

TEST(dex, la_rama_de_eevee_cuadra_con_el_codigo) {
  CHECK_EQ((int)DEX_TBL[DEX_EEVEE].rarity, (int)R_COMUN);
  CHECK_RANGE((int)DEX_TBL[DEX_EEVEE].evolvesTo, 134, 136);
  for (int d = 134; d <= 136; d++) {
    CHECK_EQ((int)DEX_TBL[d].rarity, (int)R_EVO);
    CHECK_EQ((int)DEX_TBL[d].evolvesTo, 0);
  }
}

TEST(dex, los_iniciales_clasicos_son_validos) {
  CHECK_EQ((int)(sizeof(CLASSIC_DEX) / sizeof(CLASSIC_DEX[0])), NUM_CLASSIC_DEX);
  for (int i = 0; i < NUM_CLASSIC_DEX; i++) {
    int16_t d = CLASSIC_DEX[i];
    CHECK_RANGE(d, (int16_t)1, (int16_t)151);
    CHECK_MSG(DEX_TBL[d].rarity != R_EVO, "un inicial no puede ser una forma evolucionada");
  }
}

TEST(dex, hay_al_menos_una_especie_de_cada_rareza) {
  int count[4] = { 0, 0, 0, 0 };
  for (int d = 1; d <= N; d++) count[DEX_TBL[d].rarity]++;
  for (int r = 0; r <= R_LEGENDARIO; r++)
    CHECK_MSG(count[r] > 0, "hay una rareza sin especies: el sorteo del huevo se quedaria sin candidatos");
}

// pickEggSpecies mete los candidatos de un tier en un array de 80: si algun
// tier tuviera mas, se sortearia solo entre los 80 primeros
TEST(dex, ningun_tier_desborda_el_array_de_candidatos) {
  for (int r = R_COMUN; r <= R_LEGENDARIO; r++) {
    int n = 0;
    for (int d = 1; d <= N; d++)
      if (DEX_TBL[d].rarity == r) n++;
    CHECK_MSG(n <= 80, "mas de 80 candidatos en un tier: pickEggSpecies los recorta");
  }
}

TEST(dex, dexName_cae_al_nombre_base_si_no_hay_traduccion) {
  gLang = LANG_ES;
  CHECK_STREQ(dexName(1), DEX_TBL[1].name);
  gLang = LANG_FR;
  for (int d = 1; d <= N; d++) {
    const char *want = DEX_NAME_FR[d] ? DEX_NAME_FR[d] : DEX_TBL[d].name;
    CHECK_STREQ(dexName(d), want);
  }
  gLang = LANG_DE;
  for (int d = 1; d <= N; d++) {
    const char *want = DEX_NAME_DE[d] ? DEX_NAME_DE[d] : DEX_TBL[d].name;
    CHECK_STREQ(dexName(d), want);
  }
  gLang = LANG_DEFAULT;
}

TEST(dex, dexName_fuera_de_rango_no_revienta) {
  for (int lang = 0; lang < LANG_COUNT; lang++) {
    gLang = (Lang)lang;
    CHECK_STREQ(dexName(0), DEX_TBL[0].name);
    CHECK_STREQ(dexName(-1), DEX_TBL[0].name);
    CHECK_STREQ(dexName(152), DEX_TBL[0].name);
    CHECK_STREQ(dexName(30000), DEX_TBL[0].name);
  }
  gLang = LANG_DEFAULT;
}
