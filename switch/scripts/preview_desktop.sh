#!/usr/bin/env bash
# Builds the desktop (Linux) version and screenshots the UI on a virtual display.
# Used by CI to preview the interface without a console. Output: preview/*.png
set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)/.."
OUT="$ROOT/preview"
mkdir -p "$OUT"

bash scripts/setup.sh
cmake -B build-desktop -DPLATFORM_DESKTOP=ON -DGLFW_BUILD_WAYLAND=OFF -DCMAKE_BUILD_TYPE=Release
make -C build-desktop -j"$(nproc)" AnikkuNX

mkdir -p ~/.config/AnikkuNX
echo '{"sourcesChosen": true, "checkUpdates": false, "checkNewEpisodes": false, "enabledSources": ["en.animepahe", "en.kickassanime", "en.anikoto", "en.anichi", "en.wcofun", "en.animenosub", "en.aniwave", "en.animesogo"]}' \
  > ~/.config/AnikkuNX/config.json

export LIBGL_ALWAYS_SOFTWARE=1
Xvfb :99 -screen 0 1280x720x24 &
export DISPLAY=:99
sleep 2

cd build-desktop
./AnikkuNX > "$OUT/app.log" 2>&1 &
APP=$!

press() {  # hold each key briefly: borealis polls the keyboard once per frame
  for _ in $(seq "${2:-1}"); do
    xdotool keydown "$1"; sleep 0.12; xdotool keyup "$1"; sleep 0.5
  done
}
shot() { sleep "${2:-3}"; import -window root "$OUT/$1.png"; }

sleep 50
WIN=$(xdotool search --name AnikkuNX | head -1 || true)
[ -n "$WIN" ] && xdotool windowfocus "$WIN" || true

shot 01-home 1
for i in 1 2 3 4 5 6; do
  press Down
  shot "0$((i + 1))-down-$i" 2
done
shot 08-after-wait 12
press Right 3
shot 09-right 2
press Up 12
press Right 5
shot 10-gear-focused 1
press Return
shot 11-settings 4
press Return
shot 12-source-picker 4

kill "$APP" || true
echo "screenshots in $OUT"
