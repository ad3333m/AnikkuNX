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
# one fake history entry so the Continue Watching row shows up
echo '{"k": {"sourceId": "en.anichi", "animeUrl": "preview", "animeTitle": "Black Clover", "thumbnail": "https://s4.anilist.co/file/anilistcdn/media/anime/cover/large/bx97940-fyh8o7gNbha0.png", "episodeUrl": "preview-5", "episodeName": "Episode 5", "position": 600, "duration": 1440, "updatedAt": 1}}'   > ~/.config/AnikkuNX/progress.json

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
press Down
shot 02-continue-watching 2
press Up
press Return
shot 03-anime-top 16
press Down 2
shot 04-anime-grid 4
press Down
press Right 2
shot 05-anime-grid-2 4
press Escape
shot 06-back-home 3
press Up 12
press Right 5
press Return
shot 07-settings 4
press Down 6
shot 08-settings-scrolled 2
press Escape
press Left 5
press Return
shot 09-browse 4
press Escape
press Right
press Return
shot 10-my-list 3

kill "$APP" || true
echo "screenshots in $OUT"
