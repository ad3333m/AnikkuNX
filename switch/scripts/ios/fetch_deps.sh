#!/usr/bin/env bash
# Downloads prebuilt iOS (arm64 device) static libraries into switch/library/ios-deps:
#   - libmpv, FFmpeg and their dependencies from MPVKit (LGPL build)
#   - libcurl + nghttp2 from Build-OpenSSL-cURL (OpenSSL itself comes from MPVKit, to avoid duplicates)
# Run on macOS (needs lipo).
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT="$(pwd)"
OUT="$ROOT/library/ios-deps"
MPVKIT_TAG=1.0.0
CURL_TGZ=https://github.com/jasonacox/Build-OpenSSL-cURL/releases/download/1.0.3/libcurl-8.17.0-openssl-3.0.18-nghttp2-1.68.0.tgz
WORK="${RUNNER_TEMP:-/tmp}/iosdeps"

rm -rf "$OUT" "$WORK"
mkdir -p "$OUT/lib" "$OUT/include" "$WORK"
cd "$WORK"

# Copy the iOS device slice of an xcframework as lib<Name>.a (thinned to arm64).
take() {
    local xc="$1" name slice bin
    name=$(basename "$xc" .xcframework)
    slice=$(find "$xc" -mindepth 1 -maxdepth 1 -type d -name 'ios-arm64*' ! -name '*simulator*' ! -name '*maccatalyst*' | head -1)
    if [ -z "$slice" ]; then
        echo "!! no ios-arm64 slice in $xc"; ls "$xc"; return 1
    fi
    if [ -d "$slice/$name.framework" ]; then
        bin="$slice/$name.framework/$name"
    else
        bin=$(find "$slice" -maxdepth 2 -name '*.a' | head -1)
    fi
    if [ -z "$bin" ] || [ ! -f "$bin" ]; then
        echo "!! no library in $slice"; find "$slice" -maxdepth 3; return 1
    fi
    lipo "$bin" -thin arm64 -output "$OUT/lib/lib$name.a" 2>/dev/null || cp "$bin" "$OUT/lib/lib$name.a"
    echo "  $name <- ${bin#$WORK/}"
}

echo "== MPVKit $MPVKIT_TAG"
curl -fsSL "https://raw.githubusercontent.com/mpvkit/MPVKit/$MPVKIT_TAG/Package.swift" -o Package.swift
grep -o 'https://[^"]*\.xcframework\.zip' Package.swift | sort -u \
    | grep -v -- '-GPL\.xcframework' | grep -v 'Libluajit' | grep -v 'Libsmbclient' > urls.txt
cat urls.txt
mkdir -p mpvkit
while read -r url; do
    f="mpvkit/$(basename "$url")"
    curl -fsSL --retry 4 --retry-delay 5 "$url" -o "$f"
    unzip -q -o "$f" -d mpvkit
done < urls.txt
for xc in mpvkit/*.xcframework; do take "$xc"; done
cp -R "mpvkit/Libmpv.xcframework/ios-arm64/Libmpv.framework/Headers/." "$OUT/include/"

echo "== libcurl"
curl -fsSL --retry 4 --retry-delay 5 "$CURL_TGZ" -o curl.tgz
mkdir -p curlpkg && tar xzf curl.tgz -C curlpkg
find curlpkg -maxdepth 4 -type d | head -40
for lib in libcurl libnghttp2; do
    xc=$(find curlpkg -type d -name "$lib.xcframework" | head -1)
    if [ -n "$xc" ]; then
        take "$xc"
    else
        fat=$(find curlpkg -name "${lib}_iOS.a" -o -name "${lib}-iOS.a" | head -1)
        [ -n "$fat" ] || { echo "!! $lib not found"; find curlpkg -name '*.a' | head -40; exit 1; }
        lipo "$fat" -thin arm64 -output "$OUT/lib/$lib.a"
        echo "  $lib <- $fat"
    fi
done
hdr=$(find curlpkg -path '*/curl/curl.h' | grep -iv 'simulator\|macos\|tvos\|catalyst\|visionos\|xros' | head -1)
[ -n "$hdr" ] || hdr=$(find curlpkg -path '*/curl/curl.h' | head -1)
cp -R "$(dirname "$hdr")" "$OUT/include/"

echo "== result"
ls -la "$OUT/lib"
ls "$OUT/include"
