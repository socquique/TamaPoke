@echo off
setlocal
cd /d "%~dp0"

where pio >nul 2>nul
if errorlevel 1 (
  echo.
  echo PlatformIO CLI was not found.
  echo Open this folder in VS Code with the PlatformIO extension installed,
  echo then use the PlatformIO Build button.
  echo.
  pause
  exit /b 1
)

echo Building TamaPoke for Cardputer ADV...
pio run -e m5stack-cardputer-adv
if errorlevel 1 (
  echo.
  echo BUILD FAILED. Copy the error text and send it to ChatGPT.
  pause
  exit /b 1
)

echo.
echo BUILD SUCCESS.
echo Firmware:
echo   .pio\build\m5stack-cardputer-adv\firmware.bin
echo.
pause
