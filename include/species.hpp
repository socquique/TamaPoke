#pragma once
#include <stdint.h>

// GENERADO por tools/sprites.py - edita alli y ejecuta:
//   python3 tools/sprites.py emit

#define SPRITE_W 32
#define SPRITE_H 32

#define UI_BG_DAY 0xF77C  // #f2efe1
#define UI_BG_NIGHT 0x10C5  // #141828
#define UI_INK 0x2946  // #2a2a36
#define UI_INK_NIGHT 0xDEFE  // #d8dcf0
#define UI_TRACK 0xDE97  // #d8d2bd
#define UI_BAR_OK 0x5DCD  // #58b868
#define UI_BAR_WARN 0xED07  // #e8a23c
#define UI_BAR_BAD 0xEA87  // #e8503a
#define UI_WHITE 0xFFFF  // #ffffff

enum ElementType : uint8_t { TYPE_FUEGO, TYPE_PLANTA, TYPE_AGUA };

enum : int8_t {
  SP_CHARMANDER = 0,
  SP_CHARMELEON,
  SP_CHARIZARD,
  SP_BULBASAUR,
  SP_IVYSAUR,
  SP_VENUSAUR,
  SP_SQUIRTLE,
  SP_WARTORTLE,
  SP_BLASTOISE,
  NUM_SPECIES
};

struct Species {
  const char *name;
  ElementType type;
  const char *const *sprite;
  uint8_t scale;
  int8_t evolvesTo;
  uint8_t evolveLevel;
  uint8_t eyeRow, eyeColL, eyeColR;  // anclas para expresiones (ojos 3x4)
  uint8_t mouthRow, mouthCol;
  uint16_t bodyColor;  // para borrar ojos/boca al expresar
  uint16_t accent;     // color UI del tipo
};

// caracter de sprite -> RGB565
static inline uint16_t spriteColor(char ch) {
  switch (ch) {
    case 'k': return 0x18C4;  // #1b1b25
    case 'w': return 0xFFFF;  // #ffffff
    case 'y': return 0xFED2;  // #f8d990
    case 'Y': return 0xE5CC;  // #e0b860
    case 'o': return 0xF427;  // #f5863d
    case 'O': return 0xD2E5;  // #d65f28
    case 'r': return 0xEA87;  // #e8503a
    case 'R': return 0xB184;  // #b53224
    case 'f': return 0xFECB;  // #ffd95e
    case 't': return 0x8EB6;  // #8fd6b4
    case 'T': return 0x5D71;  // #5fae8c
    case 'g': return 0x5DCD;  // #58b868
    case 'G': return 0x3C49;  // #3c8a4c
    case 'd': return 0x3BEC;  // #3f7e62
    case 'p': return 0xF454;  // #f08aa4
    case 'P': return 0xC2F0;  // #c75f80
    case 'b': return 0x7E3D;  // #7cc4ea
    case 'B': return 0x4C98;  // #4f93c4
    case 'N': return 0x3B74;  // #3a6fa0
    case 'M': return 0x2A8F;  // #2a5278
    case 'c': return 0xB3C8;  // #b07a45
    case 'C': return 0x7AA6;  // #7e5530
    case 'l': return 0x9D5C;  // #9aa9e0
    case 'L': return 0x6BF7;  // #6f7cb8
    case 's': return 0xAD97;  // #aab0bc
    case 'S': return 0x7BF1;  // #787e8c
    default: return 0;
  }
}

