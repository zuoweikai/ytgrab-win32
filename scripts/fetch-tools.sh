#!/usr/bin/env bash
# Download the yt-dlp, Deno and ffmpeg builds that ship inside the YtGrab release zip.
# Usage: scripts/fetch-tools.sh <x64|arm64> <dest-dir>
# Works on Linux and in Git Bash on Windows (needs curl plus unzip or 7z).
#   yt-dlp: latest release of yt-dlp/yt-dlp (native ARM64 build on arm64)
#   deno:   latest denoland/deno release; yt-dlp finds deno.exe next to yt-dlp.exe and uses it
#           to solve YouTube's JavaScript challenges (required for YouTube downloads)
#   ffmpeg: yt-dlp/FFmpeg-Builds, GPL shared build (ffmpeg.exe + ffprobe.exe share the DLLs)
set -euo pipefail
arch=${1:?arch}; dest=${2:?dest}
case "$arch" in
  x64)   yt=yt-dlp.exe;       ff=win64;    dn=x86_64 ;;
  arm64) yt=yt-dlp_arm64.exe; ff=winarm64; dn=aarch64 ;;
  *) echo "unknown arch $arch" >&2; exit 1 ;;
esac
mkdir -p "$dest"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

curl -fsSL --retry 3 -o "$dest/yt-dlp.exe" "https://github.com/yt-dlp/yt-dlp/releases/latest/download/$yt"

curl -fsSL --retry 3 -o "$tmp/deno.zip" "https://github.com/denoland/deno/releases/latest/download/deno-$dn-pc-windows-msvc.zip"
if command -v unzip >/dev/null 2>&1; then unzip -q -o "$tmp/deno.zip" -d "$dest"; else 7z x -y -o"$dest" "$tmp/deno.zip" >/dev/null; fi
[ -f "$dest/deno.exe" ] || { echo "deno.exe missing" >&2; exit 1; }

curl -fsSL --retry 3 -o "$tmp/ff.zip" "https://github.com/yt-dlp/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-$ff-gpl-shared.zip"
if command -v unzip >/dev/null 2>&1; then unzip -q "$tmp/ff.zip" -d "$tmp/ff"; else 7z x -y -o"$tmp/ff" "$tmp/ff.zip" >/dev/null; fi
bin=$(dirname "$(find "$tmp/ff" -type f -name ffmpeg.exe | head -n1)")
root=$(dirname "$bin")
cp "$bin/ffmpeg.exe" "$bin/ffprobe.exe" "$dest/"
for f in "$bin"/*.dll; do
  case "$(basename "$f")" in SDL2*.dll) ;; *) cp "$f" "$dest/" ;; esac   # SDL is only for ffplay
done
[ -f "$root/LICENSE.txt" ] && cp "$root/LICENSE.txt" "$dest/ffmpeg-LICENSE.txt"
echo "yt-dlp, deno and ffmpeg ($arch) -> $dest"
ls -la "$dest"
