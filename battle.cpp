#include "battle.h"
#include "dex.h"

namespace {
constexpr uint8_t MAX_BATTLE_ROUNDS = 50;
constexpr uint8_t MAX_TURN_ROUNDS = 20;

uint16_t hpFor(const BattleStats &stats) {
  if (stats.hp > 0) return stats.hp;
  return (uint16_t)(18 + stats.level * 3 + stats.def / 2);
}

uint8_t clampedLuck(uint8_t luck) {
  return luck > 99 ? 99 : luck;
}

uint16_t damageFor(const BattleStats &attacker, const BattleStats &defender, uint8_t luck) {
  uint16_t level = attacker.level > 0 ? attacker.level : 1;
  uint32_t pressure = (uint32_t)attacker.atk * (10 + level / 4);
  uint32_t guard = (uint32_t)defender.def + 35;
  uint32_t base = 1 + pressure / guard;
  uint32_t variation = 90 + (clampedLuck(luck) % 21);  // 90..110 %
  uint32_t damage = base * variation / 100;
  if (damage == 0) damage = 1;
  return damage > 65535 ? 65535 : (uint16_t)damage;
}

void applyHit(uint16_t &hpLeft, uint16_t damage, uint16_t &damageTotal) {
  uint16_t dealt = damage > hpLeft ? hpLeft : damage;
  hpLeft -= dealt;
  damageTotal += dealt;
}

uint16_t turnHpFor(const BattleStats &stats) {
  uint16_t level = stats.level > 0 ? stats.level : 1;
  uint32_t scaled = 30 + (uint32_t)level * 5 + stats.def;
  uint16_t legacy = hpFor(stats);
  if (scaled < legacy) scaled = legacy;
  return scaled > 65535 ? 65535 : (uint16_t)scaled;
}

uint16_t cappedTurnDamage(const BattleStats &attacker,
                          const BattleStats &defender,
                          uint16_t defenderMaxHp,
                          uint8_t luck,
                          bool counter,
                          uint8_t powerPct) {
  uint16_t damage = damageFor(attacker, defender, luck);
  damage = (uint16_t)((uint32_t)damage * powerPct / 100);
  if (damage == 0) damage = 1;
  if (counter) damage = (uint16_t)((uint32_t)damage * 3 / 2);
  uint16_t cap = (uint16_t)((uint32_t)defenderMaxHp * 35 / 100);
  if (cap < 1) cap = 1;
  if (damage > cap) damage = cap;
  return damage;
}

bool finished(const BattleRuntime &battle) {
  return battle.playerHp == 0 || battle.enemyHp == 0 || battle.round >= MAX_TURN_ROUNDS;
}

bool winner(const BattleRuntime &battle) {
  if (battle.enemyHp == 0) return true;
  if (battle.playerHp == 0) return false;
  return battle.playerHp >= battle.enemyHp;
}

bool isAttackAction(BattleAction action) {
  return action == BATTLE_ATTACK || action == BATTLE_ATTACK_QUICK || action == BATTLE_ATTACK_HEAVY;
}

uint8_t attackPowerPct(BattleAction action) {
  if (action == BATTLE_ATTACK_QUICK) return 85;
  if (action == BATTLE_ATTACK_HEAVY) return 125;
  return 100;
}

uint8_t enemyDodgeChance(const BattleRuntime &battle, BattleAction action) {
  int chance = battle.enemy.spe > battle.player.spe ? 18 : 10;
  if (action == BATTLE_ATTACK_QUICK) chance -= 5;
  else if (action == BATTLE_ATTACK_HEAVY) chance += 18;
  if (chance < 0) chance = 0;
  if (chance > 45) chance = 45;
  return (uint8_t)chance;
}
}  // namespace

bool canStartWildBattle(bool isEgg, bool sleeping, uint8_t ceremony) {
  return !isEgg && !sleeping && ceremony == 0;
}

uint8_t wildLevelFor(uint8_t petLevel, uint8_t luckRoll) {
  int level = (int)(petLevel ? petLevel : 1) + (int)(luckRoll % 4) - 1;
  if (level < 1) level = 1;
  return level > 100 ? 100 : (uint8_t)level;
}

int16_t pickWildSpecies(uint8_t roll) {
  int16_t pool[DEX_COUNT];
  int count = 0;
  uint8_t targetRarity = (roll % 100) < 25 ? R_RARO : R_COMUN;
  for (int16_t dex = 1; dex <= DEX_COUNT; dex++) {
    if (DEX_TBL[dex].rarity == targetRarity) pool[count++] = dex;
  }
  if (count == 0 && targetRarity == R_RARO) {
    for (int16_t dex = 1; dex <= DEX_COUNT; dex++)
      if (DEX_TBL[dex].rarity == R_COMUN) pool[count++] = dex;
  }
  return count > 0 ? pool[roll % count] : 1;
}

