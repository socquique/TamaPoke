#include <cstdlib>
#include <iostream>

#include "battle.h"
#include "dex.h"

static int failures = 0;

#define EXPECT_TRUE(expr) do { \
  if (!(expr)) { \
    std::cerr << "FEHLER: " << __FILE__ << ":" << __LINE__ << ": " << #expr << "\n"; \
    failures++; \
  } \
} while (0)

#define EXPECT_EQ(actual, expected) do { \
  auto a = (actual); \
  auto e = (expected); \
  if (!(a == e)) { \
    std::cerr << "FEHLER: " << __FILE__ << ":" << __LINE__ << ": " << #actual \
              << " war " << +a << ", erwartet " << +e << "\n"; \
    failures++; \
  } \
} while (0)

static void testStrongerPlayerWins() {
  BattleStats player = { 0, 90, 70, 80, 20 };
  BattleStats enemy = { 0, 45, 35, 40, 18 };

  BattleResult result = resolveBattle(player, enemy, { 60 });

  EXPECT_TRUE(result.playerWon);
  EXPECT_TRUE(result.playerHpLeft > 0);
  EXPECT_EQ(result.enemyHpLeft, 0);
  EXPECT_TRUE(result.playerDamage > result.enemyDamage);
}

static void testStrongerEnemyWins() {
  BattleStats player = { 0, 35, 30, 30, 12 };
  BattleStats enemy = { 0, 95, 75, 80, 20 };

  BattleResult result = resolveBattle(player, enemy, { 40 });

  EXPECT_TRUE(!result.playerWon);
  EXPECT_EQ(result.playerHpLeft, 0);
  EXPECT_TRUE(result.enemyHpLeft > 0);
}

static void testFasterPokemonHitsFirst() {
  BattleStats slowPlayer = { 8, 100, 40, 20, 20 };
  BattleStats fastEnemy = { 8, 100, 40, 90, 20 };

  BattleResult result = resolveBattle(slowPlayer, fastEnemy, { 80 });

  EXPECT_TRUE(!result.playerWon);
  EXPECT_EQ(result.playerHpLeft, 0);
  EXPECT_TRUE(result.enemyHpLeft > 0);
  EXPECT_EQ(result.playerDamage, 0);
}

static void testSpeedTieUsesLuckRoll() {
  BattleStats player = { 8, 100, 40, 50, 20 };
  BattleStats enemy = { 8, 100, 40, 50, 20 };

  BattleResult playerFirst = resolveBattle(player, enemy, { 50 });
  BattleResult enemyFirst = resolveBattle(player, enemy, { 49 });

  EXPECT_TRUE(playerFirst.playerWon);
  EXPECT_TRUE(!enemyFirst.playerWon);
}

static void testMinimumDamagePreventsStall() {
  BattleStats player = { 12, 1, 250, 50, 1 };
  BattleStats enemy = { 12, 1, 250, 40, 1 };

  BattleResult result = resolveBattle(player, enemy, { 60 });

  EXPECT_TRUE(result.playerWon);
  EXPECT_EQ(result.rounds, 12);
  EXPECT_EQ(result.playerDamage, 12);
  EXPECT_EQ(result.enemyDamage, 11);
}

static void testRoundLimitEndsDefensiveCase() {
  BattleStats player = { 1000, 1, 250, 50, 1 };
  BattleStats enemy = { 1000, 1, 250, 40, 1 };

  BattleResult result = resolveBattle(player, enemy, { 60 });

  EXPECT_TRUE(result.playerWon);
  EXPECT_EQ(result.rounds, 50);
  EXPECT_EQ(result.playerDamage, 50);
  EXPECT_EQ(result.enemyDamage, 50);
  EXPECT_EQ(result.playerHpLeft, 950);
  EXPECT_EQ(result.enemyHpLeft, 950);
}

static void testDerivedHpUsesLevelAndDefense() {
  BattleStats stats = { 0, 80, 64, 40, 10 };
  BattleStats weakEnemy = { 1, 1, 1, 1, 1 };

  BattleResult result = resolveBattle(stats, weakEnemy, { 50 });

  EXPECT_EQ(result.playerHpLeft, 80);
}

