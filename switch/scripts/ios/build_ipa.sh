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

# left unsigned with no signing data (like IPAs published for KravaSigner/ESign): the signer adds its own
rm -rf "$STAGED/_CodeSignature" "$STAGED/embedded.mobileprovision"

# Plain zip: file entries only and no "extra fields". Info-ZIP's timestamp extras differ in length
# between local and central headers, and KravaSigner's unzipper then reads Info.plist as garbage
# ("Unknown", no icon).
(cd ../dist && python3 - <<'PY'
import os, time, zipfile
stamp = time.localtime()[:6]
with zipfile.ZipFile("AnikkuNX.ipa", "w", zipfile.ZIP_DEFLATED, compresslevel=6) as out:
    files = []
    for root, _, names in os.walk("Payload"):
        files += [os.path.join(root, n) for n in names]
    for path in sorted(files):
        info = zipfile.ZipInfo(path.replace(os.sep, "/"), date_time=stamp)
        info.create_system = 3
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = (0o100755 if os.access(path, os.X_OK) else 0o100644) << 16
        with open(path, "rb") as f:
            out.writestr(info, f.read())
PY
rm -rf Payload)
unzip -l ../dist/AnikkuNX.ipa | head -8
ls -la ../dist
