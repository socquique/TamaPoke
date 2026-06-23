#include <cstdlib>
#include <iostream>
#include <string>

#include "pet.h"

uint32_t gMockMillis = 0;
SerialMock Serial;

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

static Pet hatchedPet(int16_t dex) {
  Pet pet;
  pet.chooseStarter(dex);
  pet.eggTap();
  pet.eggTap();
  pet.eggTap();
  return pet;
}

static void testEggHatchesChosenStarter() {
  Pet pet = hatchedPet(4);

  EXPECT_TRUE(!pet.isEgg());
  EXPECT_EQ(pet.speciesId, 4);
  EXPECT_TRUE(pet.isRegistered(4));
  EXPECT_EQ(pet.registeredCount(), 1);
}

static void testBattleStatsUseBaseGenesLevelAndTraining() {
  Pet pet = hatchedPet(4);
  pet.ageMinutes = 15 * MINUTES_PER_LEVEL;
  pet.geneAtk = 110;
  pet.geneDef = 90;
  pet.geneSpe = 100;
  pet.trAtk = 7;
  pet.trDef = 3;
  pet.trSpe = 11;

  EXPECT_EQ(pet.level(), 16);
  EXPECT_EQ(pet.atkStat(), 52 * 110 / 100 + 16 + 7);
  EXPECT_EQ(pet.defStat(), 43 * 90 / 100 + 16 + 3);
  EXPECT_EQ(pet.speStat(), 65 + 16 + 11);
}

static void testEvolutionRequiresLevelAndHealthyStats() {
  Pet pet = hatchedPet(4);
  pet.ageMinutes = 15 * MINUTES_PER_LEVEL;
  pet.fullness = 39;

  EXPECT_TRUE(!pet.canEvolveNow());

  pet.fullness = 40;
  EXPECT_TRUE(pet.canEvolveNow());

  pet.evolve();
  EXPECT_EQ(pet.prevSpeciesId, 4);
  EXPECT_EQ(pet.speciesId, 5);
  EXPECT_TRUE(pet.isRegistered(5));
}

static void testCareMistakeDelaysEvolution() {
  Pet pet = hatchedPet(4);
  pet.ageMinutes = 15 * MINUTES_PER_LEVEL;
  pet.careMistakes = 1;

  EXPECT_TRUE(!pet.canEvolveNow());

  pet.ageMinutes = 16 * MINUTES_PER_LEVEL;
  EXPECT_TRUE(pet.canEvolveNow());
}

static void testTrainingRewardsClampAndAffectNeeds() {
  Pet pet = hatchedPet(7);
  pet.trAtk = 95;
  pet.energy = 50;
  pet.fullness = 50;
  pet.weight = 30;

  uint8_t gain = pet.trainStrength(100);

  EXPECT_EQ(gain, 18);
  EXPECT_EQ(pet.trAtk, 100);
  EXPECT_EQ(pet.energy, 38);
  EXPECT_EQ(pet.fullness, 45);
  EXPECT_EQ(pet.weight, 0);
}

static void testCatchRewardTrainsSpeedAndRecord() {
  Pet pet = hatchedPet(7);
  pet.trSpe = 98;
  pet.energy = 20;
  pet.fullness = 9;
  pet.catchHi = 12;

  uint8_t gain = pet.applyCatchResult(18);

  EXPECT_EQ(gain, 6);
  EXPECT_EQ(pet.trSpe, 100);
  EXPECT_EQ(pet.catchHi, 18);
  EXPECT_TRUE(pet.energy >= 5);
  EXPECT_TRUE(pet.fullness >= 5);

  pet.applyCatchResult(5);
  EXPECT_EQ(pet.catchHi, 18);
}

static void testMemoRewardTrainsDefenseAndRecord() {
  Pet pet = hatchedPet(1);
  pet.trDef = 99;
  pet.energy = 12;
  pet.fullness = 7;
  pet.memoHi = 4;
  pet.bond = 1;

  uint8_t gain = pet.applyMemoResult(9);

  EXPECT_EQ(gain, 4);
  EXPECT_EQ(pet.trDef, 100);
  EXPECT_EQ(pet.memoHi, 9);
  EXPECT_TRUE(pet.energy >= 5);
  EXPECT_TRUE(pet.fullness >= 5);
  EXPECT_TRUE(pet.bond >= 3);

  pet.applyMemoResult(3);
  EXPECT_EQ(pet.memoHi, 9);
}

static void testPetEventsRewardAndClamp() {
  Pet pet = hatchedPet(4);
  pet.fullness = 95;
  pet.joy = 98;

  EXPECT_TRUE(pet.applyPetEvent(PET_EVENT_BERRY));
  EXPECT_EQ(pet.fullness, 100);
  EXPECT_EQ(pet.joy, 100);

  pet.bond = 99;
  pet.joy = 95;
  EXPECT_TRUE(pet.applyPetEvent(PET_EVENT_HEART));
  EXPECT_EQ(pet.bond, 100);
  EXPECT_EQ(pet.joy, 100);

  pet.energy = 98;
  pet.hygiene = 50;
  pet.joy = 97;
  EXPECT_TRUE(pet.applyPetEvent(PET_EVENT_SPARKLE));
  EXPECT_EQ(pet.joy, 100);
  EXPECT_EQ(pet.hygiene, 53);
}

