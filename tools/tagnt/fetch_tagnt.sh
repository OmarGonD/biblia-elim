#!/bin/sh
# Descarga el TAGNT (STEPBible, CC BY 4.0) en data/sources/tagnt/ desde un commit fijo.
set -e
C=0f60797c170f11a1f8dc75c5f7617973e2e66b0d
B="https://raw.githubusercontent.com/STEPBible/STEPBible-Data/$C/Translators%20Amalgamated%20OT%2BNT"
D="$(dirname "$0")/../../data/sources/tagnt"
mkdir -p "$D"
curl -fsSL "$B/TAGNT%20Mat-Jhn%20-%20Translators%20Amalgamated%20Greek%20NT%20-%20STEPBible.org%20CC-BY.txt" -o "$D/TAGNT_Mat-Jhn.txt"
curl -fsSL "$B/TAGNT%20Act-Rev%20-%20Translators%20Amalgamated%20Greek%20NT%20-%20STEPBible.org%20CC-BY.txt" -o "$D/TAGNT_Act-Rev.txt"
