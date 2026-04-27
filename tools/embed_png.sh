#!/bin/bash
# Generate a C++ header that embeds a PNG file as a base64 string.
# Usage: embed_png.sh <input.png> <output.h> <namespace> <symbol>

set -euo pipefail

if [ "$#" -ne 4 ]; then
    echo "usage: $0 <input.png> <output.h> <namespace> <symbol>" >&2
    exit 1
fi

input="$1"
output="$2"
ns="$3"
sym="$4"

guard="$(echo "BESTIARY_${ns}_${sym}_H" | tr '[:lower:]' '[:upper:]')"

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

{
    printf '#ifndef %s\n' "$guard"
    printf '#define %s\n\n' "$guard"
    printf 'namespace %s {\n' "$ns"
    printf 'static const char %s[] =\n' "$sym"
    base64 -w 120 "$input" | sed -e 's/.*/"&"/' -e '$!s/$//' -e '$s/$/;/'
    printf '}\n\n'
    printf '#endif\n'
} > "$tmp"

mv "$tmp" "$output"
trap - EXIT
