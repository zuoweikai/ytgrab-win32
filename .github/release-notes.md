## What's new / 更新内容

- **Creator home pages**: a channel / profile link (YouTube @channel, Bilibili space, TikTok, Douyin, Instagram, Kuaishou, Xiaohongshu) saves its videos in a folder named after the creator.
  解析博主主页（YouTube 频道、B 站空间、TikTok、抖音、Instagram、快手、小红书）时，先建一个以博主名命名的文件夹再保存。
- **"Parse & download"** is the new name of the add button. The task menu gains **Delete file from disk** (to the Recycle Bin, after a confirmation).
  按钮改名为「解析并下载」；任务右键菜单新增「从硬盘删除文件」（确认后移到回收站）。
- **Playlist prompt.** The "Whole playlist" checkbox is gone. A video link that sits inside a playlist now asks: only this video, or the whole playlist. Playlist and channel links still expand into one task per video.
  **播放列表弹窗询问。** 去掉「下载整个播放列表」复选框；粘贴的视频链接属于播放列表时，弹窗询问只下载当前视频还是整个列表。
- **Start all / Stop all** buttons after "Parse & download". Stop pauses running and queued tasks; Start resumes paused, failed and canceled ones.
  **新增「开始全部」「停止全部」按钮。**
- **No auto-start on launch.** Restored unfinished tasks show as Paused until you click Start all.
  **打开软件不再自动开始下载**，恢复的未完成任务显示为「已暂停」。
- **English interface.** Settings now has "Language: System default / 简体中文 / English". Changing it offers an instant restart; unfinished downloads resume automatically.
  **新增英文界面。** 设置里可选「界面语言：跟随系统 / 简体中文 / English」，切换后可一键重启生效，未完成的下载会自动继续。
- **Simpler input row.** "Parse & add" now sits right before "Download whole playlist"; "Paste & download" was removed. Only playlist/channel links that are still being read show "Parsing…"; rows already in the list just show "Preparing".
  **输入区更简洁。**「解析并添加下载」移到「下载整个播放列表」前面，删除「粘贴并下载」；只有还在读取的播放列表/频道显示「正在解析」，已进入下载列表的任务显示「准备中」。
- **Cleaner right-click menu in the text boxes:** Undo, Cut, Copy, Paste, Delete, Select all, without the reading-order / Unicode / IME entries.
  **输入框右键菜单精简为**撤销、剪切、复制、粘贴、删除、全选，去掉阅读顺序、Unicode 控制字符和输入法项。
- **Instant feedback when you add a link.** The Add button confirms with "✓ Added", a spinning "Parsing N link(s)…" indicator shows while YouTube is being read, and rows that are still being parsed show a moving bar instead of a frozen 0%. Adding a link that is already in the list just says so instead of creating a duplicate.
  **添加链接立即有反馈。** 点击后按钮显示「✓ 已添加」，读取期间显示转圈的「正在解析 N 个链接…」，正在解析的任务行显示滚动进度条而不是停在 0%；重复添加同一链接会提示「已在列表中」，不会生成重复任务。
- Task list survives a restart; unfinished downloads resume automatically.
  任务列表关闭后保留，未完成的下次打开自动继续。
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