static const char* const SPR_CHARMANDER[32] = {  // 32x32
  "................................",
  "................................",
  "................................",
  "...........kkkkkkk..............",
  ".........kkoooooookk............",
  "........koooooooooook...........",
  ".......kokwkoooookwkok..........",
  ".......kowkkooooowkkok.....k....",
  "......kookkkoooookkkOOk....kk...",
  "......kookkkoooookkkOOk...krrk..",
  "......koooooooooooooOOk..krrrrk.",
  "......koooooooooooooOOk..krrrrk.",
  ".......kookoooookooOOk...kfffrk.",
  ".......koookkkkkooOOOk...kfffrk.",
  "........kooooooooOOOk....kfffrk.",
  ".........kkoOOOOOOkk......kffk..",
  "...........kOoooOk........kRk...",
  "..........koooooook.......kOk...",
  ".....kkk.koooooooook.kkk..kOk...",
  ".....kookoooyyyyyoookoookkOOk...",
  ".....koooooyyyyyyyooOkkkOOOOk...",
  ".....kooooyyyyyyyyYOk...kOOk....",
  ".....kkkooyyyyyyyyYOOk..kOOk....",
  "........koyyyyyyyyYOOokkOOOk....",
  "........koyyyyyyyYYOOoooOOkk....",
  "........kooyyyyyYYOOkooook......",
  ".........kooyYYYYOOk.kkkkk......",
  ".......kkoookOOOkoookk..........",
  ".......koook.kkk.koook..........",
  ".......koook.....koook..........",
  ".......kkkkk.....kkkkk..........",
  "................................",
};

static const char* const SPR_CHARMELEON[32] = {  // 32x32
  ".....................k..........",
  "....................kk..........",
  "...................kk...........",
  "...........kkkkk..krk...........",
  ".........kkrrrrrkkrkk...........",
  "........krrrrrrrrrk.............",
  ".......kkwkrrrrrkwkk......k.k...",
  "......krwkkrrrrrwkkrk......k....",
  "......krkkkrrrrrkkkRk......k....",
  "......krkkkrrrrrkkkRk....kkrkk..",
  "......krrrrrrrrrrrrRk...krrrrrk.",
  "......krrrrrrrrrrrRRk...krrrrrk.",
  "......krrkrrrrrkrrRRk...krffrrk.",
  ".......krrkkkkkrrRRk....krfffrk.",
  "........krrrrrrRRRk.....krfffrk.",
  ".........kkRRRRRkk......krfffRk.",
  "...........krrrk.........kRfkk..",
  "....kkk...krrrrrk...kkk.kRRk....",
  "....krk..krrrrrrrk..krk.kRRk....",
  "....krk.krryyyyyrrk.krk.kRRk....",
  "....krrkrryyyyyyyrRkkkk.kRRk....",
  "....krrrrryyyyyyyrRk....kRRk....",
  "....kkkrryyyyyyyYYRk.kkkRRRk....",
  ".......kryyyyyyyYYRk.krrRRkk....",
  ".......krryyyyyyYRRk.krrrk......",
  "........kryyyyyYYRk..kkkkk......",
  ".........kryYYYYRk..............",
  "......kkkrrkRRRkrrkkk...........",
  "......krrrk.kkk.krrrk...........",
  "......krrrk.....krrrk...........",
  "......kkkkk.....kkkkk...........",
  "................................",
};

static const char* const SPR_CHARIZARD[32] = {  // 32x32
  "................................",
  "..........kk.......kk...........",
  "..........kk.......kk...........",
  ".k........kk..kkk..kk.........k.",
  ".kkk.......kkkoookkk........kkk.",
  ".kTTk.....koooooooook......kTTk.",
  ".kTTTk...kkwkoooookwkk....kTTTk.",
  "..kTTTk..kwkkooooowkkk...kTTTk..",
  "..kTTTTk.kkkkoooookkkk..kTTTTk..",
  "..kTTTTTkokkkoooookkkOk.kTTTTk..",
  "...kTTTTToooooooooooOk.kTTTTk...",
  "...kTTTTToooooooooooOk.kTTTTk...",
  "...kTTTTTookoooookoOOkkTTTTTk...",
  "....kTTTTTkokkkkkOOOk.kTTTTTk...",
  "....kTTTTk.kkkOOOkkk..kTTTTk....",
  ".....kkkkk....kOk.....kkkkTrkk..",
  "...............k..........krrrk.",
  "...........kkkkokkkk......krrrk.",
  "..........koooooooook....krffrrk",
  ".........kooooyyyooook....kfffk.",
  "........koooyyyyyyyoook...kfffk.",
  ".......koooyyyyyyyyyoOOk.kORfk..",
  ".......koooyyyyyyyyYoOOk.kOOk...",
  ".......kooyyyyyyyyyYYOOk.kOOk...",
  ".......kooyyyyyyyyyYYOOk.kOOk...",
  ".......koooyyyyyyyyYoOOOkOOOk...",
  "........kooyyyyyyyYYOOOooOOkk...",
  ".........kooyyYYYYYOOOookkk.....",
  ".......kkooooOYYYOoooook........",
  ".......koooookkOkkoooook........",
  ".......kooook..k..kooook........",
  ".......kkkkkk.....kkkkkk........",
};

