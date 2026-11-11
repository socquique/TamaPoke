#pragma once
#include <Arduino.h>

// BLE time-sync fallback when Wi-Fi/NTP is unavailable. Opt-in: nothing
// starts at boot unless a phone has been paired with BTPAIR.
// The board is a peripheral named "TamaPoke". After a Just Works bond it
// acts as a GATT client on the phone's Current Time Service (0x1805).
// A custom writable characteristic is also offered so a generic app
// (nRF Connect / LightBlue) can push a Unix UTC epoch if iOS CTS is
// not exposed. BLE starts only after Wi-Fi is off, never blocks the UI,
// and is fully deinitialised after success or timeout.

void btTimeStart(uint32_t advertiseMs, bool pairMode);
void btTimeStop();
void btTimePoll();  // apply a pending epoch on the Arduino loop task
bool btTimeBusy();
uint32_t btTimeAppliedCount();
bool btTimeHasBond();  // a phone was paired before (BTPAIR); boot only advertises then  // increments only after an epoch is applied on the loop task

// BTPAIR, BTSYNC, BTCLR, BT?
// Returns true if the line was a Bluetooth command (whether OK or ERR).
bool btSerialCommand(const String &line);
