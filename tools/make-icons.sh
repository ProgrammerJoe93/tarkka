#!/usr/bin/env bash
# Regenerates every raster icon from assets/*.svg.
#
# Checked-in PNGs are build inputs (Qt resources and Android mipmaps cannot
# consume SVG directly), so this script is the record of how they were made.
set -euo pipefail

cd "$(dirname "$0")/.."
command -v rsvg-convert >/dev/null || { echo "rsvg-convert is required" >&2; exit 1; }

mkdir -p assets/icons

# Desktop window icon. Qt picks the closest size from the set.
for size in 16 24 32 48 64 128 256 512; do
    rsvg-convert -w "$size" -h "$size" assets/logo.svg -o "assets/icons/tarkka-${size}.png"
done

# Android. Adaptive icons are 108dp of which only the central 72dp is
# guaranteed visible, so foreground and background are separate full-bleed
# layers and the launcher masks them; the legacy square is for pre-API-26.
declare -A adaptive=([mdpi]=108 [hdpi]=162 [xhdpi]=216 [xxhdpi]=324 [xxxhdpi]=432)
declare -A legacy=([mdpi]=48 [hdpi]=72 [xhdpi]=96 [xxhdpi]=144 [xxxhdpi]=192)

for density in "${!adaptive[@]}"; do
    dir="android/res/mipmap-${density}"
    mkdir -p "$dir"
    rsvg-convert -w "${adaptive[$density]}" -h "${adaptive[$density]}" \
        assets/mark-adaptive.svg -o "$dir/ic_launcher_foreground.png"
    rsvg-convert -w "${adaptive[$density]}" -h "${adaptive[$density]}" \
        assets/background.svg -o "$dir/ic_launcher_background.png"
    rsvg-convert -w "${legacy[$density]}" -h "${legacy[$density]}" \
        assets/logo.svg -o "$dir/ic_launcher.png"
done

mkdir -p android/res/mipmap-anydpi-v26
cat > android/res/mipmap-anydpi-v26/ic_launcher.xml <<'XML'
<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@mipmap/ic_launcher_background"/>
    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>
</adaptive-icon>
XML
cp android/res/mipmap-anydpi-v26/ic_launcher.xml \
   android/res/mipmap-anydpi-v26/ic_launcher_round.xml

echo "icons regenerated"
