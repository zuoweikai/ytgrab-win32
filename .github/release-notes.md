## What's new / 更新内容

- **Deno is now included.** YouTube now requires a JavaScript runtime for downloads; YtGrab ships `deno.exe` next to `yt-dlp.exe`, so YouTube works on a clean PC with nothing to install.
  **内置 Deno。** YouTube 下载现在需要 JavaScript 运行时，YtGrab 已在 `yt-dlp.exe` 旁边附带 `deno.exe`，干净电脑上也能直接下载，无需另装。
- yt-dlp 2026.10.26 (security fixes) and latest ffmpeg.
  yt-dlp 2026.10.26（含安全修复）与最新 ffmpeg。

## Download / 下载

| File / 文件 | For / 适用于 |
|---|---|
| `YtGrab-Win32-x64.zip` | Most PCs (Intel / AMD) · 大多数电脑 |
| `YtGrab-Win32-arm64.zip` | ARM devices (Snapdragon etc.) · ARM 设备（骁龙等） |

Unzip and run `YtGrab.exe`. Windows 10 / 11. No .NET, VC++ runtime, or Python needed.
解压后运行 `YtGrab.exe`，支持 Windows 10 / 11，无需安装 .NET、VC++ 运行库或 Python。

ffmpeg is from [yt-dlp/FFmpeg-Builds](https://github.com/yt-dlp/FFmpeg-Builds) (GPL, see `tools\ffmpeg-LICENSE.txt`).
Windows 11 style edition / Windows 11 风格版本：[ytgrab-winui](https://github.com/zuoweikai/ytgrab-winui)