static void testPetEventsBlockedForEggAndCeremony() {
  Pet egg;
  EXPECT_TRUE(!egg.applyPetEvent(PET_EVENT_BERRY));

  Pet pet = hatchedPet(7);
  pet.ceremony = CER_FAREWELL;
  EXPECT_TRUE(!pet.applyPetEvent(PET_EVENT_HEART));
}

static void testPersonalityIsDerivedWithoutMutating() {
  Pet pet = hatchedPet(4);
  uint8_t joy = pet.joy;
  uint8_t energy = pet.energy;
  uint8_t fullness = pet.fullness;
  uint16_t wins = pet.battleWins;

  EXPECT_EQ(pet.personality(), PERS_BALANCED);
  EXPECT_EQ(pet.joy, joy);
  EXPECT_EQ(pet.energy, energy);
  EXPECT_EQ(pet.fullness, fullness);
  EXPECT_EQ(pet.battleWins, wins);

  Pet playful = hatchedPet(7);
  playful.catchHi = 18;
  EXPECT_EQ(playful.personality(), PERS_PLAYFUL);

  Pet brave = hatchedPet(25);
  brave.battleWins = 8;
  EXPECT_EQ(brave.personality(), PERS_BRAVE);

  Pet calm = hatchedPet(1);
  calm.bond = 45;
  calm.careMistakes = 1;
  EXPECT_EQ(calm.personality(), PERS_CALM);

  Pet lazy = hatchedPet(6);
  lazy.weight = 72;
  lazy.battleWins = 20;
  EXPECT_EQ(lazy.personality(), PERS_LAZY);
}

static void testDailyGoalsProgressAndReset() {
  Pet pet = hatchedPet(4);
  pet.setClock(86400);
  pet.ensureDailyGoals();

  EXPECT_EQ(pet.dailyGoalType[0], DAILY_GOAL_CARE);
  EXPECT_EQ(pet.dailyGoalType[1], DAILY_GOAL_PLAY);
  EXPECT_EQ(pet.dailyGoalType[2], DAILY_GOAL_CATCH);
  EXPECT_TRUE(!pet.dailyGoalComplete(0));

  pet.feedCandy();
  EXPECT_EQ(pet.dailyGoalProgress[0], 1);
  EXPECT_TRUE(pet.dailyGoalComplete(0));

  pet.playResult(10);
  EXPECT_EQ(pet.dailyGoalProgress[1], 1);
  EXPECT_TRUE(pet.dailyGoalComplete(1));

  pet.applyCatchResult(3);
  EXPECT_EQ(pet.dailyGoalProgress[2], 3);
  EXPECT_TRUE(!pet.dailyGoalComplete(2));
  pet.applyCatchResult(3);
  EXPECT_EQ(pet.dailyGoalProgress[2], 5);
  EXPECT_TRUE(pet.dailyGoalComplete(2));

  pet.setClock(2 * 86400);
  pet.ensureDailyGoals();
  EXPECT_EQ(pet.dailyGoalDay, 2U);
  EXPECT_EQ(pet.dailyGoalDone, 0);
  EXPECT_EQ(pet.dailyGoalProgress[0], 0);
}

static void testDailyBattleGoalCompletesOnWinOnly() {
  Pet pet = hatchedPet(4);
  pet.setClock(3 * 86400);
  pet.ensureDailyGoals();

  EXPECT_EQ(pet.dailyGoalType[2], DAILY_GOAL_BATTLE);
  pet.applyBattleLoss();
  EXPECT_EQ(pet.dailyGoalProgress[2], 0);
  EXPECT_TRUE(!pet.dailyGoalComplete(2));

  pet.applyBattleWin(66, false);
  EXPECT_EQ(pet.dailyGoalProgress[2], 1);
  EXPECT_TRUE(pet.dailyGoalComplete(2));
}

static void testCaughtDexIsSeparateFromRaisedDex() {
  Pet pet = hatchedPet(4);

  EXPECT_EQ(pet.caughtCount(), 0);
  EXPECT_TRUE(!pet.isRegistered(66));

  pet.registerCaught(66);
  EXPECT_TRUE(pet.isCaught(66));
  EXPECT_TRUE(!pet.isRegistered(66));
  EXPECT_EQ(pet.caughtCount(), 1);

  pet.registerCaught(66);
  EXPECT_EQ(pet.caughtCount(), 1);
}

