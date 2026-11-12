# Waveshare ESP32-S3-Touch-AMOLED-1.75C (experimental community support)

The same tree builds for both boards. **The default build is the original
ESP32-S3-Touch-AMOLED-1.75** (microSD, 16 MB flash): CI, the web installer and
an Arduino IDE build without extra flags are unchanged. The 1.75C is selected
with `-DBOARD_AMOLED_175C=1` and its own partition table,
`boards/175c/partitions.csv`. `tools/build_175c.sh` does both, and CI runs that
same script, so the 1.75C build cannot quietly rot.

All 1.75C code is behind `#if BOARD_AMOLED_175C`. On the original board the
pins, SD card, PWR short press, touch handling, training layout and flush path
compile exactly as before.

Tested on a real 1.75C: display, touch, 32 MB flash, LittleFS sprites, the PWR
screen lock, BOOT mute and the strength-training fixes. No original 1.75 was
available to me, so that board is compile-tested only.

## How the boards differ (verified)

Sources: [1.75C pin_config.h](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/blob/main/examples/arduino/libraries/board_config/pin_config.h), [1.75C schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75C/ESP32-S3-Touch-AMOLED-1.75C-schematic.pdf), [1.75 hardware reference](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/HARDWARE_REFERENCE.md).

| Item | Original 1.75 | 1.75C |
|---|---|---|
| LCD reset | GPIO 39 | GPIO **1** |
| Touch reset | GPIO 40 | GPIO **2** |
| I2S MCLK | GPIO 42 | GPIO **16** |
| QSPI / I2C / I2S BCLK, WS, DO, DI / PA | 38, 4–7, 12; 15/14, INT 11; 9, 45, 8, 10; PA 46 | Same |
| PWR button | AXP2101 PEK IRQ | Key2 → AXP2101 **PWRON** (4 s power-off) and **SYS_OUT → GPIO 3** (high while pressed) |
| OLED power | AXP **BLDO1 3.3 V** | Panel J3 is on **VCC3V3 (DCDC1)**. BLDO1 is 1.2 V and BLDO2 2.8 V on this board, so BLDO1 is **not** reprogrammed. |
| RTC PCF85063 | Present (I2C 0x51) | **Not fitted.** The firmware keeps a software clock (`time()`/`settimeofday()`), set from the clock screen; it is lost when the AXP cuts power. |
| microSD | Yes (sprites) | **No slot**: sprites live in a LittleFS partition in flash |
| Flash | 16 MB | **32 MB** (product page, repo and factory image). The schematic still draws a 16 MB part; check with `esptool flash_id` before the first flash. |
| Touch axes | `setMirrorXY(true, true)` | Same (matches the official 1.75C widgets demo) |
| Speaker | External MX1.25 | Onboard (PA still GPIO 46); volume left at the 1.75 value |

The UI is already laid out for a 466×466 round panel, so nothing is rescaled.

## Building

### arduino-cli

```bash
# original 1.75 (default, exactly what CI and the web installer build)
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" .

# 1.75C: -DBOARD_AMOLED_175C=1 + boards/175c/partitions.csv, output in ../out
tools/build_175c.sh --out ./out
tools/build_spritefs.sh ./out/storage.littlefs.bin
tools/flash_175c.sh --port /dev/ttyACM0 --out ./out      # bootloader + table + app + sprites
```

`tools/build_175c.sh` compiles with FQBN
`esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=32M,PSRAM=opi,PartitionScheme=custom`,
adds `-DBOARD_AMOLED_175C=1` to `compiler.cpp.extra_flags`, and points the ESP32
core's partition prebuild hook at `boards/175c/partitions.csv`. It then checks
that the table that went into the build really is that file. Extra compiler
flags can be added with `--flags '-DSOMETHING=1'`.

There is deliberately **no `partitions.csv` in the sketch root**: the core
copies a root `partitions.csv` into every build, whatever scheme is selected,
which would break the original board. `build_175c.sh` refuses to run if one
appears there.

`tools/flash_175c.sh` writes the partition table it generates from
`boards/175c/partitions.csv` with the core's `gen_esp32part.py`, and reads the
app and LittleFS offsets from the same file.

### Arduino IDE

1. Board **ESP32S3 Dev Module**, Flash Size **32MB (256Mb)**, PSRAM **OPI PSRAM**,
   USB CDC On Boot **Enabled**.