static void testWildBattleStartGate() {
  EXPECT_TRUE(canStartWildBattle(false, false, 0));
  EXPECT_TRUE(!canStartWildBattle(true, false, 0));
  EXPECT_TRUE(!canStartWildBattle(false, true, 0));
  EXPECT_TRUE(!canStartWildBattle(false, false, 1));
}

static void testWildLevelStaysNearPetLevel() {
  EXPECT_EQ(wildLevelFor(1, 0), 1);
  EXPECT_EQ(wildLevelFor(10, 0), 9);
  EXPECT_EQ(wildLevelFor(10, 1), 10);
  EXPECT_EQ(wildLevelFor(10, 3), 12);
}

static void testWildSpeciesUsesHatchableNonLegendaryPool() {
  for (uint8_t roll = 0; roll < 100; roll++) {
    int16_t dex = pickWildSpecies(roll);
    EXPECT_TRUE(dex >= 1 && dex <= DEX_COUNT);
    EXPECT_TRUE(DEX_TBL[dex].rarity == R_COMUN || DEX_TBL[dex].rarity == R_RARO);
  }
}

static void testWildStatsUseDexBaseAndLevel() {
  BattleStats stats = wildBattleStats(4, 7);
  EXPECT_EQ(stats.level, 7);
  EXPECT_EQ(stats.atk, DEX_TBL[4].bAtk + 7);
  EXPECT_EQ(stats.def, DEX_TBL[4].bDef + 7);
  EXPECT_EQ(stats.spe, DEX_TBL[4].bSpe + 7);
  EXPECT_EQ(stats.type1, TYPE_FIRE);
  EXPECT_EQ(stats.type2, TYPE_NONE);
}

static void testBattleRuntimeStartsWithScaledHp() {
  BattleRuntime battle = beginBattleRuntime({ 0, 60, 40, 50, 10 },
                                            { 0, 58, 42, 48, 10 });

  EXPECT_EQ(battle.round, 0);
  EXPECT_EQ(battle.playerMaxHp, 30 + 10 * 5 + 40);
  EXPECT_EQ(battle.enemyMaxHp, 30 + 10 * 5 + 42);
  EXPECT_EQ(battle.playerHp, battle.playerMaxHp);
  EXPECT_EQ(battle.enemyHp, battle.enemyMaxHp);
  EXPECT_EQ(battle.restUsesLeft, 2);
  EXPECT_TRUE(!battle.counterReady);
}

static void testAttackStepsOneRoundAndAvoidsNormalOneHit() {
  BattleRuntime battle = beginBattleRuntime({ 0, 120, 55, 70, 20 },
                                            { 0, 45, 45, 40, 19 });

  BattleTurnResult turn = stepBattle(battle, BATTLE_ATTACK, 50);

  EXPECT_EQ(battle.round, 1);
  EXPECT_TRUE(turn.playerDamage > 0);
  EXPECT_TRUE(battle.enemyHp > 0);
  EXPECT_TRUE(!turn.battleEnded);
}

static void testDodgePreventsEnemyDamageDeterministically() {
  BattleRuntime battle = beginBattleRuntime({ 0, 50, 40, 60, 10 },
                                            { 0, 90, 40, 50, 10 });
  uint16_t before = battle.playerHp;

  BattleTurnResult turn = stepBattle(battle, BATTLE_DODGE, 20);

  EXPECT_TRUE(turn.playerDodged);
  EXPECT_EQ(turn.enemyDamage, 0);
  EXPECT_EQ(battle.playerHp, before);
  EXPECT_TRUE(turn.counterReady);
  EXPECT_TRUE(battle.counterReady);
}

static void testRestHealsOnlyToMaxHpAndConsumesUse() {
  BattleRuntime battle = beginBattleRuntime({ 0, 50, 60, 50, 10 },
                                            { 0, 1, 40, 40, 10 });
  battle.playerHp = battle.playerMaxHp - 3;

  BattleTurnResult turn = stepBattle(battle, BATTLE_REST, 50);

  EXPECT_TRUE(turn.playerRested);
  EXPECT_EQ(turn.playerHeal, 3);
  EXPECT_EQ(battle.restUsesLeft, 1);
  EXPECT_TRUE(battle.playerHp <= battle.playerMaxHp);
  EXPECT_TRUE(battle.playerHp >= battle.playerMaxHp - 1);
}

