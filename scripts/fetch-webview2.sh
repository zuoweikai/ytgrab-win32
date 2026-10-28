#!/usr/bin/env bash
# Fetches the WebView2 SDK: header for the build (src/wv2) and WebView2Loader.dll for the app.
#   scripts/fetch-webview2.sh <x64|arm64> [tools-dir]
set -euo pipefail
ARCH=${1:-x64}
TOOLS=${2:-}
VER=1.0.4314.35
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
curl -fsSL -o "$TMP/wv2.zip" "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/$VER"
unzip -q "$TMP/wv2.zip" 'build/native/*' -d "$TMP"
mkdir -p "$ROOT/src/wv2"
cp "$TMP/build/native/include/WebView2.h" "$ROOT/src/wv2/"
# WebView2.h includes "EventToken.h"; MinGW ships it as eventtoken.h (matters on case-sensitive hosts)
printf '#pragma once\n#include <eventtoken.h>\n' > "$ROOT/src/wv2/EventToken.h"
if [ -n "$TOOLS" ]; then
  mkdir -p "$TOOLS"
  cp "$TMP/build/native/$ARCH/WebView2Loader.dll" "$TOOLS/"
  cp "$TMP/LICENSE.txt" "$TOOLS/WebView2-LICENSE.txt" 2>/dev/null || true
fi
rm -rf "$TMP"
echo "WebView2 SDK $VER ready ($ARCH)"
