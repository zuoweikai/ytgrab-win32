## What's new / 更新内容

- **New playlist prompt.** A clean panel with a bold heading and two choices ("Only this video" is selected), plus OK and Cancel.
  **播放列表询问改版。** 新样式面板：粗体标题、两个选项（默认「只下载当前视频」），右下角「确定」「取消」。
- **Speed limit box lines up with its unit box.**
  **限速输入框与单位下拉框对齐。**
- **No more sounds.** Every message and confirmation is now a silent panel in the same style; Delete uses a red button with Cancel focused by default.
  **全程无提示音。** 所有提示和确认框都换成同样式的静音面板；删除文件用红色按钮，默认选中「取消」。

### Earlier / 之前

- **Subtitles in 13 languages.** Settings > Subtitles now has a **Languages…** button: pick any of English, Simplified/Traditional Chinese, Japanese, Korean, Spanish, French, German, Russian, Vietnamese, Thai, Portuguese, Arabic (Select all / Clear). Default stays English + Simplified Chinese.
  **字幕支持 13 种语言。** 设置里「下载字幕」旁新增「字幕语言…」按钮，可多选英语、简体/繁体中文、日语、韩语、西班牙语、法语、德语、俄语、越南语、泰语、葡萄牙语、阿拉伯语（全选 / 清空），默认仍为英语 + 简体中文。
- **Speed limit: number + unit.** Type just a number and pick KB/s or MB/s; empty = unlimited. Old values like 5M are converted.
  **限速改为「数字 + 单位」。** 只填数字，后面选 KB/s 或 MB/s，留空不限速；以前填的 5M 等会自动转换。
- **Components section simplified.** yt-dlp and ffmpeg ship with YtGrab, so the yt-dlp path box and Get ffmpeg button are gone; it shows the yt-dlp version and ffmpeg status, and keeps **Update yt-dlp**.
  **组件区简化。** yt-dlp 和 ffmpeg 已内置，去掉「yt-dlp 路径」和「获取 ffmpeg」，改为显示 yt-dlp 版本和 ffmpeg 状态，保留「更新 yt-dlp」。
- **Window and dialogs polish.** The Name column now fills the list width when you resize the window, status bar sections fit their text, Settings labels line up with their boxes, and the playlist prompt uses a plain row of buttons.
  **界面细节。** 缩放窗口时「名称」列自动填满宽度，状态栏各格按文字宽度显示完整，设置页标签与输入框对齐，播放列表询问弹窗改为底部一排按钮。

- **"Grab this page" on a single Xiaohongshu note now works.** It reads the note straight from the page you're looking at (including a note opened as a pop-up over the feed), falls back to the photos/video on screen, and says why when nothing was added (already in the list, or nothing readable). On other sites the button now shows a message instead of seeming to do nothing.
  **小红书单条笔记「抓取本页全部」修复。** 直接从当前页面读取笔记（包括在首页弹出的笔记），读不到时改用页面上显示的图片/视频；没加入任务时会说明原因（已在列表中或没读到内容）。在其他网站点击会弹出提示，不再像没反应。
- **Built-in browser (WebView2).** New **Sign in…** button: sign in to any site (YouTube, Bilibili, Douyin, Instagram, members-only content…) once inside YtGrab, and downloads use that sign-in automatically. This also works when Chrome/Edge cookies can't be read. Settings > Cookies from now defaults to **Built-in browser**. A failed download that needs a sign-in says so; right-click > **Sign in with built-in browser** opens that site.
  **内置浏览器（WebView2）。** 新增「登录网站…」按钮：在 YtGrab 里登录任意网站（YouTube、B 站、抖音、Instagram、会员内容等）一次，之后下载自动带上登录状态，Chrome/Edge 的 Cookies 读不出来也没关系。设置里「Cookies 来源」默认改为「内置浏览器」。需要登录而失败的任务会提示，右键「在内置浏览器中登录」直接打开该网站。
- **Xiaohongshu home pages, every note.** Paste a Xiaohongshu profile or share link (xhslink) and the built-in browser opens it, asks you to sign in the first time, then scrolls the page slowly at an uneven, human pace and reads each note one by one with pauses. Videos and image notes (all pictures, Live Photos too) are saved to `<save folder>\<creator>`. Single note links work too, including image notes. Large profiles take a while on purpose; Xiaohongshu may still ask for verification, in which case it waits and retries.
  **小红书主页全部笔记。** 粘贴小红书主页或分享短链，内置浏览器打开，首次需要在窗口里登录，之后像真人一样慢速、随机地滚动主页，再逐条读取笔记并穿插停顿。视频和图文（全部图片，含实况图）保存到「保存目录\博主名」。单条笔记链接（含图文）也支持。为降低风控，大主页会比较慢；若小红书要求验证，会自动等待后重试。

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
