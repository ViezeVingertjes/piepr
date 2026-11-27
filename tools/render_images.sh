#!/usr/bin/env bash
# Turns the editable images (assets/src, hardware/src) into the published ones
# next to them, with all text converted to outlines, so they look the same on a
# machine without the Ubuntu font.
#   tools/render_images.sh
# Needs Linux with Chromium (or set $CHROME), pdftocairo (poppler-utils), perl and
# the Ubuntu and Ubuntu Mono fonts.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
chrome="${CHROME:-chromium}"

for tool in "$chrome" pdftocairo perl; do
    command -v "$tool" > /dev/null || { echo "'$tool' is not installed" >&2; exit 1; }
done

fonts="$(fc-list : family)"
for font in "Ubuntu" "Ubuntu Mono"; do
    grep -qx "$font" <<< "$fonts" || { echo "font '$font' is not installed" >&2; exit 1; }
done

# A snap-packaged Chromium can only read files under the home directory.
tmp="$(mktemp -d -p "${RENDER_TMP:-$HOME}" piepr-render.XXXXXX)"
trap 'rm -rf "$tmp"' EXIT

# The value of attribute $1 on the root element of $2.
attr() { sed -n -E "0,/<svg /s|.*<svg [^>]* $1=\"([^\"]*)\".*|\1|p" "$2"; }

for src in "$root"/assets/src/*.svg "$root"/hardware/src/*.svg; do
    out="$(dirname "$(dirname "$src")")/$(basename "$src")"
    width="$(attr width "$src")"
    height="$(attr height "$src")"
    label="$(attr aria-label "$src")"
    if [ -z "$width" ] || [ -z "$height" ] || [ -z "$label" ]; then
        echo "$src: the <svg> tag needs width, height and aria-label, on one line" >&2
        exit 1
    fi
    label="$(sed -e 's/[\\&|]/\\&/g' <<< "$label")" # safe inside the sed below
    rm -f "$tmp"/page.*

    {
        echo "<!doctype html><meta charset=\"utf-8\"><style>"
        echo "@page { size: ${width}px ${height}px; margin: 0 } html, body { margin: 0 } svg { display: block }"
        echo "</style>"
        cat "$src"
    } > "$tmp/page.html"
    "$chrome" --headless --disable-gpu --no-pdf-header-footer \
        --print-to-pdf="$tmp/page.pdf" "file://$tmp/page.html" > /dev/null 2>&1
    [ -s "$tmp/page.pdf" ] || { echo "$src: Chromium wrote no PDF" >&2; exit 1; }
    pdftocairo -svg -f 1 -l 1 "$tmp/page.pdf" "$tmp/page.svg"

    # Back to pixel size (the PDF is in points, 3/4 of a pixel, and its page is
    # rounded), and the name a screen reader gets.
    box="0 0 $(awk "BEGIN { print $width * 0.75, $height * 0.75 }")"
    sed -E -e "0,/<svg /s|<svg ([^>]*)width=\"[^\"]*\" height=\"[^\"]*\"|<svg \1width=\"$width\" height=\"$height\" role=\"img\" aria-label=\"$label\"|" \
        -e "0,/viewBox=/s|viewBox=\"[^\"]*\"|viewBox=\"$box\"|" \
        -e "0,/<svg [^>]*>/s|(<svg [^>]*>)|\1<title>$label</title>|" \
        "$tmp/page.svg" |
        # Two decimals are plenty for outlines and halve the file size.
        perl -pe 's{( d="[^"]*")}{ (my $d = $1) =~ s/(\d+\.\d\d)\d+/$1/g; $d }ge' > "$out"

    if grep -q -E "<text|font-family" "$out"; then
        echo "$out still depends on a font" >&2
        exit 1
    fi
    echo "$out ($(stat -c%s "$out") bytes)"
done