static void testRestIsLimitedToTwoUses() {
  BattleRuntime battle = beginBattleRuntime({ 0, 50, 60, 50, 10 },
                                            { 0, 1, 40, 40, 10 });
  battle.playerHp = battle.playerMaxHp / 2;

  BattleTurnResult first = stepBattle(battle, BATTLE_REST, 50);
  battle.playerHp = battle.playerMaxHp / 2;
  BattleTurnResult second = stepBattle(battle, BATTLE_REST, 50);
  battle.playerHp = battle.playerMaxHp / 2;
  BattleTurnResult third = stepBattle(battle, BATTLE_REST, 50);

  EXPECT_TRUE(first.playerHeal > 0);
  EXPECT_TRUE(second.playerHeal > 0);
  EXPECT_TRUE(third.restFailed);
  EXPECT_EQ(third.playerHeal, 0);
  EXPECT_EQ(battle.restUsesLeft, 0);
  EXPECT_EQ(third.enemyDamage, 0);
  EXPECT_EQ(battle.round, 2);
}

static void testCounterBoostsNextAttackDamage() {
  BattleStats player = { 0, 80, 45, 70, 20 };
  BattleStats enemy = { 0, 50, 35, 40, 20 };
  BattleRuntime normal = beginBattleRuntime(player, enemy);
  BattleRuntime counter = beginBattleRuntime(player, enemy);
  counter.counterReady = true;

  BattleTurnResult normalTurn = stepBattle(normal, BATTLE_ATTACK, 50);
  BattleTurnResult counterTurn = stepBattle(counter, BATTLE_ATTACK, 50);

  EXPECT_TRUE(counterTurn.counterUsed);
  EXPECT_TRUE(!counter.counterReady);
  EXPECT_TRUE(counterTurn.playerDamage > normalTurn.playerDamage);
  EXPECT_EQ(counterTurn.playerDamage, (uint16_t)(normalTurn.playerDamage * 3 / 2));
}

static void testQuickAndHeavyAttackDamageTradeoff() {
  BattleStats player = { 0, 90, 45, 70, 20 };
  BattleStats enemy = { 0, 50, 35, 40, 20 };
  BattleRuntime normal = beginBattleRuntime(player, enemy);
  BattleRuntime quick = beginBattleRuntime(player, enemy);
  BattleRuntime heavy = beginBattleRuntime(player, enemy);

  BattleTurnResult normalTurn = stepBattle(normal, BATTLE_ATTACK, 50);
  BattleTurnResult quickTurn = stepBattle(quick, BATTLE_ATTACK_QUICK, 50);
  BattleTurnResult heavyTurn = stepBattle(heavy, BATTLE_ATTACK_HEAVY, 50);

  EXPECT_TRUE(quickTurn.playerDamage < normalTurn.playerDamage);
  EXPECT_TRUE(heavyTurn.playerDamage > normalTurn.playerDamage);
}

static void testHeavyAttackCanBeDodgedWhenQuickWouldHit() {
  BattleStats player = { 0, 90, 45, 70, 20 };
  BattleStats enemy = { 0, 50, 35, 40, 20 };
  BattleRuntime quick = beginBattleRuntime(player, enemy);
  BattleRuntime heavy = beginBattleRuntime(player, enemy);

  BattleTurnResult quickTurn = stepBattle(quick, BATTLE_ATTACK_QUICK, 0);
  BattleTurnResult heavyTurn = stepBattle(heavy, BATTLE_ATTACK_HEAVY, 0);

  EXPECT_TRUE(!quickTurn.enemyDodged);
  EXPECT_TRUE(heavyTurn.enemyDodged);
  EXPECT_TRUE(quickTurn.playerDamage > 0);
  EXPECT_EQ(heavyTurn.playerDamage, 0);
}

