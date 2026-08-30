@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title TamaPoke Cardputer ADV - One Click Builder
color 0A

echo ============================================================
echo       TamaPoke Cardputer ADV v0.7 - ONE CLICK BUILD
echo ============================================================
echo.
echo This compiles locally on THIS Windows PC using PlatformIO.
echo No Replit or paid service is used.
echo.

set "PIO_EXE="

for /f "delims=" %%I in ('where platformio.exe 2^>nul') do if not defined PIO_EXE set "PIO_EXE=%%I"
for /f "delims=" %%I in ('where pio.exe 2^>nul') do if not defined PIO_EXE set "PIO_EXE=%%I"
for /f "delims=" %%I in ('where pio 2^>nul') do if not defined PIO_EXE set "PIO_EXE=%%I"

if not defined PIO_EXE if exist "%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" set "PIO_EXE=%USERPROFILE%\.platformio\penv\Scripts\platformio.exe"
if not defined PIO_EXE if exist "%USERPROFILE%\.platformio\penv\Scripts\pio.exe" set "PIO_EXE=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"

if not defined PIO_EXE (
    color 0C
    echo ERROR: PlatformIO could not be found.
    echo.
    echo Open VS Code once and make sure the PlatformIO extension is installed,
    echo then double-click this file again.
    echo.
    pause
    exit /b 1
)

echo PlatformIO found:
echo %PIO_EXE%
echo.

echo [1/2] Cleaning old build files...
"%PIO_EXE%" run -e m5stack-cardputer-adv -t clean
if errorlevel 1 (
    color 0C
    echo.
    echo CLEAN FAILED.
    echo Send a screenshot of this window to ChatGPT.
    pause
    exit /b 1
)

echo.
echo [2/2] Building firmware...
echo The first build can take a few minutes if PlatformIO downloads libraries.
echo.
"%PIO_EXE%" run -e m5stack-cardputer-adv
if errorlevel 1 (
    color 0C
    echo.
    echo ============================================================
    echo                     BUILD FAILED
    echo ============================================================
    echo.
    echo Do not change anything. Send a screenshot of the error to ChatGPT.
    echo.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo                    BUILD SUCCESSFUL
echo ============================================================
echo.

if exist "%CD%\TamaPoke-CardputerADV-v0.7-MERGED.bin" (
    echo SINGLE FLASH BIN:
    echo   TamaPoke-CardputerADV-v0.7-MERGED.bin
    echo.
    start "" explorer.exe /select,"%CD%\TamaPoke-CardputerADV-v0.7-MERGED.bin"
) else if exist "%CD%\TamaPoke-CardputerADV-v0.7-firmware.bin" (
    echo APP FIRMWARE BIN:
    echo   TamaPoke-CardputerADV-v0.7-firmware.bin
    echo.
    start "" explorer.exe /select,"%CD%\TamaPoke-CardputerADV-v0.7-firmware.bin"
) else (
    echo PlatformIO finished, but the copied BIN was not found.
    echo Check:
    echo   .pio\build\m5stack-cardputer-adv\firmware.bin
)

echo.
pause
