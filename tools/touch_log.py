#!/usr/bin/env python3
"""Monitor de toques: python3 tools/touch_log.py (Ctrl-C para salir)."""
import glob, serial
port = glob.glob('/dev/cu.usbmodem*')[0]
ser = serial.Serial(port, 115200, timeout=1)
print(f"escuchando {port}... toca la pantalla (Ctrl-C para salir)")
try:
    while True:
        line = ser.readline().decode(errors='replace').strip()
        if line:
            print(line)
except KeyboardInterrupt:
    pass
