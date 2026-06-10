#pragma once
#include <stdint.h>

// GENERADO por tools/gen_dex.py desde tools/dex_data.py - no editar

#define DEX_COUNT 151
#define DEX_EEVEE 133  // rama al azar: 134/135/136

// rareza: 0 = solo por evolucion, 1 = comun, 2 = raro, 3 = legendario
enum : uint8_t { R_EVO = 0, R_COMUN, R_RARO, R_LEGENDARIO };

struct DexEntry {
  const char *name;
  uint8_t evolvesTo;    // numero de dex, 0 = forma final
  uint8_t evolveLevel;
  uint8_t rarity;       // sale de huevo si > 0
  uint16_t accent;      // color RGB565 del tipo para la UI
};

static const DexEntry DEX_TBL[DEX_COUNT + 1] = {
  { "?", 0, 0, 0, 0x2946 },  // 0: sin usar
  { "BULBASAUR", 2, 16, R_COMUN, 0x3C49 },  // 1 planta
  { "IVYSAUR", 3, 32, R_EVO, 0x3C49 },  // 2 planta
  { "VENUSAUR", 0, 0, R_EVO, 0x3C49 },  // 3 planta
  { "CHARMANDER", 5, 16, R_COMUN, 0xEA87 },  // 4 fuego
  { "CHARMELEON", 6, 36, R_EVO, 0xEA87 },  // 5 fuego
  { "CHARIZARD", 0, 0, R_EVO, 0xEA87 },  // 6 fuego
  { "SQUIRTLE", 8, 16, R_COMUN, 0x4C98 },  // 7 agua
  { "WARTORTLE", 9, 36, R_EVO, 0x4C98 },  // 8 agua
  { "BLASTOISE", 0, 0, R_EVO, 0x4C98 },  // 9 agua
  { "CATERPIE", 11, 7, R_COMUN, 0x7CC4 },  // 10 bicho
  { "METAPOD", 12, 10, R_EVO, 0x7CC4 },  // 11 bicho
  { "BUTTERFREE", 0, 0, R_EVO, 0x7CC4 },  // 12 bicho
  { "WEEDLE", 14, 7, R_COMUN, 0x7CC4 },  // 13 bicho
  { "KAKUNA", 15, 10, R_EVO, 0x7CC4 },  // 14 bicho
  { "BEEDRILL", 0, 0, R_EVO, 0x7CC4 },  // 15 bicho
  { "PIDGEY", 17, 18, R_COMUN, 0x8C4D },  // 16 normal
  { "PIDGEOTTO", 18, 36, R_EVO, 0x8C4D },  // 17 normal
  { "PIDGEOT", 0, 0, R_EVO, 0x8C4D },  // 18 normal
  { "RATTATA", 20, 20, R_COMUN, 0x8C4D },  // 19 normal
  { "RATICATE", 0, 0, R_EVO, 0x8C4D },  // 20 normal
  { "SPEAROW", 22, 20, R_COMUN, 0x8C4D },  // 21 normal
  { "FEAROW", 0, 0, R_EVO, 0x8C4D },  // 22 normal
  { "EKANS", 24, 22, R_COMUN, 0x8A73 },  // 23 veneno
  { "ARBOK", 0, 0, R_EVO, 0x8A73 },  // 24 veneno
  { "PIKACHU", 26, 30, R_COMUN, 0xBCA1 },  // 25 electrico
  { "RAICHU", 0, 0, R_EVO, 0xBCA1 },  // 26 electrico
  { "SANDSHREW", 28, 22, R_COMUN, 0xB447 },  // 27 tierra
  { "SANDSLASH", 0, 0, R_EVO, 0xB447 },  // 28 tierra
  { "NIDORAN H", 30, 16, R_COMUN, 0x8A73 },  // 29 veneno
  { "NIDORINA", 31, 30, R_EVO, 0x8A73 },  // 30 veneno
  { "NIDOQUEEN", 0, 0, R_EVO, 0x8A73 },  // 31 veneno
  { "NIDORAN M", 33, 16, R_COMUN, 0x8A73 },  // 32 veneno
  { "NIDORINO", 34, 30, R_EVO, 0x8A73 },  // 33 veneno
  { "NIDOKING", 0, 0, R_EVO, 0x8A73 },  // 34 veneno
  { "CLEFAIRY", 36, 30, R_COMUN, 0x8C4D },  // 35 normal
  { "CLEFABLE", 0, 0, R_EVO, 0x8C4D },  // 36 normal
  { "VULPIX", 38, 30, R_COMUN, 0xEA87 },  // 37 fuego
  { "NINETALES", 0, 0, R_EVO, 0xEA87 },  // 38 fuego
  { "JIGGLYPUFF", 40, 30, R_COMUN, 0x8C4D },  // 39 normal
  { "WIGGLYTUFF", 0, 0, R_EVO, 0x8C4D },  // 40 normal
  { "ZUBAT", 42, 22, R_COMUN, 0x8A73 },  // 41 veneno
  { "GOLBAT", 0, 0, R_EVO, 0x8A73 },  // 42 veneno
  { "ODDISH", 44, 21, R_COMUN, 0x3C49 },  // 43 planta
  { "GLOOM", 45, 36, R_EVO, 0x3C49 },  // 44 planta
  { "VILEPLUME", 0, 0, R_EVO, 0x3C49 },  // 45 planta
  { "PARAS", 47, 24, R_COMUN, 0x7CC4 },  // 46 bicho
  { "PARASECT", 0, 0, R_EVO, 0x7CC4 },  // 47 bicho
  { "VENONAT", 49, 31, R_COMUN, 0x7CC4 },  // 48 bicho
  { "VENOMOTH", 0, 0, R_EVO, 0x7CC4 },  // 49 bicho
  { "DIGLETT", 51, 26, R_COMUN, 0xB447 },  // 50 tierra
  { "DUGTRIO", 0, 0, R_EVO, 0xB447 },  // 51 tierra
  { "MEOWTH", 53, 28, R_COMUN, 0x8C4D },  // 52 normal
  { "PERSIAN", 0, 0, R_EVO, 0x8C4D },  // 53 normal
  { "PSYDUCK", 55, 33, R_COMUN, 0x4C98 },  // 54 agua
  { "GOLDUCK", 0, 0, R_EVO, 0x4C98 },  // 55 agua
  { "MANKEY", 57, 28, R_COMUN, 0xA2A5 },  // 56 lucha
  { "PRIMEAPE", 0, 0, R_EVO, 0xA2A5 },  // 57 lucha
  { "GROWLITHE", 59, 30, R_RARO, 0xEA87 },  // 58 fuego
  { "ARCANINE", 0, 0, R_EVO, 0xEA87 },  // 59 fuego
  { "POLIWAG", 61, 25, R_COMUN, 0x4C98 },  // 60 agua
  { "POLIWHIRL", 62, 36, R_EVO, 0x4C98 },  // 61 agua
  { "POLIWRATH", 0, 0, R_EVO, 0x4C98 },  // 62 agua
  { "ABRA", 64, 16, R_COMUN, 0xD28F },  // 63 psiquico
  { "KADABRA", 65, 40, R_EVO, 0xD28F },  // 64 psiquico
  { "ALAKAZAM", 0, 0, R_EVO, 0xD28F },  // 65 psiquico
  { "MACHOP", 67, 28, R_COMUN, 0xA2A5 },  // 66 lucha
  { "MACHOKE", 68, 40, R_EVO, 0xA2A5 },  // 67 lucha
  { "MACHAMP", 0, 0, R_EVO, 0xA2A5 },  // 68 lucha
  { "BELLSPROUT", 70, 21, R_COMUN, 0x3C49 },  // 69 planta
  { "WEEPINBELL", 71, 36, R_EVO, 0x3C49 },  // 70 planta
  { "VICTREEBEL", 0, 0, R_EVO, 0x3C49 },  // 71 planta
  { "TENTACOOL", 73, 30, R_COMUN, 0x4C98 },  // 72 agua
  { "TENTACRUEL", 0, 0, R_EVO, 0x4C98 },  // 73 agua
  { "GEODUDE", 75, 25, R_COMUN, 0x9407 },  // 74 roca
  { "GRAVELER", 76, 40, R_EVO, 0x9407 },  // 75 roca
  { "GOLEM", 0, 0, R_EVO, 0x9407 },  // 76 roca
  { "PONYTA", 78, 40, R_RARO, 0xEA87 },  // 77 fuego
  { "RAPIDASH", 0, 0, R_EVO, 0xEA87 },  // 78 fuego
  { "SLOWPOKE", 80, 37, R_COMUN, 0x4C98 },  // 79 agua
  { "SLOWBRO", 0, 0, R_EVO, 0x4C98 },  // 80 agua
  { "MAGNEMITE", 82, 30, R_COMUN, 0xBCA1 },  // 81 electrico
  { "MAGNETON", 0, 0, R_EVO, 0xBCA1 },  // 82 electrico
  { "FARFETCHD", 0, 0, R_RARO, 0x8C4D },  // 83 normal
  { "DODUO", 85, 31, R_COMUN, 0x8C4D },  // 84 normal
  { "DODRIO", 0, 0, R_EVO, 0x8C4D },  // 85 normal
  { "SEEL", 87, 34, R_COMUN, 0x4C98 },  // 86 agua
  { "DEWGONG", 0, 0, R_EVO, 0x4C98 },  // 87 agua
  { "GRIMER", 89, 38, R_RARO, 0x8A73 },  // 88 veneno
  { "MUK", 0, 0, R_EVO, 0x8A73 },  // 89 veneno
  { "SHELLDER", 91, 30, R_COMUN, 0x4C98 },  // 90 agua
  { "CLOYSTER", 0, 0, R_EVO, 0x4C98 },  // 91 agua
  { "GASTLY", 93, 25, R_COMUN, 0x6AD3 },  // 92 fantasma
  { "HAUNTER", 94, 40, R_EVO, 0x6AD3 },  // 93 fantasma
  { "GENGAR", 0, 0, R_EVO, 0x6AD3 },  // 94 fantasma
  { "ONIX", 0, 0, R_RARO, 0x9407 },  // 95 roca
  { "DROWZEE", 97, 26, R_COMUN, 0xD28F },  // 96 psiquico
  { "HYPNO", 0, 0, R_EVO, 0xD28F },  // 97 psiquico
  { "KRABBY", 99, 28, R_COMUN, 0x4C98 },  // 98 agua
  { "KINGLER", 0, 0, R_EVO, 0x4C98 },  // 99 agua
  { "VOLTORB", 101, 30, R_COMUN, 0xBCA1 },  // 100 electrico
  { "ELECTRODE", 0, 0, R_EVO, 0xBCA1 },  // 101 electrico
  { "EXEGGCUTE", 103, 30, R_COMUN, 0x3C49 },  // 102 planta
  { "EXEGGUTOR", 0, 0, R_EVO, 0x3C49 },  // 103 planta
  { "CUBONE", 105, 28, R_COMUN, 0xB447 },  // 104 tierra
  { "MAROWAK", 0, 0, R_EVO, 0xB447 },  // 105 tierra
  { "HITMONLEE", 0, 0, R_RARO, 0xA2A5 },  // 106 lucha
  { "HITMONCHAN", 0, 0, R_RARO, 0xA2A5 },  // 107 lucha
  { "LICKITUNG", 0, 0, R_RARO, 0x8C4D },  // 108 normal
  { "KOFFING", 110, 35, R_COMUN, 0x8A73 },  // 109 veneno
  { "WEEZING", 0, 0, R_EVO, 0x8A73 },  // 110 veneno
  { "RHYHORN", 112, 42, R_RARO, 0xB447 },  // 111 tierra
  { "RHYDON", 0, 0, R_EVO, 0xB447 },  // 112 tierra
  { "CHANSEY", 0, 0, R_RARO, 0x8C4D },  // 113 normal
  { "TANGELA", 0, 0, R_RARO, 0x3C49 },  // 114 planta
  { "KANGASKHAN", 0, 0, R_RARO, 0x8C4D },  // 115 normal
  { "HORSEA", 117, 32, R_COMUN, 0x4C98 },  // 116 agua
  { "SEADRA", 0, 0, R_EVO, 0x4C98 },  // 117 agua
  { "GOLDEEN", 119, 33, R_COMUN, 0x4C98 },  // 118 agua
  { "SEAKING", 0, 0, R_EVO, 0x4C98 },  // 119 agua
  { "STARYU", 121, 30, R_COMUN, 0x4C98 },  // 120 agua
  { "STARMIE", 0, 0, R_EVO, 0x4C98 },  // 121 agua
  { "MR. MIME", 0, 0, R_RARO, 0xD28F },  // 122 psiquico
  { "SCYTHER", 0, 0, R_RARO, 0x7CC4 },  // 123 bicho
  { "JYNX", 0, 0, R_RARO, 0x4DB8 },  // 124 hielo
  { "ELECTABUZZ", 0, 0, R_RARO, 0xBCA1 },  // 125 electrico
  { "MAGMAR", 0, 0, R_RARO, 0xEA87 },  // 126 fuego
  { "PINSIR", 0, 0, R_RARO, 0x7CC4 },  // 127 bicho
  { "TAUROS", 0, 0, R_RARO, 0x8C4D },  // 128 normal
  { "MAGIKARP", 130, 20, R_COMUN, 0x4C98 },  // 129 agua
  { "GYARADOS", 0, 0, R_EVO, 0x4C98 },  // 130 agua
  { "LAPRAS", 0, 0, R_RARO, 0x4C98 },  // 131 agua
  { "DITTO", 0, 0, R_RARO, 0x8C4D },  // 132 normal
  { "EEVEE", 134, 30, R_COMUN, 0x8C4D },  // 133 normal
  { "VAPOREON", 0, 0, R_EVO, 0x4C98 },  // 134 agua
  { "JOLTEON", 0, 0, R_EVO, 0xBCA1 },  // 135 electrico
  { "FLAREON", 0, 0, R_EVO, 0xEA87 },  // 136 fuego
  { "PORYGON", 0, 0, R_RARO, 0x8C4D },  // 137 normal
  { "OMANYTE", 139, 40, R_RARO, 0x9407 },  // 138 roca
  { "OMASTAR", 0, 0, R_EVO, 0x9407 },  // 139 roca
  { "KABUTO", 141, 40, R_RARO, 0x9407 },  // 140 roca
  { "KABUTOPS", 0, 0, R_EVO, 0x9407 },  // 141 roca
  { "AERODACTYL", 0, 0, R_RARO, 0x9407 },  // 142 roca
  { "SNORLAX", 0, 0, R_RARO, 0x8C4D },  // 143 normal
  { "ARTICUNO", 0, 0, R_LEGENDARIO, 0x4DB8 },  // 144 hielo
  { "ZAPDOS", 0, 0, R_LEGENDARIO, 0xBCA1 },  // 145 electrico
  { "MOLTRES", 0, 0, R_LEGENDARIO, 0xEA87 },  // 146 fuego
  { "DRATINI", 148, 30, R_RARO, 0x5A98 },  // 147 dragon
  { "DRAGONAIR", 149, 55, R_EVO, 0x5A98 },  // 148 dragon
  { "DRAGONITE", 0, 0, R_EVO, 0x5A98 },  // 149 dragon
  { "MEWTWO", 0, 0, R_LEGENDARIO, 0xD28F },  // 150 psiquico
  { "MEW", 0, 0, R_LEGENDARIO, 0xD28F },  // 151 psiquico
};

// el primer huevo de la partida: iniciales clasicos
static const int16_t CLASSIC_DEX[] = { 1, 4, 7, 25, 133 };
#define NUM_CLASSIC_DEX 5