static const char* const SPR_BULBASAUR[32] = {  // 32x32
  "................................",
  "....................kkk.........",
  "....................kGk.........",
  ".............kk...kkgggkkk......",
  "...kk.......ktk..kggggggggk.....",
  "...ktk......ktk.kggggggggggk....",
  "...ktk......kttkggggggggggggk...",
  "...ktk.ddkkktttggggggggggggGk...",
  "...kktkkkttttttggggggggggggGk...",
  ".....kttttttttttgggggggggggGk...",
  "....ktkwkttttkwktggggggggggGk...",
  "...kttwkkttttwkkTTggggggggGGk...",
  "...kttkkkttttkkkTTgggggggGGk....",
  "...kttkkkttttkkkTTggGGGGGGG.....",
  "...kttttttttttttGGkkkkkkkG......",
  "...ktttttttttttTTk..............",
  "....ktktttttktTTTk..............",
  ".....ktkkkkkTddTttkk............",
  "......kTTTTTTddtttttkk..........",
  "......kttTTTttttttttttk.........",
  ".....kttttttttttttttttTk........",
  ".....ddttttttttttttttTTk........",
  ".....ddttttttttttddttTTk........",
  ".....ktttttttttttddttTTk........",
  ".....kttttttttttttttTTTk........",
  "......kttttttttttttTTtttk.......",
  "....kkttktttttttttttktttk.......",
  "....kttk.kttttTTtttk.kttk.......",
  "....kttk..ktttkktttk.kttk.......",
  "....kttk..kttk..kttk.kkkk.......",
  "....kkkk..kkkk..kkkk............",
  "................................",
};

static const char* const SPR_IVYSAUR[32] = {  // 32x32
  ".....................kk.........",
  "....................kPpk........",
  "...................kpPppk.......",
  ".............kk...kpppppPk......",
  "...kk.......ktk....kppppk.......",
  "...ktk......kttk..kppppPPk....k.",
  "...ktk......kttgkkgggPPgggkkkkk.",
  "...ktk.ddkkktttggggggggggggggkk.",
  "...kktkkkttttttggggggggggggGk...",
  ".....kttttttttttgggggggggggGk...",
  "....ktkwkttttkwktggggggggggGk...",
  "...kttwkkttttwkkTTggggggggGGk...",
  "...kttkkkttttkkkTTggggggggGk....",
  "...kttkkkttttkkkTTggggggGGkk....",
  "...kttttttttttttGTkkGGGGkk......",
  "...ktttttttttttTTk..kkkk........",
  "....ktktttttktTTTk..............",
  ".....ktkkkkkTddTttkk............",
  "......kTTTTTTddtttttkk..........",
  "......kttTTTttttttttttk.........",
  ".....kttttttttttttttttTk........",
  ".....ddttttttttttttttTTk........",
  ".....ddttttttttttttttTTk........",
  ".....ktttttttttttttttTTk........",
  ".....kttttttttttttttTTTk........",
  "......kttttttttttttTTtttk.......",
  "....kkttktttttttttttktttk.......",
  "....kttk.kttttTTtttk.kttk.......",
  "....kttk..ktttkktttk.kttk.......",
  "....kttk..kttk..kttk.kkkk.......",
  "....kkkk..kkkk..kkkk............",
  "................................",
};

