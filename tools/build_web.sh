#!/bin/bash
# Regenera los binarios separados para el instalador web.
# Importante: no usar un binario combinado desde 0x0 para actualizaciones,
# porque rellenaria con 0xFF el hueco 0x9000..0xDFFF y borraria NVS (save).
# Uso: bash tools/build_web.sh
set -e
cd "$(dirname "$0")/.."
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
VERSION="1.20.1-box-v2"

echo "Compilando..."
arduino-cli compile --fqbn "$FQBN" --export-binaries .

B=build/esp32.esp32.esp32s3
echo "Copiando binarios separados..."
cp "$B/TamaPoke.ino.bootloader.bin" "web/firmware/tamapoke-$VERSION-bootloader.bin"
cp "$B/TamaPoke.ino.partitions.bin" "web/firmware/tamapoke-$VERSION-partitions.bin"
cp "$B/boot_app0.bin" "web/firmware/tamapoke-$VERSION-boot_app0.bin"
cp "$B/TamaPoke.ino.bin" "web/firmware/tamapoke-$VERSION-app.bin"

echo "OK -> web/firmware/tamapoke-$VERSION-app.bin ($(du -h "web/firmware/tamapoke-$VERSION-app.bin" | cut -f1))"

echo "Empaquetando sprites..."
python3 tools/pack_bundle.py
