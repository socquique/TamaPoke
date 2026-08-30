# TamaPoke for M5Stack Cardputer ADV

> **PORT / ADAPTATION — NOT THE ORIGINAL TAMA POKE PROJECT**
>
> This repository is an unofficial **M5Stack Cardputer ADV port** of **TamaPoke**, originally developed by **socquique**.
> The original project, pet engine, game design, Pokédex data, and core TamaPoke work belong to the original developer.
>
> **Original TamaPoke:** https://github.com/socquique/TamaPoke
>
> This repository's contribution is the Cardputer ADV hardware/UI adaptation, low-memory PMD streaming, keyboard controls, display layout, and Cardputer-specific persistence work.


This port adapts the original 466×466 touchscreen/AMOLED experience to the **M5Stack Cardputer ADV** with its 240×135 ST7789 display, physical keyboard, speaker, and microSD slot.

> **Current release:** v0.7  
> **Target:** M5Stack Cardputer ADV  
> **Upstream TamaPoke commit pinned by this port:** `fdb24a7d19564ee641c9a7dfc776f6bce11cd78b`

## Highlights

- 151 Generation-1 Pokémon
- Normal and shiny PMD SpriteCollab animations
- Starter selection: Bulbasaur / Charmander / Squirtle
- Feeding, happiness, energy, hygiene, sleep and cleaning
- Egg hatching and evolution
- Pokédex gallery
- Pet profile, battle stats, medals and progress pages
- Pokéball minigame
- Strength-training punching bag
- Time-of-day habitats: dawn, day, sunset and night
- Meadow, beach, forest, volcano, mountain and snow biomes
- Speaker sound effects
- Buffered full-screen rendering to reduce flicker
- Cardputer-native keyboard navigation
- Automatic microSD save journal with restart recovery

## Cardputer ADV controls

| Control | Action |
|---|---|
| Left / Right arrow keycaps | Select bottom action |
| Enter | Activate selected action |
| Space | Pet Pokémon / minigame action |
| Up | Pet card |
| Down | Settings |
| D | Pokédex |
| E | Evolution when available |
| G | Farewell / runaway when available |
| N | Rename |
| R | Release |
| Esc / top-left key | Back |
| Del / Backspace | Back |

The printed Cardputer arrow keycaps work directly without requiring Fn.

## microSD setup

TamaPoke sprite data is **not distributed in this repository**.

Generate the sprite packs from the original TamaPoke project and copy the resulting `mons` folder to the root of a FAT32 microSD:

```text
SD CARD/
└── mons/
    ├── p001.bin
    ├── ps001.bin
    ├── ...
    ├── p151.bin
    └── ps151.bin
```

The port streams one PMD animation frame at a time because Cardputer ADV does not include PSRAM.

### Save files

v0.7 automatically creates two rotating save slots on the microSD:

```text
/tamapoke_v7_a.bin
/tamapoke_v7_b.bin
```

Keep the same microSD inserted when using TamaPoke. Important actions save immediately, and changed state is checked for background autosave about every 2 seconds.

## One-click Windows build

1. Install VS Code + PlatformIO once.
2. Download or clone this repository.
3. Double-click:

```text
00_BUILD_BIN_ONE_CLICK.bat
```

4. Wait for `BUILD SUCCESSFUL`.

The preferred one-file output is:

```text
TamaPoke-CardputerADV-v0.7-MERGED.bin
```

## Manual PlatformIO build

```bash
platformio run -e m5stack-cardputer-adv
```

The build system downloads the pinned upstream TamaPoke `pet.cpp`, `pet.h`, and `dex.h` and applies the Cardputer ADV compatibility/persistence patches before compilation.

## Hardware

- M5Stack Cardputer ADV / Stamp-S3A
- ESP32-S3
- 240×135 ST7789 display
- TCA8418 keyboard
- microSD
- built-in speaker

microSD SPI pins used by this port:

| Signal | GPIO |
|---|---:|
| CS | 12 |
| MOSI | 14 |
| SCK | 40 |
| MISO | 39 |

## M5Burner

A release-ready merged BIN can be shared through M5Burner **USER CUSTOM → Publish**.

Recommended listing:

**Name:** `TamaPoke Cardputer ADV`  
**Version:** `0.7`  
**Device:** `Cardputer ADV`  
**Framework:** `Arduino / PlatformIO`

The firmware requires a microSD containing the generated `/mons` sprite directory.

## Credits

- Original TamaPoke project and pet engine: **socquique**
- Original project: https://github.com/socquique/TamaPoke
- Pokémon Mystery Dungeon SpriteCollab sprite data is maintained by its respective contributors.
- M5Stack / M5Cardputer libraries by M5Stack.

## Licensing and trademark notice

The upstream TamaPoke source is MIT licensed. See `LICENSE-UPSTREAM`.

Port-specific source code in this repository is released under the MIT License; see `LICENSE`.

**Pokémon, Pokémon character names, artwork, and related trademarks are property of Nintendo, Game Freak, and The Pokémon Company.** This is an unofficial fan project and is not affiliated with or endorsed by those companies.

Pokémon sprite assets are not included in this repository or firmware package. Users must obtain/generate compatible sprite data separately in accordance with the upstream project and sprite-source terms.

## Disclaimer

This is a community hobby project. Use at your own risk. Back up your microSD save files before experimenting with development builds.
