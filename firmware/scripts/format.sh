#!/usr/bin/env bash
# Formats the C code with .clang-format, or with --check only reports files that
# are not formatted. Needs clang-format 22.1.5 (pip install clang-format==22.1.5):
# other versions lay the code out differently.
set -euo pipefail

WANTED="22.1.5"
cd "$(dirname "$0")/.."

have="$(clang-format --version | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' | head -1)"
if [ "$have" != "$WANTED" ]; then
    echo "clang-format $WANTED is needed, this is $have" >&2
    exit 1
fi

files=(src/*.[ch] lib/*/*/*.[ch] test/*/*.c fuzz/*.c)
if [ "${1:-}" = "--check" ]; then
    clang-format --dry-run -Werror "${files[@]}"
else
    clang-format -i "${files[@]}"
fi
