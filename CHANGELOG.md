# Changelog

All notable changes to the firmware. Versions match the number shown at the
bottom of the clock/settings screen (swipe down) and `web/manifest.json`.

Updating from the [web installer](https://socquique.github.io/TamaPoke/web/)
**without** ticking "Erase device" keeps your Pokémon.

## [1.16] - 2026-09-09

### Fixed

- **Japanese was silently dropping every `！` and `？`.** 21 exclamation marks
  and 4 question marks — a quarter of the Japanese strings, including the mood
  line on the main screen (`ごきげん！` rendered as `ごきげん`). The cause is that
  `u8g2_font_unifont_t_japanese1` carries kana and kanji but no CJK punctuation
  at all — no `！？。、「」` — and `Arduino_GFX` draws nothing for a glyph it can't
  find, without even advancing the cursor, so the loss is invisible in the
  source and on screen. Switched to `japanese3`, the only U8g2 Japanese font
  that has them. Same unifont family: the metrics header is byte-identical and
  all 226 codepoints the firmware uses have byte-identical bitmaps, so nothing
  moves. Costs 102 KB of flash (705 KB → 810 KB, 22% → 25% of the app
  partition); RAM is unchanged.
- **Nidoran♀ and Nidoran♂ both showed as ニドラン in Japanese**, leaving #29
  and #32 indistinguishable in the Pokédex. No U8g2 unifont subset carries
  ♀ (U+2640) or ♂ (U+2642) — not `japanese*`, `korean*` or `chinese*` — and
  `Arduino_GFX` draws nothing for a glyph it can't find, without even
  advancing the cursor. `gen_names.py` already substituted F and M on the
  Latin path; it now does the same on the UTF-8 one, so the names read
  ニドランF and ニドランM. Convention chosen by @usakomint, who preferred it
  over ニドラン(女)/(男) as closer to a species name and shorter on a small
  round screen.

## [1.15] - 2026-09-08

### Added

- **Japanese.** UI, medals and all 151 species names in katakana (フシギダネ,
  ピカチュウ). Selectable from the settings screen like any other language.

### Changed

- Text measurement and cursor positioning are now centralized (`textW()`,
  `centerX()`, `setCur()`, `setSize()`, `printT()`), replacing ~180 sites that
  assumed one byte per character and a fixed 6 px width. Latin languages render
  identically — the equivalence is arithmetic, not approximate. This is what
  makes CJK possible without a per-language branch at every draw site, and
  Chinese and Korean now only need their strings.
- Bar positions on the stat card and the care row are derived from the width of
  the widest translated label instead of fixed coordinates, so they stay aligned
  in any language. With Latin labels the numbers come out identical to before.
- New dependency: **U8g2**, for its CJK font data only.

Japanese font handling, readability and layout were all tuned against real
hardware by @usakomint, who found and diagnosed every issue in the process.

## [1.11] - 2026-09-07

### Fixed

- **The whole loop froze for exactly 1 second when touching the screen.**
  `touch.getPoint()` hung on a sleeping CST9217 for the ESP32 I2C driver's
  default 1000 ms timeout, which `Wire.setTimeOut(50)` doesn't bound because
  SensorLib doesn't route through it. Most visible in the ball minigame, where
  taps come fast and two or three stalls ran together — reported as "freezes for
  2–3 seconds on ball tap". An address-only Wire transaction before the read
  fixes it. Measured on hardware over equivalent play (~470 touch reads): 5
  stalls in 60 s before, 0 after. Reported by @gingsiro.

## [1.10] - 2026-09-07

### Fixed

- TPK2 sprites with zero-length frame durations spun `pmdFrameAt()` forever,
  hanging the render loop. These files arrive over USB, so they aren't trusted
  input: durations are clamped at load time and the loop is bounded regardless.
- `thumbs.bin` was mapped into PSRAM without bounding its size or checking that
  the offsets it contains point inside what was actually read. A truncated file
  (plausible — the transfer takes minutes) could read past the allocation.
- The serial `PUT` handler ignored `f.write()`'s return value, so a full SD card
  still reported `DONE` while leaving a truncated file behind. It also took the
  destination path straight from the serial line, so a crafted `PUT` could write
  anywhere on the card; writes are now confined to `/mons/`.
- Touches just outside the Pokédex grid and the on-screen keyboard were treated
  as cell 0. The guard divided first and then checked for a negative index, but
  integer division truncates toward zero, so it never saw one. On a round screen
  that strip is exactly where a finger lands near the edge.

All four from @ajtudela.

## [1.9] - 2026-09-07

### Fixed

- All ~13 game timers compared `millis()` against an absolute deadline, which
  gives the wrong answer once `millis()` wraps at ~49.7 days of uptime: a timer
  could stay active forever, or a ceremony cut off instantly.
- The sprite transfer restarted from scratch on every attempt. It now records
  each confirmed file, so a reload after a dropped connection doesn't redo the
  full ~40 MB / ~10 min.
- Minigame physics advanced per frame rather than per unit of time, so the ball
  fell measurably slower with a large sprite on screen (Charizard) than a small
  one (Diglett) — high scores weren't comparable between species.

### Security

- `esp-web-tools` was loaded from a floating `@10` range with no integrity
  check. Pinned to 10.4.0 with a verified sha384 SRI hash.

All from @ajtudela.

## [1.8] - 2026-09-07

### Fixed

- **The level counter wrapped to 0 after ~10.6 days of play.** `level()`
  returned `uint8_t` and, at 60 minutes per level, the level is the hour count —
  so it rolled over at 256. Reachable in ordinary play, since "stay together"
  postpones the farewell indefinitely. Now `uint16_t`, capped at 999.
- Two related truncations found while fixing it: `calcStat()` took the level as
  `uint8_t`, so stats would have overflowed anyway; and `canEvolveNow()` compared
  against `(uint8_t)(evolveLevel + careMistakes)`, which wrapped with many care
  mistakes and let evolution happen early. The stat card already computed that
  threshold as `int`, so the screen and the actual check could disagree.

Diagnosed by @ajtudela.

## [1.7] - 2026-09-07

### Fixed

- **The UI had no accents, and the assumption behind that was wrong.** The code
  stated in two places that the font had none. In fact `glcdfont.h` is a full
  256-glyph CP437 table and `write()` doesn't filter bytes ≥ 0x80. Spanish now
  has its tildes and ñ, German its umlauts, French its accents. Characters are
  written as single bytes in octal, never UTF-8, because the UI centres with
  `strlen(t) * 6`. Only strings whose length was unchanged were touched.
  Uppercase accented vowels other than É aren't in CP437, so `EVOLUCION` and
  `PROGRES` stay unaccented, as do Portuguese ã/õ.
- German grammar, from the same contributor: "Wähle deinen Starter" (accusative),
  "Leb wohl" as two words, "Entwicklung" rather than "Entwickelt", and more.

Discovered by @danielberndt.

## [1.6] - 2026-09-07

### Fixed

- **Short sound effects were inaudible.** The amplifier was switched on 8 ms
  before each effect, far less than the NS4150B needs to settle, so a 35 ms tap
  finished before the speaker woke up. The boot jingle survived because it's
  ~440 ms — which is why sound seemed to work at startup and nowhere else. The
  amplifier now stays powered while sound is on and the pet is awake, matching
  Waveshare's reference. Notes and amplitude are unchanged: this doesn't alter
  the sound design, it just makes it audible.

Diagnosed by @djyf1.

## [1.5] - 2026-08-19

### Added

- Localized Pokémon names in French and German (Bulbizarre, Bisasam…). Only
  those two differ in gen 1; Spanish, Italian and Portuguese officially use the
  English names. Names come from PokéAPI via `tools/gen_names.py`.

## [1.4] - 2026-08-07

### Fixed

- **Bond was mathematically stuck.** It could gain at most 12 points a day, but
  a single care mistake cost 3 and could repeat every 30 minutes — up to 144 a
  day. With poops draining hygiene, mistakes chained and bond never recovered.
  Two players independently reported it pinned at exactly 10, which is the
  first-day ceiling of 13 minus one mistake. Daily cap 8 → 20, penalty 3 → 1,
  cooldown 30 → 60 min.

## [1.3] - 2026-08-07

### Fixed

- **The egg hatched by itself during starter selection.** Game time kept running
  while the starter screen was open, so taking more than 3 minutes to choose
  meant the egg hatched into the species already rolled — no egg animation, and
  the player's choice ignored. Three community reports matched (Pikachu twice,
  Eevee once). Time now freezes until a starter is chosen.

## [1.2] - 2026-06

### Added

- Firmware version shown at the bottom of the clock/settings screen.
