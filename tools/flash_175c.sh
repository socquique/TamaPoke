#!/usr/bin/env bash
# Flash TamaPoke 1.75C firmware and/or the LittleFS sprite image.
#
# Build first with tools/build_175c.sh. The partition table is generated here
# from boards/175c/partitions.csv (never from whatever the last build left), and
# the app/LittleFS offsets are read from that same file.
#
# Usage:
#   tools/flash_175c.sh --port /dev/ttyACM0           # app + bootloader + partitions + FS
#   tools/flash_175c.sh --port /dev/ttyACM0 --fs-only
#   tools/flash_175c.sh --port /dev/ttyACM0 --fw-only
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TAMAPOKE_OUT:-${ROOT}/../out}"
PORT=""
DO_FW=1
DO_FS=1
BAUD="${BAUD:-921600}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port) PORT="$2"; shift 2 ;;
    --out)  OUT="$2"; shift 2 ;;
    --fs-only) DO_FW=0; DO_FS=1; shift ;;
    --fw-only) DO_FW=1; DO_FS=0; shift ;;
    --baud) BAUD="$2"; shift 2 ;;
    -h|--help) sed -n '2,14p' "$0"; exit 0 ;;
    *) echo "Unknown arg: $1" >&2; exit 1 ;;
  esac
done

if [[ -z "$PORT" ]]; then
  echo "Pass --port /dev/ttyACM0 (or COM3 on Windows)" >&2
  exit 1
fi

# Prefer the esptool that ships with the Arduino ESP32 core, then a venv.
ESPTOOL="${ESPTOOL:-}"
if [[ -z "$ESPTOOL" ]]; then
  for c in \
    "${HOME}/.arduino15/packages/esp32/tools/esptool_py/"*/esptool \
    "${HOME}/.arduino15/packages/esp32/tools/esptool_py/"*/esptool.py \
    "${ROOT}/.venv/bin/esptool.py" \
    "$(command -v esptool.py || true)" \
    "$(command -v esptool || true)"; do
    if [[ -n "$c" && -e "$c" ]]; then ESPTOOL="$c"; break; fi
  done
fi
if [[ -z "${ESPTOOL}" ]]; then
  echo "esptool not found" >&2
  exit 1
fi

PARTS_CSV="${ROOT}/boards/175c/partitions.csv"
[[ -f "$PARTS_CSV" ]] || { echo "Missing $PARTS_CSV" >&2; exit 1; }
# Offset of a partition by name, straight from the CSV.
part_off() { awk -F, -v n="$1" '!/^#/ { gsub(/[ \t]/, ""); if ($1 == n) print $4 }' "$PARTS_CSV"; }

BOOTLOADER_OFF=0x0
PARTITIONS_OFF=0x8000
BOOT_APP0_OFF="$(part_off otadata)"
APP_OFF="$(part_off app0)"
FS_OFF="$(part_off storage)"
[[ -n "$APP_OFF" && -n "$FS_OFF" && -n "$BOOT_APP0_OFF" ]] || { echo "Could not read offsets from $PARTS_CSV" >&2; exit 1; }

args=( --chip esp32s3 --port "$PORT" --baud "$BAUD" write_flash -z --flash_mode dio --flash_freq 80m --flash_size 32MB )

if [[ "$DO_FW" -eq 1 ]]; then
  boot="${OUT}/TamaPoke.ino.bootloader.bin"
  bootapp="${OUT}/TamaPoke.ino.boot_app0.bin"
  app="${OUT}/TamaPoke.ino.bin"
  for f in "$boot" "$app"; do
    [[ -f "$f" ]] || { echo "Missing $f — run tools/build_175c.sh first (see PORTING.md)" >&2; exit 1; }
  done
  # Partition table from boards/175c/partitions.csv, using the core's own tool.
  GEN="$(ls "${HOME}"/.arduino15/packages/esp32/hardware/esp32/*/tools/gen_esp32part.py 2>/dev/null | tail -n1 || true)"
  [[ -n "$GEN" ]] || { echo "gen_esp32part.py not found (install the esp32 core)" >&2; exit 1; }
  parts="$(mktemp "${TMPDIR:-/tmp}/tamapoke-175c-parts.XXXXXX.bin")"
  trap 'rm -f "$parts"' EXIT
  python3 "$GEN" -q --flash-size 32MB "$PARTS_CSV" "$parts"
  args+=( "$BOOTLOADER_OFF" "$boot" "$PARTITIONS_OFF" "$parts" )
  if [[ -f "$bootapp" ]]; then
    args+=( "$BOOT_APP0_OFF" "$bootapp" )
  fi
  args+=( "$APP_OFF" "$app" )
fi

if [[ "$DO_FS" -eq 1 ]]; then
  fs="${OUT}/storage.littlefs.bin"
  if [[ ! -f "$fs" ]]; then
    fs="${ROOT}/tools/build/storage.littlefs.bin"
  fi
  if [[ ! -f "$fs" ]]; then
    echo "LittleFS image not found. Run tools/build_spritefs.sh first." >&2
    exit 1
  fi
  args+=( "$FS_OFF" "$fs" )
fi

echo "+ $ESPTOOL ${args[*]}"
"$ESPTOOL" "${args[@]}"
echo "Done. Reset the board if it does not restart by itself."