static const char* const SPR_VENUSAUR[32] = {  // 32x32
  "................................",
  "................................",
  ".................kkkkkkkkk......",
  "................kPpppppppPk.....",
  "............kk.kpppppppppppk....",
  "..kk.......ktk.kppppfffpppPk....",
  "..ktk......ktk.kpppfffffppPk....",
  "..ktk.dd...ktk.kpppfffffpPPk....",
  "..ktk.kkkkkttk..kpgggggggPk.....",
  "..kttkttttttttkkgggggggggggkk...",
  "...ktkwkttttkwktggggggggggggGk..",
  "...ktwkkttttwkktggggggggggggGGk.",
  "..kttkkkttttkkkTTggggggggggGGk..",
  "..kttkkkttttkkkTTkggggggGkkkk...",
  "..kttttttttttttTk.kkkkkkk.......",
  "..ktttttttttttTTk...............",
  "...ktktttttktTTk................",
  "...kttkkkkktddTTkkkkk...........",
  "....kktTTTTTddTttttttkk.........",
  "......kTTTTTTttttttttttkk.......",
  "......ktttttttttttttttttk.......",
  ".....ktttttttttttttttttTTk......",
  ".....ddtttttttttttddtttTTk......",
  "....kddtttttttttttddtttTTTk.....",
  ".....ktttttttttttttttttTTk......",
  ".....kttttttttttttttttTtttkk....",
  "...kktttttttttttttttttTttttk....",
  "...kttttttttttttttttttTttttk....",
  "...kttttkktttttTTtttttkttttk....",
  "...ktttk..kttttTkttttk.kkkkk....",
  "...kkkkk..kkkkkk.kkkkk..........",
  "................................",
};

static const char* const SPR_SQUIRTLE[32] = {  // 32x32
  "................................",
  "................................",
  "..........kkkkkkk...............",
  "........kkbbbbbbbkk.............",
  ".......kbbbbbbbbbbbk............",
  "......kbkwkbbbbbkwkbk...........",
  "......kbwkkbbbbbwkkbk...........",
  ".....kbbkkkbbbbbkkkBBk..........",
  ".....kbbkkkbbbbbkkkBBk..........",
  ".....kbbbbbbbbbbbbbBBk..........",
  ".....kbbbbbbbbbbbbbBBk..........",
  ".....kbbbkbbbbbkbbbBBk..........",
  "......kbbbkkkkkbbbBBk...........",
  "......kbbbbbbbbbbBBBk...........",
  ".......kbbbbbbbBBBBk............",
  "........kkBBBBBBBkk....kk.......",
  "..........kBBBBBk....kkbbkk.....",
  "....kkk....kbbbk....kbbbkkbk....",
  "....kbk..kkbbbbbkk..kbbk..kk....",
  "....kbk.kbcccccccbkkbbbk...kk...",
  "....kbbkbcccyyycccbBbbbk...kk...",
  "....kbbbbcyyyyyyyCBBbbbbk.kBk...",
  "....kkkbccyyyyyyYCCBbbbbbkbk....",
  ".......kccyyyyyyYCCBkbbbbbBk....",
  ".......kbcyyyyyYYCBk.kkBBkk.....",
  ".......kbcccYYYCCCBk...kk.......",
  "........kbccCCCCCBk.............",
  "......kkbbbBBBBBbbbkk...........",
  "......kbbbbkkkkkbbbbk...........",
  "......kbbbk.....kbbbk...........",
  "......kkkkk.....kkkkk...........",
  "................................",
};

