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
  uint8_t bHp, bAtk, bDef, bSpe;  // base stats reales de gen 1
};

static const DexEntry DEX_TBL[DEX_COUNT + 1] = {
  { "?", 0, 0, 0, 0x2946, 50, 50, 50, 50 },  // 0: sin usar
  { "BULBASAUR", 2, 16, R_COMUN, 0x3C49, 45, 49, 49, 45 },  // 1 planta
  { "IVYSAUR", 3, 32, R_EVO, 0x3C49, 60, 62, 63, 60 },  // 2 planta
  { "VENUSAUR", 0, 0, R_EVO, 0x3C49, 80, 82, 83, 80 },  // 3 planta
  { "CHARMANDER", 5, 16, R_COMUN, 0xEA87, 39, 52, 43, 65 },  // 4 fuego
  { "CHARMELEON", 6, 36, R_EVO, 0xEA87, 58, 64, 58, 80 },  // 5 fuego
  { "CHARIZARD", 0, 0, R_EVO, 0xEA87, 78, 84, 78, 100 },  // 6 fuego
  { "SQUIRTLE", 8, 16, R_COMUN, 0x4C98, 44, 48, 65, 43 },  // 7 agua
  { "WARTORTLE", 9, 36, R_EVO, 0x4C98, 59, 63, 80, 58 },  // 8 agua
  { "BLASTOISE", 0, 0, R_EVO, 0x4C98, 79, 83, 100, 78 },  // 9 agua
  { "CATERPIE", 11, 7, R_COMUN, 0x7CC4, 45, 30, 35, 45 },  // 10 bicho
  { "METAPOD", 12, 10, R_EVO, 0x7CC4, 50, 20, 55, 30 },  // 11 bicho
  { "BUTTERFREE", 0, 0, R_EVO, 0x7CC4, 60, 45, 50, 70 },  // 12 bicho
  { "WEEDLE", 14, 7, R_COMUN, 0x7CC4, 40, 35, 30, 50 },  // 13 bicho
  { "KAKUNA", 15, 10, R_EVO, 0x7CC4, 45, 25, 50, 35 },  // 14 bicho
  { "BEEDRILL", 0, 0, R_EVO, 0x7CC4, 65, 90, 40, 75 },  // 15 bicho
  { "PIDGEY", 17, 18, R_COMUN, 0x8C4D, 40, 45, 40, 56 },  // 16 normal
  { "PIDGEOTTO", 18, 36, R_EVO, 0x8C4D, 63, 60, 55, 71 },  // 17 normal
  { "PIDGEOT", 0, 0, R_EVO, 0x8C4D, 83, 80, 75, 101 },  // 18 normal
  { "RATTATA", 20, 20, R_COMUN, 0x8C4D, 30, 56, 35, 72 },  // 19 normal
  { "RATICATE", 0, 0, R_EVO, 0x8C4D, 55, 81, 60, 97 },  // 20 normal
  { "SPEAROW", 22, 20, R_COMUN, 0x8C4D, 40, 60, 30, 70 },  // 21 normal
  { "FEAROW", 0, 0, R_EVO, 0x8C4D, 65, 90, 65, 100 },  // 22 normal
  { "EKANS", 24, 22, R_COMUN, 0x8A73, 35, 60, 44, 55 },  // 23 veneno
  { "ARBOK", 0, 0, R_EVO, 0x8A73, 60, 95, 69, 80 },  // 24 veneno
  { "PIKACHU", 26, 30, R_COMUN, 0xBCA1, 35, 55, 40, 90 },  // 25 electrico
  { "RAICHU", 0, 0, R_EVO, 0xBCA1, 60, 90, 55, 110 },  // 26 electrico
  { "SANDSHREW", 28, 22, R_COMUN, 0xB447, 50, 75, 85, 40 },  // 27 tierra
  { "SANDSLASH", 0, 0, R_EVO, 0xB447, 75, 100, 110, 65 },  // 28 tierra
  { "NIDORAN H", 30, 16, R_COMUN, 0x8A73, 55, 47, 52, 41 },  // 29 veneno
  { "NIDORINA", 31, 30, R_EVO, 0x8A73, 70, 62, 67, 56 },  // 30 veneno
  { "NIDOQUEEN", 0, 0, R_EVO, 0x8A73, 90, 92, 87, 76 },  // 31 veneno
  { "NIDORAN M", 33, 16, R_COMUN, 0x8A73, 46, 57, 40, 50 },  // 32 veneno
  { "NIDORINO", 34, 30, R_EVO, 0x8A73, 61, 72, 57, 65 },  // 33 veneno
  { "NIDOKING", 0, 0, R_EVO, 0x8A73, 81, 102, 77, 85 },  // 34 veneno
  { "CLEFAIRY", 36, 30, R_COMUN, 0x8C4D, 70, 45, 48, 35 },  // 35 normal
  { "CLEFABLE", 0, 0, R_EVO, 0x8C4D, 95, 70, 73, 60 },  // 36 normal
  { "VULPIX", 38, 30, R_COMUN, 0xEA87, 38, 41, 40, 65 },  // 37 fuego
  { "NINETALES", 0, 0, R_EVO, 0xEA87, 73, 76, 75, 100 },  // 38 fuego
  { "JIGGLYPUFF", 40, 30, R_COMUN, 0x8C4D, 115, 45, 20, 20 },  // 39 normal
  { "WIGGLYTUFF", 0, 0, R_EVO, 0x8C4D, 140, 70, 45, 45 },  // 40 normal
  { "ZUBAT", 42, 22, R_COMUN, 0x8A73, 40, 45, 35, 55 },  // 41 veneno
  { "GOLBAT", 0, 0, R_EVO, 0x8A73, 75, 80, 70, 90 },  // 42 veneno
  { "ODDISH", 44, 21, R_COMUN, 0x3C49, 45, 50, 55, 30 },  // 43 planta
  { "GLOOM", 45, 36, R_EVO, 0x3C49, 60, 65, 70, 40 },  // 44 planta
  { "VILEPLUME", 0, 0, R_EVO, 0x3C49, 75, 80, 85, 50 },  // 45 planta
  { "PARAS", 47, 24, R_COMUN, 0x7CC4, 35, 70, 55, 25 },  // 46 bicho
  { "PARASECT", 0, 0, R_EVO, 0x7CC4, 60, 95, 80, 30 },  // 47 bicho
  { "VENONAT", 49, 31, R_COMUN, 0x7CC4, 60, 55, 50, 45 },  // 48 bicho
  { "VENOMOTH", 0, 0, R_EVO, 0x7CC4, 70, 65, 60, 90 },  // 49 bicho
  { "DIGLETT", 51, 26, R_COMUN, 0xB447, 10, 55, 25, 95 },  // 50 tierra
  { "DUGTRIO", 0, 0, R_EVO, 0xB447, 35, 100, 50, 120 },  // 51 tierra
  { "MEOWTH", 53, 28, R_COMUN, 0x8C4D, 40, 45, 35, 90 },  // 52 normal
  { "PERSIAN", 0, 0, R_EVO, 0x8C4D, 65, 70, 60, 115 },  // 53 normal
  { "PSYDUCK", 55, 33, R_COMUN, 0x4C98, 50, 52, 48, 55 },  // 54 agua
  { "GOLDUCK", 0, 0, R_EVO, 0x4C98, 80, 82, 78, 85 },  // 55 agua
  { "MANKEY", 57, 28, R_COMUN, 0xA2A5, 40, 80, 35, 70 },  // 56 lucha
  { "PRIMEAPE", 0, 0, R_EVO, 0xA2A5, 65, 105, 60, 95 },  // 57 lucha
  { "GROWLITHE", 59, 30, R_RARO, 0xEA87, 55, 70, 45, 60 },  // 58 fuego
  { "ARCANINE", 0, 0, R_EVO, 0xEA87, 90, 110, 80, 95 },  // 59 fuego
  { "POLIWAG", 61, 25, R_COMUN, 0x4C98, 40, 50, 40, 90 },  // 60 agua
  { "POLIWHIRL", 62, 36, R_EVO, 0x4C98, 65, 65, 65, 90 },  // 61 agua
  { "POLIWRATH", 0, 0, R_EVO, 0x4C98, 90, 95, 95, 70 },  // 62 agua
  { "ABRA", 64, 16, R_COMUN, 0xD28F, 25, 20, 15, 90 },  // 63 psiquico
  { "KADABRA", 65, 40, R_EVO, 0xD28F, 40, 35, 30, 105 },  // 64 psiquico
  { "ALAKAZAM", 0, 0, R_EVO, 0xD28F, 55, 50, 45, 120 },  // 65 psiquico
  { "MACHOP", 67, 28, R_COMUN, 0xA2A5, 70, 80, 50, 35 },  // 66 lucha
  { "MACHOKE", 68, 40, R_EVO, 0xA2A5, 80, 100, 70, 45 },  // 67 lucha
  { "MACHAMP", 0, 0, R_EVO, 0xA2A5, 90, 130, 80, 55 },  // 68 lucha
  { "BELLSPROUT", 70, 21, R_COMUN, 0x3C49, 50, 75, 35, 40 },  // 69 planta
  { "WEEPINBELL", 71, 36, R_EVO, 0x3C49, 65, 90, 50, 55 },  // 70 planta
  { "VICTREEBEL", 0, 0, R_EVO, 0x3C49, 80, 105, 65, 70 },  // 71 planta
  { "TENTACOOL", 73, 30, R_COMUN, 0x4C98, 40, 40, 35, 70 },  // 72 agua
  { "TENTACRUEL", 0, 0, R_EVO, 0x4C98, 80, 70, 65, 100 },  // 73 agua
  { "GEODUDE", 75, 25, R_COMUN, 0x9407, 40, 80, 100, 20 },  // 74 roca
  { "GRAVELER", 76, 40, R_EVO, 0x9407, 55, 95, 115, 35 },  // 75 roca
  { "GOLEM", 0, 0, R_EVO, 0x9407, 80, 120, 130, 45 },  // 76 roca
  { "PONYTA", 78, 40, R_RARO, 0xEA87, 50, 85, 55, 90 },  // 77 fuego
  { "RAPIDASH", 0, 0, R_EVO, 0xEA87, 65, 100, 70, 105 },  // 78 fuego
  { "SLOWPOKE", 80, 37, R_COMUN, 0x4C98, 90, 65, 65, 15 },  // 79 agua
  { "SLOWBRO", 0, 0, R_EVO, 0x4C98, 95, 75, 110, 30 },  // 80 agua
  { "MAGNEMITE", 82, 30, R_COMUN, 0xBCA1, 25, 35, 70, 45 },  // 81 electrico
  { "MAGNETON", 0, 0, R_EVO, 0xBCA1, 50, 60, 95, 70 },  // 82 electrico
  { "FARFETCHD", 0, 0, R_RARO, 0x8C4D, 52, 90, 55, 60 },  // 83 normal
  { "DODUO", 85, 31, R_COMUN, 0x8C4D, 35, 85, 45, 75 },  // 84 normal
  { "DODRIO", 0, 0, R_EVO, 0x8C4D, 60, 110, 70, 110 },  // 85 normal
  { "SEEL", 87, 34, R_COMUN, 0x4C98, 65, 45, 55, 45 },  // 86 agua
  { "DEWGONG", 0, 0, R_EVO, 0x4C98, 90, 70, 80, 70 },  // 87 agua
  { "GRIMER", 89, 38, R_RARO, 0x8A73, 80, 80, 50, 25 },  // 88 veneno
  { "MUK", 0, 0, R_EVO, 0x8A73, 105, 105, 75, 50 },  // 89 veneno
  { "SHELLDER", 91, 30, R_COMUN, 0x4C98, 30, 65, 100, 40 },  // 90 agua
  { "CLOYSTER", 0, 0, R_EVO, 0x4C98, 50, 95, 180, 70 },  // 91 agua
  { "GASTLY", 93, 25, R_COMUN, 0x6AD3, 30, 35, 30, 80 },  // 92 fantasma
  { "HAUNTER", 94, 40, R_EVO, 0x6AD3, 45, 50, 45, 95 },  // 93 fantasma
  { "GENGAR", 0, 0, R_EVO, 0x6AD3, 60, 65, 60, 110 },  // 94 fantasma
  { "ONIX", 0, 0, R_RARO, 0x9407, 35, 45, 160, 70 },  // 95 roca
  { "DROWZEE", 97, 26, R_COMUN, 0xD28F, 60, 48, 45, 42 },  // 96 psiquico
  { "HYPNO", 0, 0, R_EVO, 0xD28F, 85, 73, 70, 67 },  // 97 psiquico
  { "KRABBY", 99, 28, R_COMUN, 0x4C98, 30, 105, 90, 50 },  // 98 agua
  { "KINGLER", 0, 0, R_EVO, 0x4C98, 55, 130, 115, 75 },  // 99 agua
  { "VOLTORB", 101, 30, R_COMUN, 0xBCA1, 40, 30, 50, 100 },  // 100 electrico
  { "ELECTRODE", 0, 0, R_EVO, 0xBCA1, 60, 50, 70, 150 },  // 101 electrico
  { "EXEGGCUTE", 103, 30, R_COMUN, 0x3C49, 60, 40, 80, 40 },  // 102 planta
  { "EXEGGUTOR", 0, 0, R_EVO, 0x3C49, 95, 95, 85, 55 },  // 103 planta
  { "CUBONE", 105, 28, R_COMUN, 0xB447, 50, 50, 95, 35 },  // 104 tierra
  { "MAROWAK", 0, 0, R_EVO, 0xB447, 60, 80, 110, 45 },  // 105 tierra
  { "HITMONLEE", 0, 0, R_RARO, 0xA2A5, 50, 120, 53, 87 },  // 106 lucha
  { "HITMONCHAN", 0, 0, R_RARO, 0xA2A5, 50, 105, 79, 76 },  // 107 lucha
  { "LICKITUNG", 0, 0, R_RARO, 0x8C4D, 90, 55, 75, 30 },  // 108 normal
  { "KOFFING", 110, 35, R_COMUN, 0x8A73, 40, 65, 95, 35 },  // 109 veneno
  { "WEEZING", 0, 0, R_EVO, 0x8A73, 65, 90, 120, 60 },  // 110 veneno
  { "RHYHORN", 112, 42, R_RARO, 0xB447, 80, 85, 95, 25 },  // 111 tierra
  { "RHYDON", 0, 0, R_EVO, 0xB447, 105, 130, 120, 40 },  // 112 tierra
  { "CHANSEY", 0, 0, R_RARO, 0x8C4D, 250, 5, 5, 50 },  // 113 normal
  { "TANGELA", 0, 0, R_RARO, 0x3C49, 65, 55, 115, 60 },  // 114 planta
  { "KANGASKHAN", 0, 0, R_RARO, 0x8C4D, 105, 95, 80, 90 },  // 115 normal
  { "HORSEA", 117, 32, R_COMUN, 0x4C98, 30, 40, 70, 60 },  // 116 agua
  { "SEADRA", 0, 0, R_EVO, 0x4C98, 55, 65, 95, 85 },  // 117 agua
  { "GOLDEEN", 119, 33, R_COMUN, 0x4C98, 45, 67, 60, 63 },  // 118 agua
  { "SEAKING", 0, 0, R_EVO, 0x4C98, 80, 92, 65, 68 },  // 119 agua
  { "STARYU", 121, 30, R_COMUN, 0x4C98, 30, 45, 55, 85 },  // 120 agua
  { "STARMIE", 0, 0, R_EVO, 0x4C98, 60, 75, 85, 115 },  // 121 agua
  { "MR. MIME", 0, 0, R_RARO, 0xD28F, 40, 45, 65, 90 },  // 122 psiquico
  { "SCYTHER", 0, 0, R_RARO, 0x7CC4, 70, 110, 80, 105 },  // 123 bicho
  { "JYNX", 0, 0, R_RARO, 0x4DB8, 65, 50, 35, 95 },  // 124 hielo
  { "ELECTABUZZ", 0, 0, R_RARO, 0xBCA1, 65, 83, 57, 105 },  // 125 electrico
  { "MAGMAR", 0, 0, R_RARO, 0xEA87, 65, 95, 57, 93 },  // 126 fuego
  { "PINSIR", 0, 0, R_RARO, 0x7CC4, 65, 125, 100, 85 },  // 127 bicho
  { "TAUROS", 0, 0, R_RARO, 0x8C4D, 75, 100, 95, 110 },  // 128 normal
  { "MAGIKARP", 130, 20, R_COMUN, 0x4C98, 20, 10, 55, 80 },  // 129 agua
  { "GYARADOS", 0, 0, R_EVO, 0x4C98, 95, 125, 79, 81 },  // 130 agua
  { "LAPRAS", 0, 0, R_RARO, 0x4C98, 130, 85, 80, 60 },  // 131 agua
  { "DITTO", 0, 0, R_RARO, 0x8C4D, 48, 48, 48, 48 },  // 132 normal
  { "EEVEE", 134, 30, R_COMUN, 0x8C4D, 55, 55, 50, 55 },  // 133 normal
  { "VAPOREON", 0, 0, R_EVO, 0x4C98, 130, 65, 60, 65 },  // 134 agua
  { "JOLTEON", 0, 0, R_EVO, 0xBCA1, 65, 65, 60, 130 },  // 135 electrico
  { "FLAREON", 0, 0, R_EVO, 0xEA87, 65, 130, 60, 65 },  // 136 fuego
  { "PORYGON", 0, 0, R_RARO, 0x8C4D, 65, 60, 70, 40 },  // 137 normal
  { "OMANYTE", 139, 40, R_RARO, 0x9407, 35, 40, 100, 35 },  // 138 roca
  { "OMASTAR", 0, 0, R_EVO, 0x9407, 70, 60, 125, 55 },  // 139 roca
  { "KABUTO", 141, 40, R_RARO, 0x9407, 30, 80, 90, 55 },  // 140 roca
  { "KABUTOPS", 0, 0, R_EVO, 0x9407, 60, 115, 105, 80 },  // 141 roca
  { "AERODACTYL", 0, 0, R_RARO, 0x9407, 80, 105, 65, 130 },  // 142 roca
  { "SNORLAX", 0, 0, R_RARO, 0x8C4D, 160, 110, 65, 30 },  // 143 normal
  { "ARTICUNO", 0, 0, R_LEGENDARIO, 0x4DB8, 90, 85, 100, 85 },  // 144 hielo
  { "ZAPDOS", 0, 0, R_LEGENDARIO, 0xBCA1, 90, 90, 85, 100 },  // 145 electrico
  { "MOLTRES", 0, 0, R_LEGENDARIO, 0xEA87, 90, 100, 90, 90 },  // 146 fuego
  { "DRATINI", 148, 30, R_RARO, 0x5A98, 41, 64, 45, 50 },  // 147 dragon
  { "DRAGONAIR", 149, 55, R_EVO, 0x5A98, 61, 84, 65, 70 },  // 148 dragon
  { "DRAGONITE", 0, 0, R_EVO, 0x5A98, 91, 134, 95, 80 },  // 149 dragon
  { "MEWTWO", 0, 0, R_LEGENDARIO, 0xD28F, 106, 110, 90, 130 },  // 150 psiquico
  { "MEW", 0, 0, R_LEGENDARIO, 0xD28F, 100, 100, 100, 100 },  // 151 psiquico
};

// el primer huevo de la partida: iniciales clasicos
static const int16_t CLASSIC_DEX[] = { 1, 4, 7, 25, 133 };
#define NUM_CLASSIC_DEX 5
