#!/bin/bash
# Makes the Vita LiveArea art (icon, background, gate) from the game itself: frames of the
# intro, captured with a headless Linux build. Writes livearea/*.png, which git ignores
# (they are the game's own art); the VPK picks them up when present.
# usage: tools/vita/make_livearea.sh [data-dir]   (needs build/, ImageMagick, curl)
set -eu
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
DATA="${1:-$HERE/../gamedata-fx}"
OUT="$HERE/livearea"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$OUT"

# The intro's timing varies from run to run, so take a frame a second and pick: the
# brightest street (after its fade-in, 30-42 s), the Quill's office (57 s) and the frame
# with most of Ron's hair in the icon crop (his close-up, 78-104 s).
{
  for t in $(seq 30000 1000 42000) $(seq 78000 1000 104000); do echo "$t shot t$t"; done
  echo "57000 shot office"
  echo "105000 quit"
} | sort -n > "$TMP/script.txt"
HM_LANG=es "$HERE/tools/test/run_script.sh" "$TMP/script.txt" "$TMP/shots" "$DATA" > "$TMP/run.log" 2>&1
best() {   # best <metric> <first ms> <last ms>: the frame with the highest metric
  for t in $(seq "$2" 1000 "$3"); do
    f="$TMP/shots/t$t.png"
    [ -f "$f" ] && echo "$($1 "$f") $f"
  done | sort -rn | head -1 | cut -d' ' -f2
}
brightness() { convert "$1" -colorspace Gray -format "%[fx:mean]" info:; }
ron_hair() { convert "$1" -crop 300x300+170+80 +repage -fuzz 12% -fill white -opaque "srgb(141,72,20)" -fill black +opaque white -format "%[fx:mean]" info:; }
STREET="$(best brightness 30000 42000)"
RON="$(best ron_hair 78000 104000)"
[ -n "$STREET" ] && [ -n "$RON" ] && [ -f "$TMP/shots/office.png" ] || { echo "frames missing, see $TMP/run.log"; exit 1; }

FONT="$TMP/Bungee.ttf"
curl -sfL -o "$FONT" https://github.com/google/fonts/raw/main/ofl/bungee/Bungee-Regular.ttf
P8=(-dither FloydSteinberg -colors 256)

convert "$RON" -crop 300x300+170+80 +repage -resize 128x128 "${P8[@]}" "PNG8:$OUT/icon0.png"
convert "$STREET" -crop 640x381+0+60 +repage -resize 840x500! \
  \( -size 840x210 gradient:"rgba(0,0,0,0.75)-rgba(0,0,0,0)" \) -gravity north -compose over -composite \
  -font "$FONT" -fill "#ffd23f" -stroke "#3a1600" -strokewidth 3 -pointsize 52 -gravity north \
  -annotate +0+26 "HOLLYWOOD" -annotate +0+86 "MONSTERS" "${P8[@]}" "PNG8:$OUT/bg0.png"
convert "$TMP/shots/office.png" -crop 640x361+0+70 +repage -resize 280x158! -fill "#00000070" -draw "rectangle 0,0 280,158" \
  -font "$FONT" -fill "#ffd23f" -stroke "#3a1600" -strokewidth 2 -pointsize 30 -gravity center \
  -annotate +0-18 "HOLLYWOOD" -annotate +0+18 "MONSTERS" "${P8[@]}" "PNG8:$OUT/startup.png"
echo "LiveArea art in $OUT"
