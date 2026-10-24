# YtGrab (Win32)

**English** | [简体中文](README.zh-CN.md)

A tiny, fast Windows video downloader built on [yt-dlp](https://github.com/yt-dlp/yt-dlp). Written in C++ with plain Win32 controls and compiled into a single static `YtGrab.exe` of about 350 KB: no .NET, VC++ runtime, or Python to install. yt-dlp and ffmpeg come bundled in the release.

Prefer a Windows 11 look? See the WinUI 3 edition: [ytgrab-winui](https://github.com/zuoweikai/ytgrab-winui).

## Download

Get the latest build from [Releases](https://github.com/zuoweikai/ytgrab-win32/releases/latest):

| File | For |
|---|---|
| `YtGrab-Win32-x64.zip` | Most Windows PCs (Intel / AMD) |
| `YtGrab-Win32-arm64.zip` | ARM devices (Snapdragon laptops, Surface Pro X, etc.) |

Unzip and run `YtGrab.exe` on Windows 10 or 11.

## Getting started

1. Unzip and run `YtGrab.exe`. yt-dlp and ffmpeg are already in the `tools` folder, so 4K, merging, and MP3 work right away.
2. Paste one or more links, choose a format and folder, then press Enter or "Add". `Ctrl+V` in the list adds links directly.

yt-dlp is found in this order: path in Settings → `tools` folder → app folder → `%LOCALAPPDATA%\YtGrab\bin` → `PATH`.
Settings are stored in `%APPDATA%\YtGrab\settings.ini`; put an empty `settings.ini` next to the exe for portable mode.

## Features

- Parallel queue (1–10 tasks), each with 1–16 fragment threads (`-N`)
- Presets: Best / 4K / 2K / 1080p / 720p / 480p / MP4 (H.264) / MP3 / M4A / original audio
- Playlists, subtitles (Chinese / English), embedded thumbnails, speed limit, proxy, browser cookies
- Live progress with themed progress bars, speed, ETA, taskbar progress, and a taskbar flash when done
- Right-click: open file, show in folder, retry, cancel, copy link, remove, clear completed
- Every download runs in its own Job Object, so cancelling or quitting also stops ffmpeg
- Per-Monitor V2 high DPI, system font, Explorer-style list, single instance

The interface is in Simplified Chinese.

## Building

- **MinGW-w64 (Linux / MSYS2)**: `sh src/build.sh` (x64). For ARM64 with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw): `CXX=aarch64-w64-mingw32-clang++ WINDRES=aarch64-w64-mingw32-windres sh src/build.sh`
- **Visual Studio**: `cmake -S src -B build -A x64 && cmake --build build --config Release` (static CRT)
- **GitHub Actions**: every push to `main` builds x64 and ARM64. When `VERSION` changes, Release `v<VERSION>` is published automatically with both zips.

## Disclaimer

YtGrab is a front end for yt-dlp. Only download content you have the right to download, and respect each site's terms of service and local copyright law.
