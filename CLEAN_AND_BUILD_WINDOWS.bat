@echo off
setlocal
cd /d "%~dp0"

where pio >nul 2>nul
if errorlevel 1 (
  echo PlatformIO CLI not found. Use VS Code PlatformIO instead.
  pause
  exit /b 1
)

pio run -e m5stack-cardputer-adv -t clean
if errorlevel 1 exit /b 1
pio run -e m5stack-cardputer-adv
if errorlevel 1 (
  echo BUILD FAILED.
  pause
  exit /b 1
)
echo BUILD SUCCESS: .pio\build\m5stack-cardputer-adv\firmware.bin
pause
