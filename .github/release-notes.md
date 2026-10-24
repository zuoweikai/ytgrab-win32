## What's new / 更新内容

- **yt-dlp and ffmpeg are now included.** Nothing to download on first run: 4K, merging, and MP3 work right away. They live in the `tools` folder next to `YtGrab.exe`.
  **内置 yt-dlp 和 ffmpeg。** 首次运行无需再下载，4K、音视频合并、转 MP3 开箱即用，组件放在 `YtGrab.exe` 旁边的 `tools` 文件夹里。
- ARM64 package uses native ARM64 builds of yt-dlp and ffmpeg.
  ARM64 版使用原生 ARM64 的 yt-dlp 和 ffmpeg。

## Download / 下载

| File / 文件 | For / 适用于 |
|---|---|
| `YtGrab-Win32-x64.zip` | Most PCs (Intel / AMD) · 大多数电脑 |
| `YtGrab-Win32-arm64.zip` | ARM devices (Snapdragon etc.) · ARM 设备（骁龙等） |

Unzip and run `YtGrab.exe`. Windows 10 / 11. No .NET, VC++ runtime, or Python needed.
解压后运行 `YtGrab.exe`，支持 Windows 10 / 11，无需安装 .NET、VC++ 运行库或 Python。

ffmpeg is from [yt-dlp/FFmpeg-Builds](https://github.com/yt-dlp/FFmpeg-Builds) (GPL, see `tools\ffmpeg-LICENSE.txt`).
Windows 11 style edition / Windows 11 风格版本：[ytgrab-winui](https://github.com/zuoweikai/ytgrab-winui)
