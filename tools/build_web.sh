#!/bin/bash
# Regenera el firmware del instalador web: las cuatro partes que flashea
# manifest.json (bootloader, particiones, boot_app0, app) y tamapoke.bin, la
# imagen combinada para flashear a mano en 0x0.
# El instalador NO usa la imagen combinada: merge-bin rellena con 0xFF el hueco
# 0x9000-0xE000, que es la NVS, y cada actualizacion borraba la partida (#30).
# Uso: bash tools/build_web.sh
set -e
cd "$(dirname "$0")/.."
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"

echo "Compilando..."
arduino-cli compile --fqbn "$FQBN" --export-binaries .

B=build/esp32.esp32.esp32s3
echo "Fusionando binarios..."
esptool.py --chip esp32s3 merge-bin -o web/firmware/tamapoke.bin \
  0x0     "$B/TamaPoke.ino.bootloader.bin" \
  0x8000  "$B/TamaPoke.ino.partitions.bin" \
  0xe000  "$B/boot_app0.bin" \
  0x10000 "$B/TamaPoke.ino.bin"

cp "$B/TamaPoke.ino.bootloader.bin" web/firmware/bootloader.bin
cp "$B/TamaPoke.ino.partitions.bin" web/firmware/partitions.bin
cp "$B/boot_app0.bin"               web/firmware/boot_app0.bin
cp "$B/TamaPoke.ino.bin"            web/firmware/app.bin

echo "OK -> web/firmware/tamapoke.bin ($(du -h web/firmware/tamapoke.bin | cut -f1))"

echo "Empaquetando sprites..."
python3 tools/pack_bundle.py
