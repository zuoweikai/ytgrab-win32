#!/bin/sh
# Cross-compile with MinGW-w64 (Linux / MSYS2). Produces a fully static YtGrab.exe.
#   x64   (default): CXX=x86_64-w64-mingw32-g++
#   ARM64 (llvm-mingw): CXX=aarch64-w64-mingw32-clang++ WINDRES=aarch64-w64-mingw32-windres
#   OUT=path/to/YtGrab.exe to choose the output file
set -e
cd "$(dirname "$0")"
CXX=${CXX:-x86_64-w64-mingw32-g++}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
OUT=${OUT:-../dist/YtGrab.exe}
mkdir -p "$(dirname "$OUT")"
$WINDRES --codepage=65001 -O coff app.rc -o app.res.o
$CXX -std=c++17 -O2 -DNDEBUG -municode -mwindows -static -s \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  main.cpp app.res.o -o "$OUT" \
  -lcomctl32 -luxtheme -lshell32 -lshlwapi -lole32 -luuid -lurlmon -lwininet -ldwmapi -lcomdlg32 -lgdi32
rm -f app.res.o
echo "built $OUT"
