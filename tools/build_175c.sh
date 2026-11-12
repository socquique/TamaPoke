#!/usr/bin/env bash
# Build TamaPoke for the Waveshare ESP32-S3-Touch-AMOLED-1.75C (experimental).
#
# The 1.75C needs two things the default build does not have:
#   - the compile flag -DBOARD_AMOLED_175C=1 (board.h defaults to the original 1.75)
#   - the 32 MB partition table in boards/175c/partitions.csv (3 MB app + LittleFS
#     for the sprites). It is passed explicitly here; it is NOT in the sketch root,
#     because a root partitions.csv would override the scheme of every build.
#
# Usage:
#   tools/build_175c.sh                      # -> ../out (or $TAMAPOKE_OUT)
#   tools/build_175c.sh --out DIR
#   tools/build_175c.sh --flags '-DSOME_FLAG=1'   # extra compiler flags
# CI runs this same script, so a green CI means this command works.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TAMAPOKE_OUT:-${ROOT}/../out}"
EXTRA=""
FQBN="${FQBN_175C:-esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=32M,PSRAM=opi,PartitionScheme=custom}"
PARTS="${ROOT}/boards/175c/partitions.csv"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out)   OUT="$2"; shift 2 ;;
    --flags) EXTRA="$2"; shift 2 ;;
    -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
    *) echo "Unknown arg: $1" >&2; exit 1 ;;
  esac
done

[[ -f "$PARTS" ]] || { echo "Missing $PARTS" >&2; exit 1; }
if [[ -f "${ROOT}/partitions.csv" ]]; then
  echo "A partitions.csv in the sketch root would override every build; remove it." >&2
  exit 1
fi
mkdir -p "$OUT"
BUILD="$(mktemp -d "${TMPDIR:-/tmp}/tamapoke-175c.XXXXXX")"
trap 'rm -rf "$BUILD"' EXIT

# The ESP32 core's first prebuild hook copies a sketch-root partitions.csv into
# the build folder. Point that hook at the 1.75C table instead, so the table is
# used on every build (even after arduino-cli cleans the build folder).
arduino-cli compile --warnings=all \
  --fqbn "$FQBN" \
  --build-path "$BUILD" \
  --build-property "compiler.cpp.extra_flags=-DBOARD_AMOLED_175C=1 ${EXTRA}" \
  --build-property "recipe.hooks.prebuild.1.pattern=cp -f \"${PARTS}\" \"{build.path}/partitions.csv\"" \
  --output-dir "$OUT" \
  "$ROOT"

# Check the table that went into the image really is the 1.75C one.
if ! diff -q <(grep -v '^#' "$PARTS" | tr -d ' \r' | grep -v '^$') \
             <(grep -v '^#' "$BUILD/partitions.csv" | tr -d ' \r' | grep -v '^$') >/dev/null; then
  echo "Build used the wrong partition table ($BUILD/partitions.csv)" >&2
  exit 1
fi
cp -f "${ROOT}/boards/175c/partitions.csv" "$OUT/partitions.csv"
echo "1.75C build OK -> $OUT (partitions: boards/175c/partitions.csv)"
