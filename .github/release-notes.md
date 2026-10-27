## What's new / 更新内容

- **Playlists and channels become one task per video.** With "Download whole playlist" checked, a playlist, a channel, or a channel's Playlists page (e.g. `youtube.com/@name/playlists`) is read first and every video gets its own row with its own progress. Videos are saved in a subfolder named after their playlist.
  **播放列表和频道展开为每个视频一条任务。** 勾选「下载整个播放列表」后，播放列表、频道或频道的「播放列表」页（如 `youtube.com/@name/playlists`）会先读出全部视频，每个视频单独一行、单独显示进度，并按播放列表名存到子文件夹。
- Includes Deno, yt-dlp 2026.10.26 and latest ffmpeg.
  内置 Deno、yt-dlp 2026.10.26 与最新 ffmpeg。

## Download / 下载

| File / 文件 | For / 适用于 |
|---|---|
| `YtGrab-Win32-x64.zip` | Most PCs (Intel / AMD) · 大多数电脑 |
| `YtGrab-Win32-arm64.zip` | ARM devices (Snapdragon etc.) · ARM 设备（骁龙等） |

Unzip and run `YtGrab.exe`. Windows 10 / 11. No .NET, VC++ runtime, or Python needed.
解压后运行 `YtGrab.exe`，支持 Windows 10 / 11，无需安装 .NET、VC++ 运行库或 Python。

ffmpeg is from [yt-dlp/FFmpeg-Builds](https://github.com/yt-dlp/FFmpeg-Builds) (GPL, see `tools\ffmpeg-LICENSE.txt`).
Windows 11 style edition / Windows 11 风格版本：[ytgrab-winui](https://github.com/zuoweikai/ytgrab-winui)
