Import("env")
from pathlib import Path
from urllib.request import Request, urlopen
import hashlib
import re

UPSTREAM_COMMIT = "fdb24a7d19564ee641c9a7dfc776f6bce11cd78b"
BASE = f"https://raw.githubusercontent.com/socquique/TamaPoke/{UPSTREAM_COMMIT}"

PROJECT = Path(env.subst("$PROJECT_DIR"))
TARGETS = {
    "pet.cpp": PROJECT / "generated" / "pet.cpp",
    "pet.h": PROJECT / "include" / "pet.h",
    "dex.h": PROJECT / "include" / "dex.h",
}


def download(name, dest):
    url = f"{BASE}/{name}"
    req = Request(url, headers={"User-Agent": "TamaPoke-CardputerADV-Port/0.7"})
    try:
        with urlopen(req, timeout=25) as r:
            data = r.read()
    except Exception as exc:
        if dest.exists() and dest.stat().st_size > 100:
            print(f"[TamaPoke] Network unavailable; using cached {dest.name}")
            return
        print(f"\n[TamaPoke] ERROR: could not download {url}\n{exc}")
        print("[TamaPoke] Connect this computer to the internet and Build again.")
        env.Exit(1)
        return

    dest.parent.mkdir(parents=True, exist_ok=True)
    old = dest.read_bytes() if dest.exists() else None
    if old != data:
        dest.write_bytes(data)
        digest = hashlib.sha256(data).hexdigest()[:12]
        print(f"[TamaPoke] fetched {name} ({len(data)} bytes, sha256 {digest}...)")
    else:
        print(f"[TamaPoke] cached {name} is current")


for name, dest in TARGETS.items():
    download(name, dest)


def fail(msg):
    print(f"[TamaPoke] ERROR: {msg}")
    env.Exit(1)


def patch_pet_header():
    h = TARGETS["pet.h"]
    text = h.read_text(encoding="utf-8").replace("\r\n", "\n").replace("\r", "\n")

    if "void saveSd();" not in text:
        # Match the actual private save/load declarations without depending on
        # CRLF/LF or exact indentation width.
        pattern = re.compile(
            r"(?m)^(?P<indent>[ \t]*)void save\(\);[ \t]*\r?\n"
            r"(?P=indent)void load\(\);[ \t]*$"
        )

        def repl(m):
            indent = m.group("indent")
            return (
                f"{indent}void save();\n"
                f"{indent}void load();\n"
                f"{indent}void saveSd();  // Cardputer ADV v0.7 direct microSD journal\n"
                f"{indent}bool loadSd();  // loaded before starter/new-game decision"
            )

        text, count = pattern.subn(repl, text, count=1)
        if count != 1:
            fail("could not patch pet.h save/load declarations")
            return

    h.write_text(text, encoding="utf-8", newline="\n")