BattleStats wildBattleStats(int16_t dex, uint8_t level) {
  if (dex < 1 || dex > DEX_COUNT) dex = 1;
  const DexEntry &entry = DEX_TBL[dex];
  uint8_t lvl = level ? level : 1;
  BattleStats stats = {};
  stats.hp = 0;
  stats.atk = entry.bAtk + lvl;
  stats.def = entry.bDef + lvl;
  stats.spe = entry.bSpe + lvl;
  stats.level = lvl;
  return stats;
}

BattleRuntime beginBattleRuntime(const BattleStats &player, const BattleStats &enemy) {
  BattleRuntime battle = {};
  battle.player = player;
  battle.enemy = enemy;
  battle.playerMaxHp = turnHpFor(player);
  battle.enemyMaxHp = turnHpFor(enemy);
  battle.playerHp = battle.playerMaxHp;
  battle.enemyHp = battle.enemyMaxHp;
  battle.restUsesLeft = 2;
  battle.counterReady = false;
  return battle;
}

BattleTurnResult stepBattle(BattleRuntime &battle, BattleAction action, uint8_t luckRoll) {
  BattleTurnResult turn = {};
  if (finished(battle)) {
    turn.battleEnded = true;
    turn.playerWon = winner(battle);
    return turn;
  }

  uint8_t luck = clampedLuck(luckRoll);
  if (action == BATTLE_REST && battle.restUsesLeft == 0) {
    turn.playerRested = true;
    turn.restFailed = true;
    return turn;
  }

  battle.round++;

  if (action == BATTLE_REST) {
    turn.playerRested = true;
    battle.restUsesLeft--;
    uint16_t heal = battle.playerMaxHp / 5;
    if (heal < 4) heal = 4;
    uint16_t missing = battle.playerMaxHp - battle.playerHp;
    turn.playerHeal = heal > missing ? missing : heal;
    battle.playerHp += turn.playerHeal;
  } else if (isAttackAction(action)) {
    uint8_t dodgeChance = enemyDodgeChance(battle, action);
    turn.enemyDodged = ((uint16_t)luck + battle.enemy.spe / 2) % 100 < dodgeChance;
    if (!turn.enemyDodged) {
      turn.counterUsed = battle.counterReady;
      turn.playerDamage = cappedTurnDamage(battle.player, battle.enemy, battle.enemyMaxHp,
                                           luck, battle.counterReady, attackPowerPct(action));
      battle.counterReady = false;
      applyHit(battle.enemyHp, turn.playerDamage, battle.playerDamageTotal);
    }
  }

  if (battle.enemyHp > 0 && !turn.enemyDodged) {
    uint16_t enemyHit = cappedTurnDamage(battle.enemy, battle.player, battle.playerMaxHp, 99 - luck, false, 100);
    if (action == BATTLE_DODGE) {
      if (luck < 75) {
        turn.playerDodged = true;
        battle.counterReady = true;
        turn.counterReady = true;
        enemyHit = 0;
      } else {
        enemyHit = enemyHit > 1 ? enemyHit / 2 : 1;
      }
    }
    if (enemyHit > 0) {
      turn.enemyDamage = enemyHit;
      applyHit(battle.playerHp, turn.enemyDamage, battle.enemyDamageTotal);
    }
  }

  turn.battleEnded = finished(battle);
  turn.playerWon = turn.battleEnded ? winner(battle) : false;
  return turn;
}

BattleResult resolveBattle(const BattleStats &player,
                           const BattleStats &enemy,
                           const BattleOptions &options) {
  BattleResult result = {};
  result.playerHpLeft = hpFor(player);
  result.enemyHpLeft = hpFor(enemy);

  const uint8_t luck = clampedLuck(options.luckRoll);
  const bool playerFirst = player.spe == enemy.spe ? luck >= 50 : player.spe > enemy.spe;
  const uint16_t playerHit = damageFor(player, enemy, luck);
  const uint16_t enemyHit = damageFor(enemy, player, 99 - luck);

  for (uint8_t round = 0; round < MAX_BATTLE_ROUNDS; round++) {
    result.rounds = round + 1;
    if (playerFirst) {
      applyHit(result.enemyHpLeft, playerHit, result.playerDamage);
      if (result.enemyHpLeft == 0) break;
      applyHit(result.playerHpLeft, enemyHit, result.enemyDamage);
      if (result.playerHpLeft == 0) break;
    } else {
      applyHit(result.playerHpLeft, enemyHit, result.enemyDamage);
      if (result.playerHpLeft == 0) break;
      applyHit(result.enemyHpLeft, playerHit, result.playerDamage);
      if (result.enemyHpLeft == 0) break;
    }
  }

  if (result.enemyHpLeft == 0) {
    result.playerWon = true;
  } else if (result.playerHpLeft == 0) {
    result.playerWon = false;
  } else if (result.enemyHpLeft != result.playerHpLeft) {
    result.playerWon = result.enemyHpLeft < result.playerHpLeft;
  } else {
    result.playerWon = playerFirst;
  }

  return result;
}
