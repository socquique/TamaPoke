#pragma once

#include <stdint.h>

struct BattleStats {
  uint16_t hp;
  uint16_t atk;
  uint16_t def;
  uint16_t spe;
  uint8_t level;
  uint8_t type1 = 0;
  uint8_t type2 = 0;
};

struct BattleOptions {
  // 0..99. Keeps ties and damage variation deterministic in tests.
  uint8_t luckRoll;
};

struct BattleResult {
  bool playerWon;
  uint8_t rounds;
  uint16_t playerDamage;
  uint16_t enemyDamage;
  uint16_t playerHpLeft;
  uint16_t enemyHpLeft;
};

enum BattleAction : uint8_t {
  BATTLE_ATTACK = 0,
  BATTLE_ATTACK_QUICK,
  BATTLE_ATTACK_HEAVY,
  BATTLE_DODGE,
  BATTLE_REST,
};

struct BattleRuntime {
  BattleStats player;
  BattleStats enemy;
  uint16_t playerHp;
  uint16_t enemyHp;
  uint16_t playerMaxHp;
  uint16_t enemyMaxHp;
  uint8_t round;
  uint16_t playerDamageTotal;
  uint16_t enemyDamageTotal;
  uint8_t restUsesLeft;
  bool counterReady;
};

struct BattleTurnResult {
  uint16_t playerDamage;
  uint16_t enemyDamage;
  uint16_t playerHeal;
  bool playerDodged;
  bool enemyDodged;
  bool playerRested;
  bool counterReady;
  bool counterUsed;
  bool restFailed;
  bool battleEnded;
  bool playerWon;
};

bool canStartWildBattle(bool isEgg, bool sleeping, uint8_t ceremony);
uint8_t wildLevelFor(uint8_t petLevel, uint8_t luckRoll);
int16_t pickWildSpecies(uint8_t roll);
BattleStats wildBattleStats(int16_t dex, uint8_t level);
BattleRuntime beginBattleRuntime(const BattleStats &player, const BattleStats &enemy);
BattleTurnResult stepBattle(BattleRuntime &battle, BattleAction action, uint8_t luckRoll);

BattleResult resolveBattle(const BattleStats &player,
                           const BattleStats &enemy,
                           const BattleOptions &options);