static const char* const SPR_WARTORTLE[32] = {  // 32x32
  "....k.............k.............",
  "...klk...........klk............",
  "..klLk...........kLlk...........",
  "...kLk....kkkkk..kLk............",
  "...klk..kklllllkkllk............",
  "...kkk.klllllllllllk............",
  "......kkwklllllkwkk.............",
  ".....klwkklllllwkklk............",
  ".....klkkklllllkkkLk............",
  ".....klkkklllllkkkLk............",
  ".....kllllllllllllLk............",
  ".....klllllllllllLLk............",
  ".....kllklllllkllLLk.kkkkkk.....",
  "......kllkkkkkllLLk.kllllllk....",
  ".......kllllllLLLk..klLllllk....",
  "........kkLLLLLkk..klllLllllk...",
  "..........kLLLk....kllllLlllk...",
  "...kkk.....klk.....kllllllllk...",
  "...klk...kklllkk...kllllllllk...",
  "...klk.kklccccclkk.kllllllllk...",
  "...klk.klccyyycclk.kllLllllLk...",
  "...kkkklccyyyyycCLkklllllllLk...",
  "......klcyyyyyyYCLk.klllllLk....",
  "......klcyyyyyyYCLk.klLLLLLk....",
  "......klccyyyyYCCLk..kkkkkk.....",
  ".......klccYYYCCLk..............",
  ".......kllcCCCCLLk..............",
  ".....kklllkLLLklllkk............",
  ".....klllk.kkk.klllk............",
  ".....klllk.....klllk............",
  ".....kkkkk.....kkkkk............",
  "................................",
};

static const char* const SPR_BLASTOISE[32] = {  // 32x32
  "................................",
  "................................",
  "................................",
  ".............kkkkkk.............",
  "...kkkk....kkNNNNNNkk....kkkk...",
  "...kkkk...kNNNNNNNNNNk...kkkk...",
  "...kSSk...kkwkNNNNNkwk...kSSk...",
  "...ksssk.kNwkkNNNNNwkkk.ksssk...",
  "....kssk.kNkkkNNNNNkkkk.kssk....",
  "....kssk.kNkkkNNNNNkkkk.kssk....",
  "....kssk.kNNNNNNNNNNMMk.kssk....",
  "....kssk..kNNNNNNNNNMk..kssk....",
  "....kssk..kNkNNNNNkMMk..kssk....",
  "....kssk...kkkkkkkMkk...kssk....",
  "....kssk.....kMMMMk.....kssk....",
  "....kssk......kNNk......kssk....",
  "....kssk...kkkNNNNkkk...kssk....",
  "....kssk.kkNNNNNNNNNNkk.kssk....",
  "....kssskNNNNccccccNNNNksssk....",
  ".....kNNNNNNcccyycccNNNNNNk.....",
  ".....kNNNNNccyyyyyyccNNNNNk.....",
  ".....kNNNNccyyyyyyyyCCMNNNk.....",
  ".....kNNNNccyyyyyyyYCCMNNNk.....",
  ".....kNNNNccyyyyyyyYCCMNNNk.....",
  ".....kNNNNccyyyyyyYYCCMNNNk.....",
  ".....kkkNNNccyyYYYYCCMMNkkk.....",
  "........kNNNccCYYCCCMMMk........",
  ".........kNNNCCCCCCMMMk.........",
  ".......kkNNNNkMMMMkNNNNkk.......",
  ".......kNNNNk.kkkk.kNNNNk.......",
  ".......kNNNNk......kNNNNk.......",
  ".......kkkkkk......kkkkkk.......",
};

static const char* const SPR_ICON_FOOD[16] = {  // 16x16
  "................",
  "................",
  "...........k....",
  "........k.kk....",
  "........k.......",
  ".......krk......",
  ".....kkrrrkk....",
  "....krwrrrrrk...",
  "....kwrrrrrrk...",
  "...krrrrrrrrRk..",
  "...krrrrrrrrRk..",
  "....krrrrrrrk...",
  "....krrrrrrRk...",
  ".....kkRRRkk....",
  ".......kkk......",
  "................",
};