static void testTypeAdvantageLightlyChangesDamage() {
  BattleStats neutral = { 0, 80, 50, 50, 20, TYPE_NORMAL, TYPE_NONE };
  BattleStats fire = { 0, 80, 50, 50, 20, TYPE_FIRE, TYPE_NONE };
  BattleStats grass = { 0, 80, 50, 50, 20, TYPE_GRASS, TYPE_NONE };

  BattleRuntime normalVsGrass = beginBattleRuntime(neutral, grass);
  BattleRuntime fireVsGrass = beginBattleRuntime(fire, grass);
  BattleRuntime grassVsFire = beginBattleRuntime(grass, fire);

  BattleTurnResult neutralTurn = stepBattle(normalVsGrass, BATTLE_ATTACK, 50);
  BattleTurnResult fireTurn = stepBattle(fireVsGrass, BATTLE_ATTACK, 50);
  BattleTurnResult resistedTurn = stepBattle(grassVsFire, BATTLE_ATTACK, 50);

  EXPECT_TRUE(fireTurn.playerDamage > neutralTurn.playerDamage);
  EXPECT_TRUE(resistedTurn.playerDamage < neutralTurn.playerDamage);
}

static void testTypeEffectHelperHandlesDualTypes() {
  EXPECT_EQ(battleTypeEffectPct(TYPE_FIRE, TYPE_GRASS, TYPE_NONE), 120);
  EXPECT_EQ(battleTypeEffectPct(TYPE_GRASS, TYPE_FIRE, TYPE_NONE), 85);
  EXPECT_EQ(battleTypeEffectPct(TYPE_FIRE, TYPE_GRASS, TYPE_ICE), 144);
}

static void testCounterDamageStillUsesTurnCap() {
  BattleRuntime battle = beginBattleRuntime({ 0, 650, 60, 70, 30 },
                                            { 0, 10, 20, 20, 30 });
  battle.counterReady = true;

  BattleTurnResult turn = stepBattle(battle, BATTLE_ATTACK, 50);
  uint16_t cap = (uint16_t)((uint32_t)battle.enemyMaxHp * 35 / 100);
  if (cap < 1) cap = 1;

  EXPECT_TRUE(turn.counterUsed);
  EXPECT_TRUE(turn.playerDamage <= cap);
  EXPECT_TRUE(battle.enemyHp > 0);
}

static void testTurnBattleEndsAtRoundLimitByRemainingHp() {
  BattleRuntime battle = beginBattleRuntime({ 0, 1, 250, 50, 1 },
                                            { 0, 1, 250, 40, 1 });

  BattleTurnResult turn = {};
  for (int i = 0; i < 20; i++) {
    turn = stepBattle(battle, BATTLE_DODGE, 20);
  }

  EXPECT_TRUE(turn.battleEnded);
  EXPECT_TRUE(turn.playerWon);
  EXPECT_EQ(battle.round, 20);
}

int main() {
  testStrongerPlayerWins();
  testStrongerEnemyWins();
  testFasterPokemonHitsFirst();
  testSpeedTieUsesLuckRoll();
  testMinimumDamagePreventsStall();
  testRoundLimitEndsDefensiveCase();
  testDerivedHpUsesLevelAndDefense();
  testWildBattleStartGate();
  testWildLevelStaysNearPetLevel();
  testWildSpeciesUsesHatchableNonLegendaryPool();
  testWildStatsUseDexBaseAndLevel();
  testBattleRuntimeStartsWithScaledHp();
  testAttackStepsOneRoundAndAvoidsNormalOneHit();
  testDodgePreventsEnemyDamageDeterministically();
  testRestHealsOnlyToMaxHpAndConsumesUse();
  testRestIsLimitedToTwoUses();
  testCounterBoostsNextAttackDamage();
  testQuickAndHeavyAttackDamageTradeoff();
  testHeavyAttackCanBeDodgedWhenQuickWouldHit();
  testTypeAdvantageLightlyChangesDamage();
  testTypeEffectHelperHandlesDualTypes();
  testCounterDamageStillUsesTurnCap();
  testTurnBattleEndsAtRoundLimitByRemainingHp();

  if (failures) {
    std::cerr << failures << " Testfehler\n";
    return EXIT_FAILURE;
  }

  std::cout << "Alle battle.cpp-Tests bestanden\n";
  return EXIT_SUCCESS;
}
