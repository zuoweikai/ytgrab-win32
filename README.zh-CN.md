# YtGrab（Win32 版）

[English](README.md) | **简体中文**

一款基于 [yt-dlp](https://github.com/yt-dlp/yt-dlp) 的 Windows 视频下载器，小巧快速。使用 C++ 和原生 Win32 控件编写，静态编译为单个约 350 KB 的 `YtGrab.exe`，无需安装 .NET、VC++ 运行库或 Python。发布包已内置 yt-dlp 和 ffmpeg。

想要 Windows 11 风格界面？请看 WinUI 3 版：[ytgrab-winui](https://github.com/zuoweikai/ytgrab-winui)。

## 下载

在 [Releases](https://github.com/zuoweikai/ytgrab-win32/releases/latest) 获取最新版本：

| 文件 | 适用于 |
|---|---|
| `YtGrab-Win32-x64.zip` | 绝大多数 Windows 电脑（Intel / AMD） |
| `YtGrab-Win32-arm64.zip` | ARM 设备（骁龙笔记本、Surface Pro X 等） |

解压后运行 `YtGrab.exe`，支持 Windows 10 / 11。

## 使用

1. 解压后运行 `YtGrab.exe`。yt-dlp 和 ffmpeg 已放在 `tools` 文件夹里，4K、音视频合并、转 MP3 都能直接用。
2. 粘贴链接（可一次粘贴多条），选格式和保存目录，按 Enter 或“添加下载”。在列表里按 Ctrl+V 也能直接添加。

查找顺序：设置里的路径 → `tools` 文件夹 → 程序目录 → `%LOCALAPPDATA%\YtGrab\bin` → PATH。
设置保存在 `%APPDATA%\YtGrab\settings.ini`；在 exe 旁放一个空的 `settings.ini` 即为便携模式。

## 功能

- 多任务并发队列（1–10 个），每个任务分片多线程（`-N`，1–16）
- 格式预设：最佳 / 4K / 2K / 1080p / 720p / 480p / MP4(H.264) 兼容 / MP3 / M4A / 原始音频
- 播放列表、字幕（中/英）、嵌入封面、限速、代理、读取浏览器 Cookies
- 内置浏览器（WebView2）：在软件里登录任意网站，下载自动使用登录状态；小红书主页可抓取全部视频和图文笔记
- 实时进度（系统主题进度条）、速度、剩余时间、任务栏进度、完成时任务栏闪烁
- 右键：打开文件、打开所在文件夹（并选中）、重新下载、取消、复制链接、移除、清除已完成
- 每个下载在独立 Job Object 中运行，取消或退出会连同 ffmpeg 子进程一起结束
- Per-Monitor V2 高 DPI、系统字体、Explorer 风格列表、单实例

## 编译

- **MinGW-w64（Linux / MSYS2）**：`sh src/build.sh`（x64）。ARM64 使用 [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)：`CXX=aarch64-w64-mingw32-clang++ WINDRES=aarch64-w64-mingw32-windres sh src/build.sh`
- **Visual Studio**：`cmake -S src -B build -A x64 && cmake --build build --config Release`（静态 CRT）
- **GitHub Actions**：每次推送到 `main` 自动编译 x64 和 ARM64；`VERSION` 变化时自动发布 `v<版本号>` Release 并附上两个 zip。

## 免责声明

YtGrab 只是 yt-dlp 的图形界面。请仅下载你有权下载的内容，并遵守各网站的服务条款及当地版权法律。