static void testCatchChanceAndRolls() {
  Pet pet = hatchedPet(4);
  pet.bond = 40;

  uint8_t common = pet.catchChanceForWild(66, 5, 5, false);
  uint8_t rare = pet.catchChanceForWild(95, 5, 5, false);
  uint8_t highLevel = pet.catchChanceForWild(66, 12, 5, false);
  uint8_t close = pet.catchChanceForWild(66, 5, 5, true);

  EXPECT_TRUE(common > rare);
  EXPECT_TRUE(highLevel < common);
  EXPECT_TRUE(close > common);
  EXPECT_EQ(pet.catchChanceForWild(144, 5, 5, true), 0);

  EXPECT_TRUE(pet.tryCatchWild(66, 5, 5, false, common - 1));
  EXPECT_TRUE(pet.isCaught(66));
  EXPECT_TRUE(!pet.tryCatchWild(95, 5, 5, false, 99));
  EXPECT_TRUE(!pet.isCaught(95));
  EXPECT_TRUE(!pet.tryCatchWild(144, 5, 5, true, 0));
  EXPECT_TRUE(!pet.isCaught(144));
}

static void testCareBonusCapsStreakContribution() {
  Pet pet;
  pet.streak = 99;
  pet.bond = 100;

  EXPECT_EQ(pet.careBonus(), 14);
}

static void testFarewellAndRunawayReadiness() {
  Pet pet = hatchedPet(6);
  pet.ageMinutes = FAREWELL_AGE_MIN;

  EXPECT_TRUE(pet.canFarewellNow());

  pet.sleeping = true;
  EXPECT_TRUE(!pet.canFarewellNow());

  pet.sleeping = false;
  pet.dbgRunawayReady();
  EXPECT_TRUE(pet.canRunawayNow());
}

static void testBattleRewardsAndProgression() {
  Pet pet = hatchedPet(25);
  pet.trDef = 99;
  pet.joy = 50;
  pet.energy = 25;
  pet.fullness = 12;
  pet.bond = 4;

  BattleReward common = pet.applyBattleWin(66, false);  // MACHOP: high ATK -> trains DEF

  EXPECT_EQ(common.stat, BATTLE_REWARD_DEF);
  EXPECT_EQ(common.amount, 1);
  EXPECT_EQ(pet.trDef, 100);
  EXPECT_EQ(pet.joy, 58);
  EXPECT_EQ(pet.energy, 20);
  EXPECT_EQ(pet.fullness, 10);
  EXPECT_TRUE(pet.bond >= 6);
  EXPECT_EQ(pet.battleWins, 1);
  EXPECT_EQ(pet.battleStreak, 1);
  EXPECT_EQ(pet.bestBattleStreak, 1);

  pet.trAtk = 98;
  BattleReward rare = pet.applyBattleWin(95, false);  // ONIX: rare, high DEF -> trains ATK

  EXPECT_EQ(rare.stat, BATTLE_REWARD_ATK);
  EXPECT_EQ(rare.amount, 2);
  EXPECT_EQ(pet.trAtk, 100);
  EXPECT_EQ(pet.battleWins, 2);
  EXPECT_EQ(pet.battleStreak, 2);
  EXPECT_EQ(pet.bestBattleStreak, 2);

  BattleReward close = pet.applyBattleWin(100, true);  // VOLTORB: high SPE -> trains SPE
  EXPECT_EQ(close.stat, BATTLE_REWARD_SPE);
  EXPECT_EQ(close.amount, 2);
  EXPECT_EQ(pet.battleStreak, 3);
  EXPECT_EQ(pet.bestBattleStreak, 3);

  pet.joy = 21;
  pet.energy = 22;
  pet.fullness = 11;
  pet.applyBattleLoss();

  EXPECT_EQ(pet.battleLosses, 1);
  EXPECT_EQ(pet.battleStreak, 0);
  EXPECT_EQ(pet.bestBattleStreak, 3);
  EXPECT_EQ(pet.joy, 20);
  EXPECT_EQ(pet.energy, 20);
  EXPECT_EQ(pet.fullness, 10);
}

int main() {
  testEggHatchesChosenStarter();
  testBattleStatsUseBaseGenesLevelAndTraining();
  testEvolutionRequiresLevelAndHealthyStats();
  testCareMistakeDelaysEvolution();
  testTrainingRewardsClampAndAffectNeeds();
  testCatchRewardTrainsSpeedAndRecord();
  testMemoRewardTrainsDefenseAndRecord();
  testPetEventsRewardAndClamp();
  testPetEventsBlockedForEggAndCeremony();
  testPersonalityIsDerivedWithoutMutating();
  testDailyGoalsProgressAndReset();
  testDailyBattleGoalCompletesOnWinOnly();
  testCaughtDexIsSeparateFromRaisedDex();
  testCatchChanceAndRolls();
  testCareBonusCapsStreakContribution();
  testFarewellAndRunawayReadiness();
  testBattleRewardsAndProgression();

  if (failures) {
    std::cerr << failures << " Testfehler\n";
    return EXIT_FAILURE;
  }

  std::cout << "Alle pet.cpp-Tests bestanden\n";
  return EXIT_SUCCESS;
}
