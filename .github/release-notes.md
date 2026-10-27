## What's new / 更新内容

- **Your task list survives a restart.** Tasks are saved automatically; finished ones stay in the list, and unfinished downloads resume when you open YtGrab again.
  **关闭软件后任务不再丢失。** 任务列表自动保存，已完成的保留在列表中，未完成的下次打开自动继续下载。
- **Reading a playlist no longer adds a placeholder row.** The status bar shows "Reading lists N" until the videos appear in the list.
  **读取列表时不再在表格里放占位行**，状态栏显示「读取列表 N」，读完后直接出现视频任务。
- Playlists and channels become one task per video (since the previous version), saved in a subfolder per playlist.
  播放列表和频道展开为每个视频一条任务，按播放列表名存到子文件夹。
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
