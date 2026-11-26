#!/bin/bash
# Installe Swell Tape dans ~/Library/Audio/Plug-Ins (après build_mac.sh).
set -e
cd "$(dirname "$0")/.."
A=build-release/SwellTape_artefacts/Release
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"; AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"
rm -rf "$VST3_DIR/Swell Tape.vst3" "$AU_DIR/Swell Tape.component"
# ditto --norsrc : pas d'AppleDouble (._*) venus du disque source
ditto --norsrc --noextattr "$A/VST3/Swell Tape.vst3" "$VST3_DIR/Swell Tape.vst3"
ditto --norsrc --noextattr "$A/AU/Swell Tape.component" "$AU_DIR/Swell Tape.component"
for b in "$VST3_DIR/Swell Tape.vst3" "$AU_DIR/Swell Tape.component"; do
    find "$b" -name '._*' -delete
    codesign --force --deep -s - "$b"
    codesign --verify --deep "$b"
done
killall -9 AudioComponentRegistrar 2>/dev/null || true   # rafraîchit la liste des AU
echo "Installé : $VST3_DIR et $AU_DIR"