def patch_pet_cpp():
    p = TARGETS["pet.cpp"]
    text = p.read_text(encoding="utf-8").replace("\r\n", "\n").replace("\r", "\n")

    if "#include <SD.h>" not in text:
        marker = '#include "audio.h"'
        if marker not in text:
            fail("could not locate pet.cpp include block")
            return
        text = text.replace(
            marker,
            marker + "\n#include <SD.h>\n#include <stddef.h>\n#include <cstring>",
            1,
        )

    # Strip remnants if the build reuses a partially patched cache.
    text = text.replace('#include "persistence.h"\n', '')
    text = text.replace(
        '  petSaveHook();  // Cardputer ADV v0.5: immediate SD journal mirror\n',
        '',
    )

    old_begin = '''void Pet::begin() {
  prefs.begin("tamapoke", false);
  if (!prefs.getBool("init", false)) {
    prefs.putBool("init", true);
    newEgg();
  } else {
    load();
  }
  lastTick = millis();
}'''

    new_begin = '''void Pet::begin() {
  prefs.begin("tamapoke", false);

  // Cardputer ADV v0.7: the microSD journal is the primary restart save.
  // Load it directly into this Pet object BEFORE deciding this is a first run.
  if (loadSd()) {
    save();
    Serial.println("SAVE7: restored direct SD journal");
  } else if (!prefs.getBool("init", false)) {
    prefs.putBool("init", true);
    newEgg();
  } else {
    load();
    saveSd();
  }
  lastTick = millis();
}'''

    if "SAVE7: restored direct SD journal" not in text:
        if old_begin not in text:
            fail("could not patch Pet::begin()")
            return
        text = text.replace(old_begin, new_begin, 1)

    # Upstream caress() intentionally does not always call save(); on Cardputer
    # we still want a sudden restart to keep that joy/bond change.
    old_caress = """void Pet::caress() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 5);
  heartUntil = millis() + HEART_MS;
  addBond(1);
  registerCare();
}"""
    new_caress = """void Pet::caress() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 5);
  heartUntil = millis() + HEART_MS;
  addBond(1);
  registerCare();
  saveSd();  // Cardputer ADV v0.7: petting is saved immediately too
}"""
    if "petting is saved immediately too" not in text:
        if old_caress not in text:
            fail("could not patch Pet::caress() for immediate autosave")
            return
        text = text.replace(old_caress, new_caress, 1)

    # Background autosave catches time/stat changes even when no button action
    # called upstream save(). saveSd() itself skips writes when state is unchanged.
    if "v0.7 background autosave" not in text:
        update_pattern = re.compile(
            r'(void Pet::update\(uint32_t nowMs\) \{\n)'
        )
        text, count = update_pattern.subn(
            r'\1  // Cardputer ADV v0.7 background autosave: at most 2 s of unsaved progress.\n'
            r'  static uint32_t lastAutoSd = 0;\n'
            r'  if (nowMs - lastAutoSd >= 2000) {\n'
            r'    lastAutoSd = nowMs;\n'
            r'    saveSd();\n'
            r'  }\n',
            text,
            count=1,
        )
        if count != 1:
            fail("could not patch Pet::update() background autosave")
            return

    if "same save, same moment, direct to SD" not in text:
        pattern = re.compile(
            r'(?m)^(?P<indent>[ \t]*)prefs\.putString\("nick", nick\);[ \t]*\r?\n\}'
        )

        def save_repl(m):
            indent = m.group("indent")
            return (
                f'{indent}prefs.putString("nick", nick);\n'
                f'{indent}saveSd();  // Cardputer ADV v0.7: same save, same moment, direct to SD\n'
                f'{indent}}}'
            )

        text, count = pattern.subn(save_repl, text, count=1)
        if count != 1:
            fail("could not patch Pet::save()")
            return

    direct_sd_impl = r'''
// ---------------------------------------------------------------------------
// Cardputer ADV v0.7 direct microSD save journal
// ---------------------------------------------------------------------------
namespace {
static constexpr uint32_t TP7_MAGIC = 0x37504B54UL; // "TKP6"
static constexpr uint16_t TP7_VERSION = 1;
static const char *TP7_A = "/tamapoke_v7_a.bin";
static const char *TP7_B = "/tamapoke_v7_b.bin";
static uint32_t tp7LastStateHash = 0;

struct __attribute__((packed)) Tp7State {
  uint8_t fullness, joy, energy, hygiene, poops, weight;
  uint8_t geneAtk, geneDef, geneSpe;
  uint8_t trAtk, trDef, trSpe;
  uint8_t berryKnown, shiny;
  uint32_t ageMinutes;
  int16_t speciesId;
  int16_t prevSpeciesId;
  uint8_t careMistakes;
  uint8_t sleeping;
  uint32_t lastSeenEpoch;
  uint8_t ceremony;
  uint8_t lastEnd;
  uint8_t dexReg[19];
  uint8_t dexShinyReg[19];
  uint16_t streak, bestStreak;
  uint32_t lastCareDay;
  uint8_t bond;
  char nick[12];
  uint16_t medals, totalMedals, lastMilestone;
  uint16_t gameHi, strHi;
  int16_t eggTarget;
  uint8_t eggShiny;
  uint8_t eggTaps;
  uint8_t mistakeCooldown;
  uint8_t ticksSinceSave;
  uint8_t starterPick;
  uint8_t neglectTicks;
  uint16_t goodTicks;
  uint8_t bondToday;
  uint8_t evoDeclinedLv;
  uint32_t farDeclinedAge;
};

struct __attribute__((packed)) Tp7Slot {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t sequence;
  Tp7State state;
  uint32_t crc;
};

static uint32_t tp7Hash(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 16777619UL;
  }
  return h;
}

static uint32_t tp7Crc(const Tp7Slot &s) {
  return tp7Hash(reinterpret_cast<const uint8_t*>(&s), offsetof(Tp7Slot, crc));
}

static bool tp7ReadSlot(const char *path, Tp7Slot &s) {
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  if (f.size() != sizeof(Tp7Slot)) { f.close(); return false; }
  size_t got = f.read(reinterpret_cast<uint8_t*>(&s), sizeof(s));
  f.close();
  if (got != sizeof(s)) return false;
  if (s.magic != TP7_MAGIC || s.version != TP7_VERSION || s.size != sizeof(Tp7Slot)) return false;
  return tp7Crc(s) == s.crc;
}

static bool tp7WriteSlot(const char *path, const Tp7Slot &s) {
  SD.remove(path);
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  size_t wrote = f.write(reinterpret_cast<const uint8_t*>(&s), sizeof(s));
  f.flush();
  f.close();
  if (wrote != sizeof(s)) return false;
  Tp7Slot verify{};
  return tp7ReadSlot(path, verify) && verify.sequence == s.sequence && verify.crc == s.crc;
}

static uint32_t tp7NewestSequence() {
  Tp7Slot a{}, b{};
  bool va = tp7ReadSlot(TP7_A, a);
  bool vb = tp7ReadSlot(TP7_B, b);
  if (va && vb) return a.sequence > b.sequence ? a.sequence : b.sequence;
  if (va) return a.sequence;
  if (vb) return b.sequence;
  return 0;
}
} // namespace

void Pet::saveSd() {
  Tp7Slot s{};
  s.magic = TP7_MAGIC;
  s.version = TP7_VERSION;
  s.size = sizeof(Tp7Slot);
  s.sequence = tp7NewestSequence() + 1;

  Tp7State &d = s.state;
  d.fullness = fullness; d.joy = joy; d.energy = energy; d.hygiene = hygiene;
  d.poops = poops; d.weight = weight;
  d.geneAtk = geneAtk; d.geneDef = geneDef; d.geneSpe = geneSpe;
  d.trAtk = trAtk; d.trDef = trDef; d.trSpe = trSpe;
  d.berryKnown = berryKnown ? 1 : 0; d.shiny = shiny ? 1 : 0;
  d.ageMinutes = ageMinutes; d.speciesId = speciesId; d.prevSpeciesId = prevSpeciesId;
  d.careMistakes = careMistakes; d.sleeping = sleeping ? 1 : 0;
  d.lastSeenEpoch = lastSeenEpoch; d.ceremony = ceremony; d.lastEnd = lastEnd;
  memcpy(d.dexReg, dexReg, sizeof(d.dexReg));
  memcpy(d.dexShinyReg, dexShinyReg, sizeof(d.dexShinyReg));
  d.streak = streak; d.bestStreak = bestStreak; d.lastCareDay = lastCareDay;
  d.bond = bond; memcpy(d.nick, nick, sizeof(d.nick)); d.nick[sizeof(d.nick) - 1] = 0;
  d.medals = medals; d.totalMedals = totalMedals; d.lastMilestone = lastMilestone;
  d.gameHi = gameHi; d.strHi = strHi;
  d.eggTarget = eggTarget; d.eggShiny = eggShiny ? 1 : 0; d.eggTaps = eggTaps;
  d.mistakeCooldown = mistakeCooldown; d.ticksSinceSave = ticksSinceSave;
  d.starterPick = starterPick ? 1 : 0; d.neglectTicks = neglectTicks;
  d.goodTicks = goodTicks; d.bondToday = bondToday; d.evoDeclinedLv = evoDeclinedLv;
  d.farDeclinedAge = farDeclinedAge;

  // Avoid needless SD wear: background autosave can call this often, but a
  // physical write only happens when persistent Pet state actually changed.
  uint32_t stateHash = tp7Hash(reinterpret_cast<const uint8_t*>(&d), sizeof(d));
  if (stateHash == tp7LastStateHash) return;

  s.crc = tp7Crc(s);

  const char *target = (s.sequence & 1) ? TP7_A : TP7_B;
  bool ok = tp7WriteSlot(target, s);
  if (ok) tp7LastStateHash = stateHash;
  Serial.printf("SAVE7: %s slot=%c seq=%lu dex=%d starter=%u age=%lu\n",
                ok ? "OK" : "FAIL", (target == TP7_A) ? 'A' : 'B',
                (unsigned long)s.sequence, (int)speciesId,
                (unsigned)starterPick, (unsigned long)ageMinutes);
}

bool Pet::loadSd() {
  Tp7Slot a{}, b{};
  bool va = tp7ReadSlot(TP7_A, a);
  bool vb = tp7ReadSlot(TP7_B, b);
  if (!va && !vb) {
    Serial.println("SAVE7: no valid direct SD slot");
    return false;
  }

  const Tp7Slot *best = (va && vb) ? ((a.sequence >= b.sequence) ? &a : &b) : (va ? &a : &b);
  const Tp7State &d = best->state;
  fullness = d.fullness; joy = d.joy; energy = d.energy; hygiene = d.hygiene;
  poops = d.poops; weight = d.weight;
  geneAtk = d.geneAtk; geneDef = d.geneDef; geneSpe = d.geneSpe;
  trAtk = d.trAtk; trDef = d.trDef; trSpe = d.trSpe;
  berryKnown = d.berryKnown != 0; shiny = d.shiny != 0;
  ageMinutes = d.ageMinutes; speciesId = d.speciesId; prevSpeciesId = d.prevSpeciesId;
  careMistakes = d.careMistakes; sleeping = d.sleeping != 0;
  lastSeenEpoch = d.lastSeenEpoch; ceremony = d.ceremony; lastEnd = d.lastEnd;
  memcpy(dexReg, d.dexReg, sizeof(dexReg));
  memcpy(dexShinyReg, d.dexShinyReg, sizeof(dexShinyReg));
  streak = d.streak; bestStreak = d.bestStreak; lastCareDay = d.lastCareDay;
  bond = d.bond; memcpy(nick, d.nick, sizeof(nick)); nick[sizeof(nick) - 1] = 0;
  medals = d.medals; totalMedals = d.totalMedals; lastMilestone = d.lastMilestone;
  gameHi = d.gameHi; strHi = d.strHi;
  eggTarget = d.eggTarget; eggShiny = d.eggShiny != 0; eggTaps = d.eggTaps;
  mistakeCooldown = d.mistakeCooldown; ticksSinceSave = d.ticksSinceSave;
  starterPick = d.starterPick != 0; neglectTicks = d.neglectTicks;
  goodTicks = d.goodTicks; bondToday = d.bondToday; evoDeclinedLv = d.evoDeclinedLv;
  farDeclinedAge = d.farDeclinedAge;

  tp7LastStateHash = tp7Hash(reinterpret_cast<const uint8_t*>(&d), sizeof(d));

  eatUntil = 0; heartUntil = 0; evolveUntil = 0; ceremonyUntil = 0;
  medalUntil = 0; milestoneUntil = 0; pendingSave = false;

  Serial.printf("SAVE7: LOAD OK seq=%lu dex=%d starter=%u age=%lu\n",
                (unsigned long)best->sequence, (int)speciesId,
                (unsigned)starterPick, (unsigned long)ageMinutes);
  return true;
}
'''

    if "Cardputer ADV v0.7 direct microSD save journal" not in text:
        text += "\n" + direct_sd_impl + "\n"

    p.write_text(text, encoding="utf-8", newline="\n")


patch_pet_header()
patch_pet_cpp()
print("[TamaPoke] v0.7 direct Pet SD persistence + autosave patch applied")
