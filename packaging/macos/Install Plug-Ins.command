#!/bin/bash
# Installs the ZYNTH VST3 and Audio Unit for the current user and clears the
# download quarantine so hosts can load them. Double-click (or right-click >
# Open the first time).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/ZYNTH.vst3" "$AU/ZYNTH.component"
cp -R "$HERE/Plug-Ins/ZYNTH.vst3" "$VST3/"
cp -R "$HERE/Plug-Ins/ZYNTH.component" "$AU/"
xattr -dr com.apple.quarantine "$VST3/ZYNTH.vst3" "$AU/ZYNTH.component" 2>/dev/null || true
if [ -d "$HERE/ZYNTH.app" ]; then
    xattr -dr com.apple.quarantine "$HERE/ZYNTH.app" 2>/dev/null || true
fi
# Make the Audio Unit visible to Logic / GarageBand without logging out.
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo
echo "Installed:"
echo "  $VST3/ZYNTH.vst3"
echo "  $AU/ZYNTH.component"
echo
echo "Restart your DAW and rescan plug-ins. In Logic, ZYNTH appears under"
echo "Instrument > AU Instruments > ZYNTH."
