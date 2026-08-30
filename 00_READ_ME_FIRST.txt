TAMAPOKE CARDPUTER ADV v0.7 - AUTOMATIC SAVE / ONE CLICK BUILD

1. Extract this ZIP.
2. Double-click: 00_BUILD_BIN_ONE_CLICK.bat
3. Wait for BUILD SUCCESSFUL.
4. Explorer opens and highlights the finished BIN.

Preferred output:
  TamaPoke-CardputerADV-v0.7-MERGED.bin

v0.7 SAVE BEHAVIOR
- The real upstream Pet::save() writes directly to microSD immediately.
- Pet::begin() loads the microSD save before first-run/starter logic.
- Important actions save immediately.
- Petting is explicitly saved too.
- A background autosave checks every 2 seconds for changed Pet state.
- If nothing changed, it does NOT write again, reducing SD-card wear.
- New alternating CRC-verified save files:
    /tamapoke_v7_a.bin
    /tamapoke_v7_b.bin
- At restart, the newest valid slot is loaded automatically.
- A sudden power-off should lose at most about 2 seconds of unsaved passive
  progress; actions that call save are written immediately.

The previous visual/input fixes remain unchanged.
Keep the same microSD inserted while using TamaPoke.