static const char* const SPR_ICON_PLAY[16] = {  // 16x16
  "................",
  "................",
  ".......kkk......",
  ".....kkrrrkk....",
  "....krrrrrrrk...",
  "...krrrrrrrrrk..",
  "...krrrrrrrrrk..",
  "..krrrrkkkrrrRk.",
  "..kkkkkkwkkkkkk.",
  "..krrwwkkkwwrRk.",
  "...kwwwwwwwwwk..",
  "...kwwwwwwwwYk..",
  "...kwwwwwwwwYk..",
  "....kwwwwwwYk...",
  ".....kkkkkkk....",
  "................",
};

static const char* const SPR_ICON_LIGHT[16] = {  // 16x16
  "................",
  "................",
  "................",
  "......kk....f...",
  "....kkk.........",
  "....kfk......f..",
  "...kffk.........",
  "...kffk.........",
  "...kfffk........",
  "...kffffk.......",
  "...kfffffkk.kk..",
  "....kffffffkk...",
  "....kkfffffkk...",
  "......kkkkk.....",
  "................",
  "................",
};

static const char* const SPR_ICON_CLEAN[16] = {  // 16x16
  "................",
  "................",
  "................",
  "...kk......kkk..",
  "...kk.....kwbbk.",
  "...kk.....kbbbk.",
  "......kkk..kkk..",
  "....kkbbbkk.....",
  "...kbwbbbbbk....",
  "...kbbwbbbBk....",
  "...kbbbbbbbk....",
  "...kbbbbbbBk....",
  "...kbbbbbbBk....",
  "....kkBBBkk.....",
  "......kkk.......",
  "................",
};

static const char* const SPR_ICON_BERRY_B[16] = {  // 16x16
  "................",
  "................",
  "...........k....",
  "........k.k.....",
  "........k.......",
  ".......kbk......",
  ".....kkbbbkk....",
  "....kbwbbbbbk...",
  "....kwbbbbbbk...",
  "...kbbbbbbbbBk..",
  "...kbbbbbbbbBk..",
  "....kbbbbbbbk...",
  "....kbbbbbbBk...",
  ".....kkBBBkk....",
  ".......kkk......",
  "................",
};

static const char* const SPR_ICON_BERRY_G[16] = {  // 16x16
  "................",
  "................",
  "...........k....",
  "........k.k.....",
  "........k.......",
  ".......kgk......",
  ".....kkgggkk....",
  "....kgwgggggk...",
  "....kwggggggk...",
  "...kggggggggGk..",
  "...kggggggggGk..",
  "....kgggggggk...",
  "....kggggggGk...",
  ".....kkGGGkk....",
  ".......kkk......",
  "................",
};

static const char* const SPR_ICON_CANDY[16] = {  // 16x16
  "................",
  "................",
  "................",
  "................",
  "................",
  "......kkkkk.....",
  "..k..kpwpppk.k..",
  "...kkpwpppppk...",
  "..kpppppPppPpk..",
  "...kkppppPpPk...",
  "..k..kppppPk.k..",
  "......kkkkk.....",
  "................",
  "................",
  "................",
  "................",
};

static const char* const SPR_EGG[32] = {  // 32x32
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................k...............",
  ".............kkkykkk............",
  "............kyyyyyyyk...........",
  "...........kyyyyyyyyyk..........",
  "..........kyyyyyyyyyyyk.........",
  ".........kyyggyyyyyyyyyk........",
  ".........kyyggyyyyyyyyyk........",
  "........kyyygyyyyyyyyyyYk.......",
  "........kyyyyyyyyyyyyyYYk.......",
  "........kyyyyyyyyyyyyyYYk.......",
  "........kyyyyyyyyyyggyYYk.......",
  "........kyyyyyyyyyyggyYYk.......",
  "........kyyyyyyyyyyygyYYk.......",
  "........kyyyyyyyyyyyyyYYk.......",
  "........kyyyyyyyyyyyyyYYk.......",
  "........kyyyyyyyyyyyyYYYk.......",
  ".........kyyyggyyyyyyYYk........",
  ".........kyyyggyyyyyYYYk........",
  "..........kyyyyyyyYYYYk.........",
  "...........kyyYYYYYYYk..........",
  "............kYYYYYYYk...........",
  ".............kkkYkkk............",
  "................k...............",
  "................................",
  "................................",
  "................................",
};

