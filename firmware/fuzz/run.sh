#!/usr/bin/env bash
# Builds and runs one libFuzzer target against lib/p1core + lib/portalcore.
#   fuzz/run.sh <p1_framer|wifi_form|captive_dns|json_escape> [seconds=30]
# Needs clang. Findings (crash-*/leak-*) are written to fuzz/build/.
set -euo pipefail

target="${1:?usage: fuzz/run.sh <p1_framer|wifi_form|captive_dns|json_escape> [seconds]}"
seconds="${2:-30}"
here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
build="$here/build"
corpus="$build/corpus/$target"

mkdir -p "$build" "$corpus"
cp -n "$here/seeds/$target/"* "$corpus/" 2>/dev/null || true

clang -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all \
    -I"$root/lib/p1core/include" -I"$root/lib/portalcore/include" \
    "$root"/lib/p1core/src/*.c "$root"/lib/portalcore/src/*.c \
    "$here/fuzz_$target.c" -o "$build/fuzz_$target"

cd "$build"
"./fuzz_$target" -max_total_time="$seconds" -print_final_stats=1 "$corpus"
