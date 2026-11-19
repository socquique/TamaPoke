# Network time, Bluetooth time and timezone (optional)

Everything here is **opt-in**. With no Wi-Fi details, no paired phone and no
timezone set, the firmware behaves exactly as before: you set the clock by hand
on the clock screen, the time shown is the time you set, no radio is switched
on, and the pet's day starts when that clock passes midnight.

All of it is shared code for every board; nothing is board-specific.

## Wi-Fi NTP

Store the details over the USB serial console (115200, NL or CRLF):

```
WIFISSID your-network
WIFIPASS your-password
WIFISYNC        # sync now (also runs at every boot from now on)
WIFI?           # shows the network name and last result, never the password
WIFICLR         # forget them
```

They live only in the board's NVS (namespace `nettime`), never in source or in
the firmware image. If they're set, boot connects (8 s limit), asks SNTP
(`time.google.com`, then `pool.ntp.org`, then a fixed fallback IP; 10 s limit),
writes the result with `rtcSetEpoch()` and turns Wi-Fi off. A result only
counts if it is at or after the firmware's compile date, so the 2026-01-01
seed can't pass for a real time. Then `pet.syncClock()` applies the minutes
the board was off, with the existing two-week cap. Without details, or if it
fails, boot is the same as today.

`WIFISYNC` queues the same flow from the loop. It blocks the UI for at most the
limits above (plus DNS); the result is printed on serial.

## Bluetooth LE time from a phone

```
BTPAIR    # 120 s pairing window (Just Works, no PIN); existing bonds kept
BTSYNC    # 45 s sync window for an already-paired phone
BT?       # state, bond count, last result
BTCLR     # forget all bonds (also forget TamaPoke on the phone)
```

Pair once: send `BTPAIR`, then on the phone open **Settings > Bluetooth**, tap
**TamaPoke** and confirm **Pair**. The board is a peripheral that solicits the
Current Time Service (0x1805) and ANCS, so iOS lists it. After bonding it reads
Current Time (0x2A2B) and Local Time Information (0x2A0F) from the phone and
converts them to UTC using the phone's own offset. If the phone doesn't offer
CTS, an app such as nRF Connect or LightBlue can write the Unix UTC epoch
(ASCII or 4-byte little-endian) to:

- service `6b1d1e90-4c7a-4f6f-9c3a-2e8b7a11a001`
- characteristic `6b1d1e90-4c7a-4f6f-9c3a-2e8b7a11a002`

The radio runs on its own task, so the game keeps running, and BLE is fully
shut down after success or timeout. Bonds stay in NVS. **Boot only advertises
(45 s) if a phone has been paired before** (a bond is found in NVS) and Wi-Fi
wasn't used or failed. A board that was never paired never turns BLE on. While
a sync is running the CPU doesn't light-sleep.

The same compile-date floor applies, and a phone time without a valid timezone
is rejected rather than guessed.

## Timezone

Network time is UTC. To show local time, set a POSIX TZ string:

```
TZ JST-9                         # Japan (POSIX signs are reversed: JST-9 = UTC+9)
TZ CET-1CEST,M3.5.0,M10.5.0/3    # Central Europe with summer time
TZ?                              # show it
TZCLR                            # remove it (back to no zone)
```

It is saved in NVS (namespace `clock`, key `tz`) and applied first thing at
boot. It can also be baked in at build time, e.g.
`--build-property 'compiler.cpp.extra_flags=-DTAMAPOKE_TZ="JST-9"'`; a value
saved with `TZ` wins over that.

**No zone set** means `TZ=UTC0`, which is what the ESP32's libc already does
when TZ is unset, so the hour on screen, the clock editor and the day boundary
are calculated exactly as before.

With a zone set:

- The scene's day/night, the clock screen and the pet's daily care/streak day
  use `localtime_r()`. The clock editor turns the hour you pick back into UTC
  with `mktime()`, keeps the local date, and uses `pet.setClock()` (no ageing).
- The PCF85063 holds **UTC**. It is read with a timezone-independent
  conversion (the old `mktime()` read only worked because TZ was always UTC)
  and written with `gmtime_r()`.
- Phone CTS is local time plus an explicit offset; that offset is subtracted
  once, without `mktime()`, so the board's own zone can't be applied twice.
- `TIME <epoch>` and `RTCSET <epoch>` take UTC Unix seconds.

A board whose clock was set by hand (local time stored as if it were UTC) will
show that time shifted by the zone's offset once a zone is set. Set the clock
again, or sync it, once after choosing a zone.

## No double-counted ageing

`Pet::syncClock()` now keeps a UTC + `millis()` anchor, so a re-sync only
applies time that really passed while the game wasn't running. Minutes already
counted live aren't counted again, and changing the timezone never changes the
epoch or the pet's age. At boot this gives the same result as before.

## Diagnostics

`TIME` prints `rtc=<utc> local=YYYY-MM-DD HH:MM:SS tz=<zone|none>
scene_hour=.. age_min=.. clock_selftest=PASS fw=..`. Boot prints the zone and a
libc self-test of it (local time round trip, local midnight, clock editor).

## Files

| File | What |
|---|---|
| `nettime.cpp` / `.h` | Wi-Fi NTP and the `WIFI*` commands |
| `bttime.cpp` / `.h` | BLE time (CTS client + writable epoch) and the `BT*` commands |
| `clockzone.cpp` / `.h` | Timezone setting, civil-time helpers, self-test, `TZ*` commands |
| `rtcbat.cpp` / `.h` | TZ-independent PCF read, compile-date floor |
| `pet.cpp` / `.h` | Local day boundary, re-sync anchor |
| `TamaPoke.ino` | Boot sync, `WIFISYNC` flow, local scene hour and clock editor, `TIME` |
| `test/test_clockzone.cpp` | Host tests for all of the above that don't need a radio |
