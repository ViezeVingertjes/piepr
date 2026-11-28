#!/usr/bin/env bash
# Builds the release files from a finished build (pio run). For each board in
# platformio.ini's default_envs: one image with bootloader, partition table and
# firmware, to be flashed at address 0x0, and its .sha256. Then manifest.json,
# which lists every board's image; the installer at piepr.nl reads it.
#   scripts/release_image.sh <version> [outdir=.]     e.g. v1.0.1
# Needs esptool 5 (pip install esptool); another binary can be set in $ESPTOOL.
set -euo pipefail

version="${1:?usage: scripts/release_image.sh <version> [outdir]}"
outdir="${2:-.}"
here="$(cd "$(dirname "$0")" && pwd)"
project="$(dirname "$here")"

die() {
    echo "release_image: $*" >&2
    exit 1
}

# The boards: the envs `pio run` builds.
boards="$(awk -F= '$1 ~ /^ *default_envs *$/ { gsub(/[ ,]+/, " ", $2); print $2 }' "$project/platformio.ini")"
[ -n "${boards// /}" ] || die "no default_envs in platformio.ini"

BOOTLOADER_AT=0x0 # on every chip Piepr supports (ESP32-C3 and -S3)
PARTITIONS_AT=0x8000
# The firmware goes where partitions.csv puts the first firmware slot.
FIRMWARE_AT="$(awk -F, '$1 ~ /^ *ota_0 *$/ { gsub(/ /, "", $4); print $4 }' "$project/partitions.csv")"
[ -n "$FIRMWARE_AT" ] || die "no ota_0 in partitions.csv"

mkdir -p "$outdir"
entries=()
for board in $boards; do
    build="$project/.pio/build/$board"
    for part in bootloader partitions firmware; do
        [ -f "$build/$part.bin" ] || die "missing $build/$part.bin: run 'pio run' first"
    done
    # The chip, as esptool names it (esp32s3) and as the chip reports itself (ESP32-S3).
    target="$(sed -n 's/^CONFIG_IDF_TARGET="\(.*\)"$/\1/p' "$project/sdkconfig.$board")"
    [ -n "$target" ] || die "no CONFIG_IDF_TARGET in sdkconfig.$board"
    chip="$(echo "${target#esp32}" | tr a-z A-Z)"
    chip="ESP32${chip:+-$chip}"
    # The board's name, from its header: env xiao-s3 is src/boards/xiao_s3.h.
    header="$project/src/boards/${board//-/_}.h"
    name="$(sed -n 's/^#define BOARD_NAME *"\(.*\)"$/\1/p' "$header" 2>/dev/null)"
    [ -n "$name" ] || die "no BOARD_NAME in $header"

    image="piepr-$board-$version.bin"
    out="$outdir/$image"
    "${ESPTOOL:-esptool}" --chip "$target" merge-bin -o "$out" \
        "$BOOTLOADER_AT" "$build/bootloader.bin" \
        "$PARTITIONS_AT" "$build/partitions.bin" \
        "$FIRMWARE_AT" "$build/firmware.bin"

    # Each part must sit unchanged at its address.
    cmp -n "$(stat -c%s "$build/bootloader.bin")" "$build/bootloader.bin" "$out" 0 "$((BOOTLOADER_AT))"
    cmp -n "$(stat -c%s "$build/partitions.bin")" "$build/partitions.bin" "$out" 0 "$((PARTITIONS_AT))"
    cmp "$build/firmware.bin" "$out" 0 "$((FIRMWARE_AT))"

    (cd "$outdir" && sha256sum "$image" > "$image.sha256")
    sha256="$(cut -d' ' -f1 "$out.sha256")"
    size="$(stat -c%s "$out")"
    entries+=("$(printf '    { "board": "%s", "name": "%s", "chip": "%s", "image": "%s", "size": %s, "sha256": "%s" }' \
        "$board" "$name" "$chip" "$image" "$size" "$sha256")")
    echo "$out ($name, $chip, $size bytes)"
done

{
    printf '{\n  "version": "%s",\n  "boards": [\n' "${version#v}"
    for i in "${!entries[@]}"; do
        printf '%s%s\n' "${entries[$i]}" "$([ "$i" -lt $((${#entries[@]} - 1)) ] && echo ,)"
    done
    printf '  ]\n}\n'
} > "$outdir/manifest.json"
echo "$outdir/manifest.json"