2. Partition Scheme **Custom**, and temporarily copy `boards/175c/partitions.csv`
   into the sketch folder (remove it again before building for the original 1.75).
3. Add `-DBOARD_AMOLED_175C=1` to the extra compile flags, or change the
   fallback in `board.h` locally.
4. The IDE won't upload LittleFS, so flash the sprites separately:
   `tools/build_spritefs.sh` then `tools/flash_175c.sh --port <port> --fs-only`.

### Flash map (32 MB, `boards/175c/partitions.csv`)

| Region | Offset | Size |
|---|---|---|
| bootloader | `0x0` | |
| partition table | `0x8000` | |
| `nvs` (pet save, settings) | `0x9000` | 20 KB |
| `otadata` / boot_app0 | `0xe000` | 8 KB |
| `app0` | `0x10000` | 3 MB (same budget as the original scheme) |
| `storage` (LittleFS sprites) | `0x310000` | 28.875 MB |
| `coredump` | `0x1FF0000` | 64 KB |

**App-only update** (keeps the pet, settings and sprites): write only
`TamaPoke.ino.bin` at `0x10000`, e.g.
`esptool --chip esp32s3 --port <port> --baud 921600 write-flash 0x10000 TamaPoke.ino.bin`.
Don't use `--erase-flash`, and don't write `0x9000` (NVS) or `0x310000`
(LittleFS). `flash_175c.sh --fw-only` also rewrites the bootloader and
partition table, which is harmless when they're unchanged but isn't app-only.

## Sprite storage trade-off

The repo pack at `tools/sdcard/mons/` is about **41.7 MB** with shinies and
**21.0 MB** for the 151 normal sprites plus `thumbs.bin`. That can't fit next to
a 3 MB app in 32 MB, so the 1.75C image (`tools/build_spritefs.sh`) holds **all
151 normal sprites and `thumbs.bin`** in the 28.875 MB `storage` partition.
Shiny forms fall back to the normal sprite (the firmware already did that when a
shiny file was missing). Roughly 7 MB is left for individual shiny files sent
over USB with `tools/send_sd.py`.

`sdBegin()` mounts LittleFS at `/littlefs` (label `storage`) and **does not
auto-format**: a failed mount means the image hasn't been flashed yet. Paths
such as `/mons/p001.bin` are unchanged, and the USB `PUT`/`LS`/`SDINFO` commands
work against LittleFS.

## File-by-file changes

| File | Change |
|---|---|
| `board.h` | **New.** `BOARD_AMOLED_175C`, default `0` (original 1.75). |
| `boards/175c/partitions.csv` | **New.** 32 MB map above. Only used when passed explicitly. |
| `pin_config.h` | 1.75C pins (LCD/touch reset, MCLK, PWR sense, BOOT) behind the switch; original pins unchanged. |
| `sdmon.cpp` / `sdmon.h` | File I/O goes through `SpriteFS`: LittleFS on the 1.75C, still `SD_MMC` on the 1.75. |
| `rtcbat.cpp` / `rtcbat.h` | 1.75C only: no PCF85063 probe, software clock, BLDO1 left alone, PEK IRQ off and GPIO 3 set up for the lock. The 4 s AXP power-off is kept. Original paths unchanged. |
| `screenlock.cpp` / `screenlock.h` | **New.** PWR triple-press detector and lock state (pure logic, compile-time and host tests). |
| `bootbutton.cpp` / `bootbutton.h` | **New.** BOOT single/double/long gesture detector (pure logic, tests). |
| `trainingtouch.*` / `trainingcapture.*` | **New.** 1.75C strength-training tap policy, CST9217 report decoder and the capture worker. |
| `i18n.cpp` / `i18n.h` | One new string, `S_HOLD_EXIT`, in all 8 languages. |
| `TamaPoke.ino` | 1.75C-only lock, BOOT, overlay and training paths; boot banner prints `BOARD_NAME`. |
| `tools/build_175c.sh` | **New.** 1.75C build (flag + partition table), also used by CI. |
| `tools/flash_175c.sh` | **New.** esptool wrapper; table and offsets from `boards/175c/partitions.csv`. |
| `tools/build_spritefs.sh` | **New.** Builds `storage.littlefs.bin` from the normal pack. |
| `test/` | Host tests for the BOOT, lock and training logic. |
| `.github/workflows/ci.yml` | Second compile step for the 1.75C. |

