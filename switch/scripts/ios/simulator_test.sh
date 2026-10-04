#!/usr/bin/env bash
# Builds AnikkuNX for the arm64 iOS Simulator, runs it on an iPhone 16 Pro Max (same 2868x1320
# screen as the 17 Pro Max) and a 13" iPad Pro (same 4:3 shape as the 12.9"), and saves
# screenshots, console output and crash reports to sim-preview/. Needs macOS with Xcode.
set -uo pipefail
cd "$(dirname "$0")/../.."
OUT="$(pwd)/../sim-preview"
mkdir -p "$OUT"
BUNDLE=com.ad3333m.anikkunx

set -e
bash scripts/setup.sh
bash scripts/ios/fetch_deps.sh simulator
touch libromfs-generator
cmake -B build-sim -G Xcode -DPLATFORM_IOS=ON -DPLATFORM=SIMULATORARM64 -DDEPLOYMENT_TARGET=15.0 -DCMAKE_BUILD_TYPE=Release
cmake --build build-sim --config Release -- -quiet CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO
APP=$(find build-sim -type d -name "AnikkuNX.app" -path "*iphonesimulator*" | head -1)
[ -n "$APP" ] || { echo "simulator app not found"; exit 1; }
set +e
codesign -dv "$APP/AnikkuNX" 2>&1 | head -5   # the linker already ad-hoc signs arm64 simulator binaries

xcrun simctl list devicetypes | grep -i -E "iphone 1[6-7] pro max|ipad pro" | tee "$OUT/devicetypes.txt"
run_on() {
    local tag="$1" type="$2"
    local id
    id=$(xcrun simctl create "anx-$tag" "$type") || { echo "cannot create $type"; return; }
    xcrun simctl boot "$id"
    xcrun simctl bootstatus "$id" -b
    xcrun simctl install "$id" "$APP"
    local data
    data=$(xcrun simctl get_app_container "$id" "$BUNDLE" data)
    mkdir -p "$data/Documents/AnikkuNX"
    echo '{"sourcesChosen": true, "checkUpdates": false, "checkNewEpisodes": false, "enabledSources": ["en.anichi", "en.anikoto", "en.aniwave", "en.animesogo", "en.kickassanime", "en.wcofun"]}' \
        > "$data/Documents/AnikkuNX/config.json"
    xcrun simctl launch --stdout="$OUT/$tag-stdout.txt" --stderr="$OUT/$tag-stderr.txt" "$id" "$BUNDLE" \
        > "$OUT/$tag-launch.txt" 2>&1
    cat "$OUT/$tag-launch.txt"
    sleep 8
    xcrun simctl io "$id" screenshot "$OUT/$tag-0-start.png"
    xcrun simctl spawn "$id" launchctl list 2>/dev/null | grep -i anikku > "$OUT/$tag-running-8s.txt"
    sleep 50
    xcrun simctl io "$id" screenshot "$OUT/$tag-1-home.png"
    sleep 20
    xcrun simctl io "$id" screenshot "$OUT/$tag-2-later.png"
    xcrun simctl spawn "$id" launchctl list 2>/dev/null | grep -i anikku > "$OUT/$tag-running-80s.txt"
    xcrun simctl spawn "$id" log show --last 5m --style compact \
        --predicate 'process == "AnikkuNX" OR eventMessage CONTAINS[c] "anikkunx"' > "$OUT/$tag-syslog.txt" 2>&1
    find ~/Library/Logs/DiagnosticReports -iname "*AnikkuNX*" -exec cp {} "$OUT/" \; 2>/dev/null
    xcrun simctl shutdown "$id"
}
run_on phone "iPhone 17 Pro Max"
run_on ipad "iPad Pro (12.9-inch) (5th generation)"
find ~/Library/Logs/DiagnosticReports -iname "*AnikkuNX*" -exec cp {} "$OUT/" \; 2>/dev/null
ls -la "$OUT"
exit 0
