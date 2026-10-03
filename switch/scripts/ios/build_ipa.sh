#!/usr/bin/env bash
# Builds an unsigned AnikkuNX.ipa (arm64, iOS 15+) into dist/. Needs macOS with Xcode.
# Sideload it with AltStore, Sideloadly, TrollStore or similar.
set -euo pipefail
cd "$(dirname "$0")/../.."
bash scripts/setup.sh
bash scripts/ios/fetch_deps.sh
# borealis' iOS toolchain insists on a libromfs generator; this build ships resources as files instead
touch libromfs-generator

cmake -B build-ios -G Xcode -DPLATFORM_IOS=ON -DPLATFORM=OS64 -DDEPLOYMENT_TARGET=15.0 -DCMAKE_BUILD_TYPE=Release
cmake --build build-ios --config Release -- -quiet CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO

APP=$(find build-ios -type d -name "AnikkuNX.app" -path "*Release-iphoneos*" | head -1)
[ -n "$APP" ] || { echo "AnikkuNX.app not found"; exit 1; }
mkdir -p ../dist
rm -rf ../dist/Payload ../dist/AnikkuNX.ipa
mkdir -p ../dist/Payload
cp -R "$APP" ../dist/Payload/
STAGED=../dist/Payload/AnikkuNX.app

# exactly one Info.plist in the bundle: sideloaders often read the first one they find
EXTRA=$(find "$STAGED" -name Info.plist ! -path "$STAGED/Info.plist")
[ -z "$EXTRA" ] || { echo "unexpected nested Info.plist: $EXTRA"; exit 1; }

# ad-hoc signature, like any other app; AltStore/Sideloadly/ESign/Feather re-sign it on install
codesign --force --sign - --timestamp=none "$STAGED"
codesign --verify --verbose "$STAGED"

# Info.plist and icons go first in the archive, then everything else
(cd ../dist &&
    zip -q AnikkuNX.ipa Payload/ Payload/AnikkuNX.app/ Payload/AnikkuNX.app/Info.plist Payload/AnikkuNX.app/AppIcon*.png &&
    zip -qr AnikkuNX.ipa Payload &&
    rm -rf Payload)
unzip -l ../dist/AnikkuNX.ipa | head -8
ls -la ../dist