## BOOT button (1.75C)

BOOT / Key1 is **GPIO 0**, active low with `INPUT_PULLUP`.

- **Single press:** toggles sound, using the existing `tamapoke/snd` setting
  shared with the Settings toggle. It fires 350 ms after release, so a double
  press doesn't mute by accident. Shows `Muted` / `Sound on` for 1.2 s.
- **Double press / long press (1.5 s):** detected but unused in this build (they
  log over serial). They are kept free for the separate time-sync work.

Edges are debounced for 25 ms. A button already held at boot is ignored until
released. GPIO 0 is a strapping pin: holding BOOT while resetting enters the ROM
download mode, as on any ESP32-S3.

## Pocket screen lock (1.75C)

**Press PWR three times to lock, three more to unlock.** Each press must be
under **600 ms**, with the next one starting within **350 ms** of the previous
release; edges are debounced for **25 ms**. Single and double presses do
nothing, and a hold of 600 ms or more cancels a partial triple.

**Hardware.** Key2 pulls AXP2101 PWRON low and, through T1 (BSS138), drives
SYS_OUT to **GPIO 3** (high = pressed, 10 k pull-up R9). The firmware samples
GPIO 3 every loop instead of the AXP PEK IRQ, which the 1.75C doesn't enable.
**Holding PWR for about 4 s still powers the board off** (AXP register 0x27
bits [3:2] = 00, set with `setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S)`).

**While locked:**

- Touch and BOOT are ignored straight away, and any half-finished gesture is
  thrown away. The touch interrupt is detached and the CST9217 isn't read.
- `Locked` shows for 600 ms, then brightness goes to 0 and the CO5300 gets
  DISPOFF + SLPIN. DCDC1 stays on (it also powers the ESP32).
- No frames are sent. Auto-dim, pet events and serial feedback can't wake the
  panel. The pet, the clock, battery checks and deferred saves keep running; on
  battery the usual 200 ms light sleep continues, with GPIO 3 as a wake source.
- Unlocking sends DISPON + SLPOUT, restores brightness and shows `Unlocked` for
  1.2 s. A finger or BOOT press held through the unlock is ignored until released.
- The lock lives in RAM only, so the board always boots unlocked. No new NVS keys.

`LOCK?` over serial prints e.g. `LOCK locked=1 panel_sleep=1 pwr=0` (read-only).
`HEALTH` adds `locked=` on the 1.75C.

## Strength training (1.75C)

On the original layout, any touch with y < 72 (an invisible strip, no button)
quit training, so fast taps near the top often ended the session. Taps that
started and ended while a frame was being drawn were also lost.

On the 1.75C:

- Almost the whole round screen counts as a hit, on touch-down, including the
  old top strip.
- You leave with a visible **Hold to exit** button at x 153–312, y 384–431. Hold
  it for **1 s**; its fill shows progress. Quick taps on it neither quit nor
  score. The 10 s session and 3.5 s result screen are unchanged.
- `trainingcapture` keeps polling the CST9217 at the usual **20 ms** while the
  frame is drawn and flushed (that part of the frame does no I2C), and queues
  timestamped hits. An RAII guard joins the worker before the frame returns, so
  nothing else races it on `Wire`. Hits are scored by capture time, so a tap
  just before the deadline still counts.
- Empty/ACK reports count as "no contact", as with SensorLib's `getPoint()`,
  and the first active finger is picked the same way SensorLib does. A no-contact
  sample is needed before the next hit, so a held finger scores once.
- If the worker can't be created, training won't start and says so.

On hardware this went from about 48 to **57 taps** in a session with no
noticeable lag. `TRAINTRACE ON` / `OFF` / `?` prints raw CST9217 reports
(`TRAINRAW ...`) for diagnosis; it's off by default and not saved.

The original board keeps its existing touch polling, training layout and
y < 72 exit; the worker and trace commands are compiled out.

## Tests

- `./test/run_tests.sh` adds `test_bootbutton.cpp`, `test_screenlock.cpp` and
  `test_trainingtouch.cpp` to the host suite.
- `bootbutton.cpp`, `screenlock.cpp` and `trainingtouch.cpp` also carry
  `static_assert` scenarios, so every firmware build checks them.

## Remaining checks

- Panel start-up after a completely flat battery / cold AXP reset.
- Onboard speaker level (ES8311 volume left at the 1.75 value).
- A regression run on a real original 1.75.
