#!/usr/bin/env bash
# Builds the release image from a finished build (pio run): bootloader,
# partition table and firmware in one file, to be flashed at address 0x0.
#   scripts/release_image.sh <version> [outdir=.]     e.g. v1.0.1
# Writes piepr-<version>.bin and piepr-<version>.bin.sha256.
# Needs esptool 5 (pip install esptool); another binary can be set in $ESPTOOL.
set -euo pipefail

version="${1:?usage: scripts/release_image.sh <version> [outdir]}"
outdir="${2:-.}"
here="$(cd "$(dirname "$0")" && pwd)"
build="$(dirname "$here")/.pio/build/supermini"
name="piepr-$version.bin"
out="$outdir/$name"

BOOTLOADER_AT=0x0
PARTITIONS_AT=0x8000
FIRMWARE_AT=0x10000

for part in bootloader partitions firmware; do
    [ -f "$build/$part.bin" ] || { echo "missing $build/$part.bin: run 'pio run' first" >&2; exit 1; }
done
mkdir -p "$outdir"

"${ESPTOOL:-esptool}" --chip esp32c3 merge-bin -o "$out" \
    --flash-mode dio --flash-freq 80m --flash-size 4MB \
    "$BOOTLOADER_AT" "$build/bootloader.bin" \
    "$PARTITIONS_AT" "$build/partitions.bin" \
    "$FIRMWARE_AT" "$build/firmware.bin"

# Each part must sit unchanged at its address. That includes the bootloader's
# header: the flash settings above have to be the ones the build used.
cmp -n "$(stat -c%s "$build/bootloader.bin")" "$build/bootloader.bin" "$out" 0 "$((BOOTLOADER_AT))"
cmp -n "$(stat -c%s "$build/partitions.bin")" "$build/partitions.bin" "$out" 0 "$((PARTITIONS_AT))"
cmp "$build/firmware.bin" "$out" 0 "$((FIRMWARE_AT))"

(cd "$outdir" && sha256sum "$name" > "$name.sha256")
echo "$out ($(stat -c%s "$out") bytes)"
