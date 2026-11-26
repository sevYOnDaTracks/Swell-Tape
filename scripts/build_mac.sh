#!/bin/bash
# Compile Swell Tape (VST3 + AU + Standalone, binaire universel) et signe en ad hoc.
set -e
cd "$(dirname "$0")/.."
command -v cmake >/dev/null || { echo "CMake manquant : brew install cmake"; exit 1; }
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target SwellTape_VST3 SwellTape_AU SwellTape_Standalone -j "$(sysctl -n hw.ncpu)"
A=build-release/SwellTape_artefacts/Release
find "$A" -name '._*' -delete
for b in "$A/VST3/Swell Tape.vst3" "$A/AU/Swell Tape.component" "$A/Standalone/Swell Tape.app"; do
    xattr -cr "$b"; codesign --force --deep -s - "$b"
done
echo "OK : $A"
