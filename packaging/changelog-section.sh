#!/usr/bin/env bash
# Prints the CHANGELOG.md section of one version (without its heading).
# Exits with an error if the version has no entry or the entry is empty.
#
# Usage: packaging/changelog-section.sh <version>   e.g. 1.2.0
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 <version>" >&2
    exit 2
fi

root="$(cd "$(dirname "$0")/.." && pwd)"
version="$1"

section="$(awk -v head="## [$version]" '
    index($0, head) == 1        { found = 1; next }   # Überschrift der Version
    found && /^## \[/           { exit }              # nächste Version beginnt
    found && /^\[[^]]+\]: http/ { next }              # Link-Referenzen am Dateiende
    found                       { print }
' "$root/CHANGELOG.md" | sed -e '/./,$!d' | sed -e ':a' -e '/^\n*$/{$d;N;ba' -e '}')"

if [ -z "${section//[[:space:]]/}" ]; then
    echo "CHANGELOG.md has no entry for version $version." >&2
    echo "Add a section '## [$version] - YYYY-MM-DD' (Added / Changed / Fixed) before building a release." >&2
    exit 1
fi

printf '%s\n' "$section"