static const char* const SPR_POOP[32] = {  // 32x32
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................k...............",
  ".................k..............",
  "...............kkk..............",
  "..............kccck.............",
  ".............kccccck............",
  "..............kccck.............",
  "..............kCCCk.............",
  "............kkccccckk...........",
  "............kccccccck...........",
  "...........kccccccccCk..........",
  "............kccccccck...........",
  "............kcccccCCk...........",
  "...........kccCCCCCcck..........",
  "..........kccccccccccck.........",
  "..........kccccccccccCk.........",
  ".........kcccccccccccCCk........",
  "..........kccccccccccCk.........",
  "..........kcccccccccCCk.........",
  "...........kkkcCCCCkkk..........",
  "..............kkkkk.............",
  "................................",
  "................................",
  "................................",
};

static const char* const SPR_HEART[32] = {  // 32x32
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "........krrrrk....krrrrk........",
  ".......krrrrrrk..krrrrrrk.......",
  "......krrrrrrrrrrrrrrrrrrk......",
  ".....krrrrrrrrrrrrrrrrrrrrk.....",
  ".....krrwwrrrrrrrrrrrrrrRrk.....",
  ".....krwwrrrrrrrrrrrrrrRRrk.....",
  "......krrrrrrrrrrrrrrrrrrk......",
  ".......krrrrrrrrrrrrrrrrk.......",
  "........krrrrrrrrrrrrrrk........",
  ".........krrrrrrrrrrrrk.........",
  "..........krrrrrrrrrrk..........",
  "...........krrrrrrrrk...........",
  "............krrrrrrk............",
  ".............krrrrk.............",
  "..............krrk..............",
  "...............kk...............",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
  "................................",
};

static const Species SPECIES[NUM_SPECIES] = {
  { "CHARMANDER", TYPE_FUEGO, SPR_CHARMANDER, 5, SP_CHARMELEON, 16, 6, 9, 17, 12, 13, 0xF427, 0xEA87 },
  { "CHARMELEON", TYPE_FUEGO, SPR_CHARMELEON, 6, SP_CHARIZARD, 36, 6, 8, 16, 12, 12, 0xEA87, 0xEA87 },
  { "CHARIZARD", TYPE_FUEGO, SPR_CHARIZARD, 7, -1, 0, 6, 10, 18, 12, 14, 0xF427, 0xEA87 },
  { "BULBASAUR", TYPE_PLANTA, SPR_BULBASAUR, 5, SP_IVYSAUR, 16, 10, 6, 13, 16, 9, 0x8EB6, 0x3C49 },
  { "IVYSAUR", TYPE_PLANTA, SPR_IVYSAUR, 6, SP_VENUSAUR, 36, 10, 6, 13, 16, 9, 0x8EB6, 0x3C49 },
  { "VENUSAUR", TYPE_PLANTA, SPR_VENUSAUR, 7, -1, 0, 10, 5, 12, 16, 8, 0x8EB6, 0x3C49 },
  { "SQUIRTLE", TYPE_AGUA, SPR_SQUIRTLE, 5, SP_WARTORTLE, 16, 5, 8, 16, 11, 12, 0x7E3D, 0x4C98 },
  { "WARTORTLE", TYPE_AGUA, SPR_WARTORTLE, 6, SP_BLASTOISE, 36, 6, 7, 15, 12, 11, 0x9D5C, 0x4C98 },
  { "BLASTOISE", TYPE_AGUA, SPR_BLASTOISE, 7, -1, 0, 6, 11, 19, 12, 15, 0x3B74, 0x4C98 },
};

static const int8_t STARTERS[] = { SP_CHARMANDER, SP_BULBASAUR, SP_SQUIRTLE };
#define NUM_STARTERS 3
