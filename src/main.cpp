// YtGrab — a native Win32 front-end for yt-dlp.
// Single static executable, no .NET / VC++ runtime required.
// Build: see build.sh (MinGW-w64) or CMakeLists.txt (MSVC / MinGW).

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <urlmon.h>
#include <wininet.h>
#include <dwmapi.h>

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <functional>
#include <algorithm>
#include <cwctype>

#include "resource.h"

// ---------------------------------------------------------------------------
// Constants / IDs
// ---------------------------------------------------------------------------
enum {
    IDC_URLLBL = 200, IDC_URL, IDC_PASTE, IDC_FMTLBL, IDC_FMT, IDC_DIRLBL, IDC_DIR,
    IDC_BROWSE, IDC_STARTALL, IDC_STOPALL, IDC_SETTINGS, IDC_OPENDIR, IDC_LOGIN, IDC_LIST, IDC_STATUS, IDC_BUSY, IDC_BUSYTXT,
    ID_OPENFILE = 300, ID_OPENFOLDER, ID_COPYURL, ID_RETRY, ID_CANCELTASK, ID_REMOVE,
    ID_CLEARDONE, ID_SELALL, ID_DELFILE, ID_UPDATE_YTDLP, ID_GET_FFMPEG, ID_GET_YTDLP, ID_LOGINSITE
};
#define IDC_ADD IDOK

enum {
    WM_APP_DONE = WM_APP + 1,   // a worker finished
    WM_APP_TOOLS,               // lParam: std::wstring* yt-dlp version
    WM_APP_COMP,                // wParam: 0 progress, 1 ok, 2 fail ; lParam: std::wstring*
    WM_APP_EXPANDED,            // a playlist placeholder was expanded; lParam: number of new tasks
};

static const wchar_t* kAppName  = L"YtGrab 视频下载器";

// ---------------------------------------------------------------------------
// UI language: Chinese strings are the keys; English comes from this table.
// ---------------------------------------------------------------------------
static bool g_en = false;
struct TrPair { const wchar_t* zh; const wchar_t* en; };
static const TrPair EN_TABLE[] = {
    { L"开始全部", L"Start all" },
    { L"，未完成的点「开始全部」继续", L"; click \"Start all\" to resume unfinished ones" },
    { L"停止全部", L"Stop all" },
    { L"已暂停", L"Paused" },
    { L"只下载当前视频", L"Only this video" },
    { L"这个链接属于一个播放列表", L"This link is part of a playlist" },
    { L"要只下载当前这个视频，还是下载整个播放列表？", L"Download only this video, or the whole playlist?" },
    { L"这个链接属于一个播放列表。\n\n是：只下载当前视频\n否：下载整个播放列表", L"This link is part of a playlist.\n\nYes: only this video\nNo: the whole playlist" },
    { L"已开始 ", L"Started " },
    { L"没有可开始的任务", L"Nothing to start" },
    { L"已暂停 ", L"Paused " },
    { L"没有正在下载的任务", L"Nothing is downloading" },
    { L"YtGrab 视频下载器", L"YtGrab Video Downloader" },
    { L"最佳画质（视频 + 音频）", L"Best quality (video + audio)" },
    { L"MP4 兼容（H.264，≤1080p）", L"MP4 compatible (H.264, ≤1080p)" },
    { L"仅音频 · MP3", L"Audio only · MP3" },
    { L"仅音频 · M4A", L"Audio only · M4A" },
    { L"仅音频 · 原始格式", L"Audio only · original" },
    { L"名称", L"Name" },
    { L"状态", L"Status" },
    { L"进度", L"Progress" },
    { L"大小", L"Size" },
    { L"速度", L"Speed" },
    { L"剩余", L"ETA" },
    { L"信息", L"Info" },
    { L"无法启动 yt-dlp：", L"Cannot start yt-dlp: " },
    { L"yt-dlp 退出代码 ", L"yt-dlp exit code " },
    { L"列表里没有视频", L"No videos in this list" },
    { L"正在下载 ", L"Downloading " },
    { L"yt-dlp 已安装到 ", L"yt-dlp installed to " },
    { L"yt-dlp 下载失败，请检查网络或代理（使用系统代理设置）。", L"Failed to download yt-dlp. Check your network or proxy (system proxy settings are used)." },
    { L"ffmpeg 下载失败，请检查网络或代理。", L"Failed to download ffmpeg. Check your network or proxy." },
    { L"正在解压 ffmpeg…", L"Extracting ffmpeg…" },
    { L"ffmpeg 已安装到 ", L"ffmpeg installed to " },
    { L"ffmpeg 解压失败（需要 Windows 10 1803 及以上自带的 tar）。", L"Failed to extract ffmpeg (needs the tar built into Windows 10 1803 or later)." },
    { L"正在更新 yt-dlp…", L"Updating yt-dlp…" },
    { L"yt-dlp 更新完成", L"yt-dlp updated" },
    { L"yt-dlp：未找到", L"yt-dlp: not found" },
    { L"ffmpeg：未找到", L"ffmpeg: not found" },
    { L"ffmpeg：就绪", L"ffmpeg: ready" },
    { L"没有找到 yt-dlp.exe。\n\n是否现在从 GitHub 自动下载最新版（约 18 MB）？\n也可以手动把 yt-dlp.exe 放在本程序同一目录下。", L"yt-dlp.exe was not found.\n\nDownload the latest version from GitHub now (about 18 MB)?\nYou can also put yt-dlp.exe in the same folder as this program." },
    { L"没有识别到 http/https 链接", L"No http/https link found" },
    { L"这个链接已在下载列表中", L"That link is already in the list" },
    { L"已添加 ", L"Added " },
    { L" 个链接", L" link(s)" },
    { L"，", L", " },
    { L" 个已在列表中", L" already in the list" },
    { L"✓ 已添加", L"✓ Added" },
    { L"已在列表中", L"Already in list" },
    { L"选择保存位置", L"Choose download folder" },
    { L"读取列表…", L"Reading list…" },
    { L"等待中", L"Queued" },
    { L"准备中…", L"Preparing…" },
    { L"下载中", L"Downloading" },
    { L"处理中…", L"Processing…" },
    { L"已完成", L"Done" },
    { L"失败", L"Failed" },
    { L"已取消", L"Canceled" },
    { L"错误：", L"Error: " },
    { L"已选择 ", L"" },
    { L" 项", L" selected" },
    { L"下载中 ", L"Downloading " },
    { L" · 等待 ", L" · Queued " },
    { L"读取列表 ", L"Reading lists " },
    { L"正在解析 ", L"Parsing " },
    { L" 个链接…", L" link(s)…" },
    { L"打开文件\tEnter", L"Open file\tEnter" },
    { L"打开所在文件夹", L"Show in folder" },
    { L"重新下载", L"Download again" },
    { L"取消", L"Cancel" },
    { L"复制链接", L"Copy link" },
    { L"移除\tDel", L"Remove\tDel" },
    { L"清除已完成", L"Clear finished" },
    { L"全选\tCtrl+A", L"Select all\tCtrl+A" },
    { L"不使用", L"None" },
    { L"例如 5M，留空不限速", L"e.g. 5M, empty = unlimited" },
    { L"例如 http://127.0.0.1:7890", L"e.g. http://127.0.0.1:7890" },
    { L"留空自动查找", L"Empty = find automatically" },
    { L"ffmpeg 未安装（合并高清需要）", L"ffmpeg missing (needed for HD merging)" },
    { L"ffmpeg 已就绪", L"ffmpeg ready" },
    { L"进度见主窗口状态栏", L"See progress in the main status bar" },
    { L"约 200 MB，进度见状态栏", L"About 200 MB, see status bar" },
    { L"撤销", L"Undo" },
    { L"剪切", L"Cut" },
    { L"复制", L"Copy" },
    { L"粘贴", L"Paste" },
    { L"删除", L"Delete" },
    { L"全选", L"Select all" },
    { L"视频链接（支持一次粘贴多个，按 Enter 添加）", L"Video links (paste several at once, press Enter to add)" },
    { L"解析并下载", L"Parse && download" },
    { L"从硬盘删除文件", L"Delete file from disk" },
    { L"确定从硬盘删除选中任务的 ", L"Delete " },
    { L" 个文件吗？\n文件会移到回收站，这些任务也会从列表中移除。", L" file(s) of the selected tasks from disk?\nThey go to the Recycle Bin, and the tasks are removed from the list." },
    { L"已删除 ", L"Deleted " },
    { L" 个文件", L" file(s)" },
    { L"格式", L"Format" },
    { L"保存到", L"Save to" },
    { L"浏览…", L"Browse…" },
    { L"下载整个播放列表", L"Whole playlist" },
    { L"打开文件夹", L"Open folder" },
    { L"设置…", L"Settings…" },
    { L"选择保存目录", L"Choose download folder" },
    { L"就绪", L"Ready" },
    { L"已恢复 ", L"Restored " },
    { L" 个任务", L" task(s)" },
    { L"还没有下载任务。粘贴视频链接后点击「解析并下载」，或在列表中按 Ctrl+V。", L"No downloads yet. Paste video links and click \"Parse && download\", or press Ctrl+V in the list." },
    { L"还有 ", L"" },
    { L" 个任务正在下载，退出将暂停它们，下次打开后点「开始全部」继续。确定退出吗？", L" download(s) still running. Quitting pauses them; click \"Start all\" next time to resume. Quit now?" },
    { L"列表已展开为 ", L"List expanded into " },
    { L" 个下载任务", L" downloads" },
    { L"完成", L"Done" },
    { L"失败，详见状态栏", L"Failed, see status bar" },
    { L"设置", L"Settings" },
    { L"下载", L"Downloads" },
    { L"同时下载任务数：", L"Parallel downloads:" },
    { L"每个任务的分片线程：", L"Fragments per task:" },
    { L"限速：", L"Rate limit:" },
    { L"下载字幕（中文 / 英文）", L"Subtitles (Chinese / English)" },
    { L"下载字幕", L"Subtitles" },
    { L"字幕语言…", L"Languages…" },
    { L"选择字幕语言", L"Subtitle languages" },
    { L"请选择需要下载的字幕语言（可多选）：", L"Choose the subtitle languages to download (any number):" },
    { L"全选", L"Select all" },
    { L"清空", L"Clear" },
    { L"请至少选择一种字幕语言", L"Choose at least one subtitle language" },
    { L"嵌入封面", L"Embed thumbnail" },
    { L"网络", L"Network" },
    { L"代理：", L"Proxy:" },
    { L"使用浏览器 Cookies：", L"Browser cookies:" },
    { L"组件", L"Components" },
    { L"正在下载视频", L"Downloading video" },
    { L"正在下载音频", L"Downloading audio" },
    { L"正在合并", L"Merging" },
    { L"正在处理", L"Processing" },
    { L"没有找到 yt-dlp", L"yt-dlp was not found" },
    { L"要现在从 GitHub 下载最新版吗（约 18 MB）？也可以手动把 yt-dlp.exe 放在本程序同一目录下。", L"Download the latest version from GitHub now (about 18 MB)? You can also put yt-dlp.exe in the same folder as this program." },
    { L"立即下载", L"Download" },
    { L"删除 ", L"Delete " },
    { L" 个文件？", L" file(s)?" },
    { L"文件会移到回收站，这些任务也会从列表中移除。", L"They go to the Recycle Bin, and the tasks are removed from the list." },
    { L" 个任务正在下载", L" download(s) still running" },
    { L"退出会暂停它们，下次打开后点「开始全部」继续。", L"Quitting pauses them. Click \"Start all\" next time to resume." },
    { L"退出", L"Quit" },
    { L"操作没有完成", L"That didn't finish" },
    { L"要下载哪些内容？", L"What do you want to download?" },
    { L"不限速", L"unlimited" },
    { L"未找到", L"not found" },
    { L"（内置）", L" (bundled)" },
    { L"未找到（合并高清需要）", L"not found (needed to merge HD)" },
    { L"已就绪", L"ready" },
    { L"网站常变，下载失败时先点这个", L"Sites change often; try this first if downloads fail" },
    { L"yt-dlp 路径：", L"yt-dlp path:" },
    { L"更新 yt-dlp", L"Update yt-dlp" },
    { L"下载 ffmpeg", L"Get ffmpeg" },
    { L"确定", L"OK" },
    { L"界面语言：", L"Language:" },
    { L"跟随系统", L"System default" },
    { L"登录网站…", L"Sign in…" },
    { L"在内置浏览器中登录", L"Sign in with built-in browser" },
    { L"内置浏览器（推荐）", L"Built-in browser (recommended)" },
    { L"Cookies 来源：", L"Cookies from:" },
    { L"YtGrab 内置浏览器", L"YtGrab built-in browser" },
    { L"前往", L"Go" },
    { L"抓取本页全部", L"Grab this page" },
    { L"停止抓取", L"Stop grabbing" },
    { L"已停止", L"Stopped" },
    { L"正在启动内置浏览器…", L"Starting the built-in browser…" },
    { L"输入要登录的网站，例如 youtube.com，按 Enter", L"Type a site to sign in to, e.g. youtube.com, then Enter" },
    { L"在这里登录网站后，下载时会自动使用登录状态（设置里 Cookies 选「内置浏览器」）", L"Sign in to sites here; downloads then use that sign-in (Settings: Cookies from built-in browser)" },
    { L"目前只能抓取小红书主页或笔记；其他网站登录后直接在主窗口粘贴链接即可", L"Grabbing works on Xiaohongshu profiles and notes; for other sites just sign in, then paste links in the main window" },
    { L"正在打开：", L"Opening: " },
    { L"小红书网页版打不开这条笔记（可能已删除、仅 App 可见或需要登录）", L"The Xiaohongshu website can't open this note (deleted, app-only, or sign-in needed)" },
    { L"正在打开主页…", L"Opening the profile…" },
    { L"请先在这个窗口登录小红书，登录后会自动继续", L"Please sign in to Xiaohongshu in this window; grabbing continues automatically" },
    { L"小红书要求验证，暂停 60 秒后重试…", L"Xiaohongshu asked for verification; waiting 60 s before retrying…" },
    { L"正在浏览主页，已找到 ", L"Browsing the profile, found " },
    { L" 条笔记", L" notes" },
    { L"正在读取笔记 ", L"Reading note " },
    { L"，已加入下载 ", L", queued " },
    { L"这个页面不是小红书主页或笔记", L"This page is not a Xiaohongshu profile or note" },
    { L"出错：", L"Error: " },
    { L"共 ", L"" },
    { L" 条笔记，开始逐条读取…", L" notes found, reading them one by one…" },
    { L"完成，本次加入下载 ", L"Done, queued " },
    { L"已经在下载列表里了，没有重复添加", L"Already in the download list, not added again" },
    { L"没读到图片或视频：请刷新页面（或先登录）后再点一次", L"Couldn't read any photo or video: refresh the page (or sign in) and click again" },
    { L" 条", L"" },
    { L"小红书：已加入 ", L"Xiaohongshu: queued " },
    { L"小红书链接已交给内置浏览器抓取", L"Xiaohongshu link handed to the built-in browser" },
    { L"需要登录：右键选「在内置浏览器中登录」，登录后重新下载。 ", L"Sign-in needed: right-click > Sign in with built-in browser, then retry. " },
    { L"无法连接：", L"Cannot connect: " },
    { L"无法写入文件：", L"Cannot write file: " },
    { L"下载中断", L"Download interrupted" },
    { L"下载不完整", L"Incomplete download" },
    { L"找不到 WebView2Loader.dll（应在 tools 文件夹中）", L"WebView2Loader.dll not found (it belongs in the tools folder)" },
    { L"无法启动内置浏览器", L"Could not start the built-in browser" },
    { L"\n\n内置浏览器需要 Microsoft Edge WebView2 运行时（Windows 10/11 通常已自带）。现在打开下载页面吗？", L"\n\nThe built-in browser needs the Microsoft Edge WebView2 Runtime (included with most Windows 10/11 PCs). Open the download page now?" }
};
static const wchar_t* T(const wchar_t* zh) {
    if (!g_en || !zh) return zh;
    for (auto& p : EN_TABLE) if (wcscmp(p.zh, zh) == 0) return p.en;
    return zh;
}
static const wchar_t* kClass    = L"YtGrabMainWnd";
static const wchar_t* kYtdlpUrl = L"https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";
static const wchar_t* kFfmpegUrl= L"https://github.com/yt-dlp/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip";

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
static std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

static std::wstring Trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return {};
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Quote one argument per the MSVCRT / CommandLineToArgvW rules.
static std::wstring Q(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\"") == std::wstring::npos) return a;
    std::wstring r = L"\"";
    size_t bs = 0;
    for (wchar_t c : a) {
        if (c == L'\\') { ++bs; continue; }
        if (c == L'"') { r.append(bs * 2 + 1, L'\\'); r += L'"'; bs = 0; continue; }
        r.append(bs, L'\\'); bs = 0; r += c;
    }
    r.append(bs * 2, L'\\');
    r += L'"';
    return r;
}

static double Num(const std::wstring& s) {
    if (s.empty() || s == L"NA" || s == L"None") return -1;
    wchar_t* e = nullptr;
    double v = wcstod(s.c_str(), &e);
    return (e == s.c_str()) ? -1 : v;
}

static std::wstring FmtBytes(double b) {
    if (b < 0) return {};
    static const wchar_t* u[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    int i = 0;
    while (b >= 1024 && i < 4) { b /= 1024; ++i; }
    wchar_t s[48];
    _snwprintf(s, 48, i == 0 ? L"%.0f %ls" : L"%.1f %ls", b, u[i]);
    s[47] = 0;
    return s;
}

static std::wstring FmtEta(double sec) {
    if (sec < 0) return {};
    long t = (long)sec;
    wchar_t s[32];
    if (t >= 3600) _snwprintf(s, 32, L"%ld:%02ld:%02ld", t / 3600, (t / 60) % 60, t % 60);
    else           _snwprintf(s, 32, L"%02ld:%02ld", t / 60, t % 60);
    s[31] = 0;
    return s;
}

static bool FileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static bool DirExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    std::wstring p(buf, n);
    return p.substr(0, p.find_last_of(L"\\/"));
}

static std::wstring KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &p))) r = p;
    if (p) CoTaskMemFree(p);
    return r;
}

static std::wstring GetText(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n, L'\0');
    if (n) GetWindowTextW(h, &s[0], n + 1);
    return s;
}

static std::wstring DirOf(const std::wstring& p) {
    size_t k = p.find_last_of(L"\\/");
    return k == std::wstring::npos ? std::wstring() : p.substr(0, k);
}

// ---------------------------------------------------------------------------
// Settings (INI, portable if settings.ini sits next to the exe)
// ---------------------------------------------------------------------------
// Subtitle languages offered in Settings > Subtitle languages (code, 中文, English, yt-dlp --sub-langs patterns)
struct SubLang { const wchar_t *code, *zh, *en, *pat; };
static const SubLang kSubLangs[] = {
    { L"en",      L"英语",     L"English",               L"en.*" },
    { L"zh-Hans", L"简体中文", L"Chinese (Simplified)",  L"zh-Hans.*,zh-CN,zh-SG,zh" },
    { L"zh-Hant", L"繁体中文", L"Chinese (Traditional)", L"zh-Hant.*,zh-TW,zh-HK" },
    { L"ja",      L"日语",     L"Japanese",              L"ja.*" },
    { L"ko",      L"韩语",     L"Korean",                L"ko.*" },
    { L"es",      L"西班牙语", L"Spanish",               L"es.*" },
    { L"fr",      L"法语",     L"French",                L"fr.*" },
    { L"de",      L"德语",     L"German",                L"de.*" },
    { L"ru",      L"俄语",     L"Russian",               L"ru.*" },
    { L"vi",      L"越南语",   L"Vietnamese",            L"vi.*" },
    { L"th",      L"泰语",     L"Thai",                  L"th.*" },
    { L"pt",      L"葡萄牙语", L"Portuguese",            L"pt.*" },
    { L"ar",      L"阿拉伯语", L"Arabic",                L"ar.*" },
};
static const int kSubLangCount = (int)(sizeof(kSubLangs) / sizeof(*kSubLangs));
static bool SubLangOn(const std::wstring& list, const wchar_t* code) {
    std::wstring l = L"," + list + L",", c = std::wstring(L",") + code + L",";
    return l.find(c) != std::wstring::npos;
}

struct Settings {
    std::wstring outDir, proxy, cookies, rate, ytdlpPath;
    int fmt = 0, conc = 3, frag = 4, lang = 0;   // lang: 0 system, 1 中文, 2 English
    bool playlist = false, subs = false, thumb = false;
    std::wstring subLangs = L"en,zh-Hans";   // codes from kSubLangs, comma separated
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT, w = 0, h = 0; bool maximized = false;
};
static Settings g_set;
static std::wstring g_ini;

static void IniPath() {
    std::wstring portable = ExeDir() + L"\\settings.ini";
    if (FileExists(portable)) { g_ini = portable; return; }
    std::wstring dir = KnownFolder(FOLDERID_RoamingAppData) + L"\\YtGrab";
    CreateDirectoryW(dir.c_str(), nullptr);
    g_ini = dir + L"\\settings.ini";
}
static std::wstring IniStr(const wchar_t* k, const wchar_t* def = L"") {
    wchar_t b[2048];
    GetPrivateProfileStringW(L"YtGrab", k, def, b, 2048, g_ini.c_str());
    return b;
}
static int IniInt(const wchar_t* k, int def) { return (int)GetPrivateProfileIntW(L"YtGrab", k, def, g_ini.c_str()); }
static void IniPut(const wchar_t* k, const std::wstring& v) { WritePrivateProfileStringW(L"YtGrab", k, v.c_str(), g_ini.c_str()); }
static void IniPut(const wchar_t* k, int v) { IniPut(k, std::to_wstring(v)); }

static void LoadSettings() {
    IniPath();
    g_set.outDir   = IniStr(L"OutDir");
    if (g_set.outDir.empty()) g_set.outDir = KnownFolder(FOLDERID_Downloads);
    g_set.proxy    = IniStr(L"Proxy");
    g_set.cookies  = IniStr(L"Cookies", L"builtin");
    g_set.rate     = IniStr(L"RateLimit");
    g_set.ytdlpPath= IniStr(L"YtdlpPath");
    g_set.fmt      = IniInt(L"Format", 0);
    g_set.conc     = std::max(1, std::min(10, IniInt(L"Concurrency", 3)));
    g_set.frag     = std::max(1, std::min(16, IniInt(L"Fragments", 4)));
    g_set.playlist = IniInt(L"Playlist", 0) != 0;
    g_set.subs     = IniInt(L"Subtitles", 0) != 0;
    g_set.subLangs = IniStr(L"SubLangs", L"en,zh-Hans");
    if (g_set.subLangs.empty()) g_set.subLangs = L"en,zh-Hans";
    g_set.thumb    = IniInt(L"Thumbnail", 0) != 0;
    g_set.x = IniInt(L"WinX", (int)CW_USEDEFAULT); g_set.y = IniInt(L"WinY", (int)CW_USEDEFAULT);
    g_set.w = IniInt(L"WinW", 0); g_set.h = IniInt(L"WinH", 0);
    g_set.maximized = IniInt(L"WinMax", 0) != 0;
    g_set.lang = std::max(0, std::min(2, IniInt(L"Language", 0)));
    g_en = g_set.lang == 2 || (g_set.lang == 0 && PRIMARYLANGID(GetUserDefaultUILanguage()) != LANG_CHINESE);
}
static void SaveSettings() {
    IniPut(L"OutDir", g_set.outDir);   IniPut(L"Proxy", g_set.proxy);
    IniPut(L"Cookies", g_set.cookies); IniPut(L"RateLimit", g_set.rate);
    IniPut(L"YtdlpPath", g_set.ytdlpPath);
    IniPut(L"Format", g_set.fmt);      IniPut(L"Concurrency", g_set.conc);
    IniPut(L"Fragments", g_set.frag);  IniPut(L"Playlist", g_set.playlist);
    IniPut(L"Subtitles", g_set.subs);  IniPut(L"Thumbnail", g_set.thumb);
    IniPut(L"SubLangs", g_set.subLangs);
    IniPut(L"WinX", g_set.x); IniPut(L"WinY", g_set.y); IniPut(L"WinW", g_set.w); IniPut(L"WinH", g_set.h);
    IniPut(L"WinMax", g_set.maximized);
    IniPut(L"Language", g_set.lang);
}

// ---------------------------------------------------------------------------
// Formats
// ---------------------------------------------------------------------------
struct FormatDef { const wchar_t* name; const wchar_t* withFF; const wchar_t* noFF; };
static const FormatDef g_formats[] = {
    { L"最佳画质（视频 + 音频）", L"-f \"bv*+ba/b\"", L"-f b" },
    { L"2160p (4K)",  L"-f \"bv*[height<=2160]+ba/b[height<=2160]/b\"", L"-f \"b[height<=2160]/b\"" },
    { L"1440p (2K)",  L"-f \"bv*[height<=1440]+ba/b[height<=1440]/b\"", L"-f \"b[height<=1440]/b\"" },
    { L"1080p",       L"-f \"bv*[height<=1080]+ba/b[height<=1080]/b\"", L"-f \"b[height<=1080]/b\"" },
    { L"720p",        L"-f \"bv*[height<=720]+ba/b[height<=720]/b\"",   L"-f \"b[height<=720]/b\"" },
    { L"480p",        L"-f \"bv*[height<=480]+ba/b[height<=480]/b\"",   L"-f \"b[height<=480]/b\"" },
    { L"MP4 兼容（H.264，≤1080p）", L"-S \"vcodec:h264,res:1080,acodec:aac\" --merge-output-format mp4",
                                     L"-f \"b[ext=mp4][height<=1080]/b[height<=1080]/b\"" },
    { L"仅音频 · MP3",  L"-f \"ba/b\" -x --audio-format mp3 --audio-quality 0", L"-f \"ba[ext=m4a]/ba/b\"" },
    { L"仅音频 · M4A",  L"-f \"ba[ext=m4a]/ba/b\" -x --audio-format m4a",       L"-f \"ba[ext=m4a]/ba/b\"" },
    { L"仅音频 · 原始格式", L"-f \"ba/b\"", L"-f \"ba/b\"" },
};
static const int kFormatCount = sizeof(g_formats) / sizeof(g_formats[0]);

// ---------------------------------------------------------------------------
// Tasks
// ---------------------------------------------------------------------------
enum TaskState { TS_EXPANDING, TS_PAUSED, TS_QUEUED, TS_STARTING, TS_DOWNLOADING, TS_PROCESSING, TS_DONE, TS_FAILED, TS_CANCELED };

struct Task {
    int id = 0;
    std::wstring url, outDir;
    int fmt = 0;
    bool playlist = false;
    // mutable, guarded by g_mx
    std::wstring title, filepath, err;
    TaskState st = TS_QUEUED;
    double pct = 0, total = -1, speed = -1, eta = -1;
    int plIdx = 0, plCount = 0;
    // separate video + audio streams: show one combined 0-100%
    int nParts = 1, part = 0;        // streams to download for the current item / index of the one in progress
    double partSize[2] = { -1, -1 }; // from yt-dlp's format info (may be unknown)
    double doneBytes = 0;            // bytes of streams already finished
    bool part0Audio = false;         // first stream is audio-only (rare format orders)
    std::wstring partId[2];          // format ids, to tell stream progress from subtitle downloads
    bool pause = false;       // stopped by "停止全部": ends as paused, not canceled
    bool expand = false;      // playlist/channel link: list its videos as separate tasks first
    std::wstring mediaId;     // Xiaohongshu note collected by the built-in browser: direct media links
    std::vector<std::wstring> media;
    std::wstring ckTmp;       // per-run copy of the built-in browser's cookies
    HANDLE job = nullptr;
    bool cancel = false;
    bool dirty = true;
};

static std::vector<std::shared_ptr<Task>> g_tasks;
static std::mutex g_mx;
static int g_nextId = 1;

static bool IsActive(TaskState s) { return s == TS_STARTING || s == TS_DOWNLOADING || s == TS_PROCESSING; }
static void DirectWorker(std::shared_ptr<Task> t, HWND hwnd);
static std::wstring CookieArgs(std::wstring* tmpOut);

// ---------------------------------------------------------------------------
// Globals: UI
// ---------------------------------------------------------------------------
static HINSTANCE g_hInst;
static HWND g_hwnd, g_hUrlLbl, g_hUrl, g_hAdd, g_hFmtLbl, g_hFmt, g_hDirLbl, g_hDir, g_hBrowse,
            g_hStartAll, g_hStopAll, g_hSettings, g_hOpenDir, g_hLogin, g_hList, g_hStatus, g_hBusy, g_hBusyTxt;
static HWND g_hSettingsDlg;
static HFONT g_font;
static UINT g_dpi = 96;
static HTHEME g_progTheme;
static HIMAGELIST g_rowIml;
static ITaskbarList3* g_taskbar;
static UINT WM_TASKBARCREATED;
static std::wstring g_ytdlp, g_ffmpeg, g_ytdlpVer;
static std::wstring g_statusMsg;
static bool g_compBusy = false;
static int g_lastActive = 0;

static const int kColW[] = { 320, 78, 170, 86, 92, 72, 300 };
static const wchar_t* kColName[] = { L"名称", L"状态", L"进度", L"大小", L"速度", L"剩余", L"信息" };
static const int kCols = 7;

static int S(int v) { return MulDiv(v, (int)g_dpi, 96); }

// ---------------------------------------------------------------------------
// Child process with private pipe + kill-on-close job object
// ---------------------------------------------------------------------------
struct Child { HANDLE proc = nullptr, job = nullptr, out = nullptr; };

static bool SpawnChild(const std::wstring& exe, std::wstring cmd, const std::wstring& cwd, Child& c) {
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 1 << 16)) return false;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    // Only let the child inherit *its own* pipe: otherwise concurrently spawned
    // children would hold each other's write ends and EOF would never arrive.
    SIZE_T sz = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &sz);
    std::vector<char> attrBuf(sz);
    auto al = (LPPROC_THREAD_ATTRIBUTE_LIST)attrBuf.data();
    InitializeProcThreadAttributeList(al, 1, 0, &sz);
    HANDLE inherit[2] = { wr, nul };
    UpdateProcThreadAttribute(al, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit,
                              nul != INVALID_HANDLE_VALUE ? sizeof(inherit) : sizeof(HANDLE), nullptr, nullptr);

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.StartupInfo.wShowWindow = SW_HIDE;
    si.StartupInfo.hStdInput = nul != INVALID_HANDLE_VALUE ? nul : nullptr;
    si.StartupInfo.hStdOutput = wr;
    si.StartupInfo.hStdError = wr;
    si.lpAttributeList = al;

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
                             nullptr, cwd.empty() ? nullptr : cwd.c_str(), &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList(al);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) { CloseHandle(rd); return false; }

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof(li));
        AssignProcessToJobObject(job, pi.hProcess);   // ffmpeg etc. spawned by yt-dlp join the job too
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    c.proc = pi.hProcess; c.job = job; c.out = rd;
    return true;
}

static void ReadLines(HANDLE rd, const std::function<void(const std::wstring&)>& cb) {
    std::string acc;
    char buf[16384];
    DWORD n = 0;
    while (ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n) {
        acc.append(buf, n);
        size_t start = 0, p;
        while ((p = acc.find_first_of("\r\n", start)) != std::string::npos) {
            if (p > start) cb(FromUtf8(acc.substr(start, p - start)));
            start = p + 1;
        }
        acc.erase(0, start);
    }
    if (!acc.empty()) cb(FromUtf8(acc));
}

static DWORD RunCapture(const std::wstring& exe, const std::wstring& args, std::wstring& out, const std::wstring& cwd = L"") {
    Child c;
    if (!SpawnChild(exe, Q(exe) + L" " + args, cwd, c)) return (DWORD)-1;
    std::string acc; char buf[8192]; DWORD n = 0;
    while (ReadFile(c.out, buf, sizeof(buf), &n, nullptr) && n) acc.append(buf, n);
    WaitForSingleObject(c.proc, INFINITE);
    DWORD code = 1; GetExitCodeProcess(c.proc, &code);
    CloseHandle(c.out); CloseHandle(c.proc); if (c.job) CloseHandle(c.job);
    out = FromUtf8(acc);
    return code;
}

// ---------------------------------------------------------------------------
// Tool discovery
// ---------------------------------------------------------------------------
static std::wstring LocalBinDir() { return KnownFolder(FOLDERID_LocalAppData) + L"\\YtGrab\\bin"; }

static bool DirWritable(const std::wstring& d) {
    std::wstring f = d + L"\\.ytgrab_write_test";
    HANDLE h = CreateFileW(f.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}

static std::wstring ToolDir() {
    std::wstring e = ExeDir();
    if (DirWritable(e)) return e;
    std::wstring d = LocalBinDir();
    SHCreateDirectoryExW(nullptr, d.c_str(), nullptr);
    return d;
}

static std::wstring FindTool(const wchar_t* name) {
    std::wstring c = ExeDir() + L"\\tools\\" + name;   // bundled with the release
    if (FileExists(c)) return c;
    c = ExeDir() + L"\\" + name;
    if (FileExists(c)) return c;
    c = LocalBinDir() + L"\\" + name;
    if (FileExists(c)) return c;
    wchar_t buf[MAX_PATH];
    if (SearchPathW(nullptr, name, nullptr, MAX_PATH, buf, nullptr)) return buf;
    return {};
}

static void DetectTools() {
    g_ytdlp = (!g_set.ytdlpPath.empty() && FileExists(g_set.ytdlpPath)) ? g_set.ytdlpPath : FindTool(L"yt-dlp.exe");
    g_ffmpeg = FindTool(L"ffmpeg.exe");
    g_ytdlpVer.clear();
    if (!g_ytdlp.empty()) {
        std::wstring exe = g_ytdlp;
        HWND hwnd = g_hwnd;
        std::thread([exe, hwnd] {
            std::wstring out;
            RunCapture(exe, L"--version", out);
            PostMessageW(hwnd, WM_APP_TOOLS, 0, (LPARAM)new std::wstring(Trim(out)));
        }).detach();
    }
}

// ---------------------------------------------------------------------------
// Download worker
// ---------------------------------------------------------------------------
static std::wstring BuildArgs(Task& t) {
    bool ff = !g_ffmpeg.empty();
    std::wstring a;
    a += L"--newline --progress --no-simulate --no-colors --encoding utf-8 --no-mtime --windows-filenames";
    a += L" --print " + Q(L"before_dl:[T]%(title)s");
    a += L" --print " + Q(L"before_dl:[S]%(requested_formats.0.filesize,requested_formats.0.filesize_approx)s;"
                         L"%(requested_formats.1.filesize,requested_formats.1.filesize_approx)s;%(requested_formats.0.vcodec)s;"
                         L"%(requested_formats.0.format_id)s;%(requested_formats.1.format_id)s");
    a += L" --print " + Q(L"after_move:[F]%(filepath)s");
    a += L" --progress-template " + Q(L"download:[P]%(progress.downloaded_bytes)s|%(progress.total_bytes)s|"
                                      L"%(progress.total_bytes_estimate)s|%(progress.speed)s|%(progress.eta)s|"
                                      L"%(info.playlist_index)s|%(info.n_entries)s|%(progress.status)s|%(info.format_id)s");
    a += L" -P " + Q(t.outDir);
    a += L" -o " + Q(L"%(title).180B [%(id)s].%(ext)s");
    int f = (t.fmt >= 0 && t.fmt < kFormatCount) ? t.fmt : 0;
    a += L" ";
    a += ff ? g_formats[f].withFF : g_formats[f].noFF;
    a += t.playlist ? L" --yes-playlist" : L" --no-playlist";
    a += L" -N " + std::to_wstring(g_set.frag);
    if (ff) a += L" --ffmpeg-location " + Q(DirOf(g_ffmpeg));
    if (!g_set.proxy.empty())   a += L" --proxy " + Q(g_set.proxy);
    a += CookieArgs(&t.ckTmp);
    if (!g_set.rate.empty())    a += L" -r " + Q(g_set.rate);
    if (g_set.subs) {
        std::wstring pats;
        for (int i = 0; i < kSubLangCount; ++i)
            if (SubLangOn(g_set.subLangs, kSubLangs[i].code)) pats += (pats.empty() ? L"" : L",") + std::wstring(kSubLangs[i].pat);
        if (pats.empty()) pats = L"en.*,zh-Hans.*,zh-CN,zh";
        a += L" --write-subs --sub-langs " + Q(pats);
        if (ff) a += L" --embed-subs";
    }
    if (g_set.thumb && ff) a += L" --embed-thumbnail";
    a += L" -- " + Q(t.url);
    return a;
}

static void HandleLine(Task& t, const std::wstring& line) {
    std::lock_guard<std::mutex> lk(g_mx);
    if (line.compare(0, 3, L"[P]") == 0) {
        std::vector<std::wstring> f;
        size_t s = 3, p;
        while ((p = line.find(L'|', s)) != std::wstring::npos) { f.push_back(line.substr(s, p - s)); s = p + 1; }
        f.push_back(line.substr(s));
        if (f.size() < 8) return;
        double dl = Num(f[0]), tot = Num(f[1]);
        if (tot <= 0) tot = Num(f[2]);
        if (t.nParts == 2) {
            std::wstring id = f.size() > 8 ? f[8] : L"";
            if (id == t.partId[0]) t.part = 0;
            else if (id == t.partId[1]) t.part = 1;
            else return;   // subtitles etc.: keep the stream progress as it is
            t.doneBytes = t.part == 1 ? (t.partSize[0] > 0 ? t.partSize[0] : 0) : 0;
        }
        int pi = std::min(t.part, 1);
        if (tot > 0 && t.nParts == 2) t.partSize[pi] = tot;   // the live figure beats the estimate
        if (t.nParts == 2) {
            // combined size: known parts, unknown audio guessed at 10% of video, unknown video at 9x audio
            double s0 = t.partSize[0], s1 = t.partSize[1];
            bool a0 = t.part0Audio;
            double v = a0 ? s1 : s0, au = a0 ? s0 : s1;
            if (v <= 0 && au > 0) v = au * 9;
            if (au <= 0 && v > 0) au = v / 9;
            double all = v + au;
            if (all > 0 && dl >= 0) t.pct = std::min(99.0, (t.doneBytes + dl) * 100.0 / all);
            if (all > 0) t.total = all;
        } else {
            if (tot > 0 && dl >= 0) t.pct = std::min(100.0, dl * 100.0 / tot);
            if (tot > 0) t.total = tot;
        }
        t.speed = Num(f[3]);
        t.eta = Num(f[4]);
        double idx = Num(f[5]), cnt = Num(f[6]);
        if (idx > 0 && cnt > 0) { t.plIdx = (int)idx; t.plCount = (int)cnt; }
        if (f[7] == L"finished") {
            if (t.nParts == 2 && t.part == 0) { t.speed = -1; t.eta = -1; }   // audio comes next
            else { t.pct = t.nParts == 2 ? 99 : 100; t.st = TS_PROCESSING; t.speed = -1; t.eta = -1; }
        }
        else if (!t.cancel) t.st = TS_DOWNLOADING;
        t.dirty = true;
    } else if (line.compare(0, 3, L"[T]") == 0) {
        t.title = line.substr(3);
        t.pct = 0; t.total = -1;
        t.nParts = 1; t.part = 0; t.doneBytes = 0; t.partSize[0] = t.partSize[1] = -1; t.part0Audio = false;
        t.dirty = true;
    } else if (line.compare(0, 3, L"[S]") == 0) {
        // "<size0>;<size1>;<vcodec0>": two entries = separate video and audio downloads
        std::vector<std::wstring> g;
        { std::wstring r = line.substr(3); size_t s0 = 0, q;
          while ((q = r.find(L';', s0)) != std::wstring::npos) { g.push_back(r.substr(s0, q - s0)); s0 = q + 1; }
          g.push_back(r.substr(s0)); }
        if (g.size() >= 5) {
            if (g[2] != L"NA" && g[4] != L"NA") {
                t.nParts = 2;
                t.partSize[0] = Num(g[0]);
                t.partSize[1] = Num(g[1]);
                t.part0Audio = g[2] == L"none";
                t.partId[0] = g[3]; t.partId[1] = g[4];
                double all = (t.partSize[0] > 0 ? t.partSize[0] : 0) + (t.partSize[1] > 0 ? t.partSize[1] : 0);
                if (t.partSize[0] > 0 && t.partSize[1] > 0) t.total = all;
            }
        }
        t.dirty = true;
    } else if (line.compare(0, 3, L"[F]") == 0) {
        t.filepath = line.substr(3);
        t.dirty = true;
    } else if (line.compare(0, 6, L"ERROR:") == 0) {
        t.err = Trim(line.substr(6));
        t.dirty = true;
    }
}

static void Worker(std::shared_ptr<Task> t, std::wstring exe, std::wstring args, HWND hwnd) {
    SHCreateDirectoryExW(nullptr, t->outDir.c_str(), nullptr);
    Child c;
    if (!SpawnChild(exe, Q(exe) + L" " + args, t->outDir, c)) {
        std::lock_guard<std::mutex> lk(g_mx);
        t->st = TS_FAILED; t->err = T(L"无法启动 yt-dlp：") + exe; t->dirty = true;
        PostMessageW(hwnd, WM_APP_DONE, 0, 0);
        return;
    }
    {
        std::lock_guard<std::mutex> lk(g_mx);
        t->job = c.job;
        if (t->cancel && c.job) TerminateJobObject(c.job, 1);
    }
    ReadLines(c.out, [&](const std::wstring& l) { HandleLine(*t, l); });
    WaitForSingleObject(c.proc, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(c.proc, &code);
    {
        std::lock_guard<std::mutex> lk(g_mx);
        t->job = nullptr;
        if (t->cancel) { t->st = t->pause ? TS_PAUSED : TS_CANCELED; t->pause = false; }
        else if (code == 0) { t->st = TS_DONE; t->pct = 100; }
        else {
            t->st = TS_FAILED;
            if (t->err.empty()) t->err = T(L"yt-dlp 退出代码 ") + std::to_wstring((int)code);
            std::wstring l = t->err;
            for (auto& ch : l) ch = towlower(ch);
            for (auto k : { L"sign in", L"log in", L"login", L"cookies", L"logged-in", L"members-only", L"private", L"not a bot", L"confirm you" })
                if (l.find(k) != std::wstring::npos) { t->err = T(L"需要登录：右键选「在内置浏览器中登录」，登录后重新下载。 ") + t->err; break; }
        }
        t->speed = -1; t->eta = -1; t->dirty = true;
        if (!t->ckTmp.empty()) { DeleteFileW(t->ckTmp.c_str()); t->ckTmp.clear(); }
    }
    if (c.job) CloseHandle(c.job);
    CloseHandle(c.proc);
    CloseHandle(c.out);
    PostMessageW(hwnd, WM_APP_DONE, 0, 0);
}

static void Pump() {
    bool noYt = g_ytdlp.empty();
    std::vector<std::shared_ptr<Task>> start;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        int running = 0;
        for (auto& t : g_tasks) if (IsActive(t->st)) ++running;
        for (auto& t : g_tasks) {
            if (running >= g_set.conc) break;
            if (t->st == TS_QUEUED && (!noYt || !t->media.empty())) {
                t->st = TS_STARTING; t->cancel = false; t->err.clear(); t->dirty = true;
                start.push_back(t); ++running;
            }
        }
    }
    for (auto& t : start) {
        if (!t->media.empty()) std::thread(DirectWorker, t, g_hwnd).detach();
        else std::thread(Worker, t, g_ytdlp, BuildArgs(*t), g_hwnd).detach();
    }
}

// ---------------------------------------------------------------------------
// Playlist / channel expansion: one task per video
// ---------------------------------------------------------------------------
static std::wstring Lower(std::wstring s) { for (auto& c : s) c = towlower(c); return s; }

static bool IsYouTube(const std::wstring& u) {
    std::wstring l = Lower(u);
    return l.find(L"youtube.com/") != std::wstring::npos || l.find(L"youtu.be/") != std::wstring::npos;
}

static bool IsYouTubeVideo(const std::wstring& u) {
    std::wstring l = Lower(u);
    return l.find(L"youtu.be/") != std::wstring::npos || l.find(L"/watch?") != std::wstring::npos ||
           l.find(L"/shorts/") != std::wstring::npos || l.find(L"/live/") != std::wstring::npos ||
           l.find(L"/embed/") != std::wstring::npos;
}

static bool Has(const std::wstring& l, const wchar_t* s) { return l.find(s) != std::wstring::npos; }

// A creator's home page (YouTube channel, Bilibili space, TikTok/Douyin/Instagram/Kuaishou/Xiaohongshu
// profile). Its videos are saved in a folder named after the creator.
static bool IsProfile(const std::wstring& u) {
    std::wstring l = Lower(u);
    if (IsYouTube(u))
        return !IsYouTubeVideo(u) && !Has(l, L"list=") && !Has(l, L"/playlist") &&
               (Has(l, L"/@") || Has(l, L"/channel/") || Has(l, L"/c/") || Has(l, L"/user/"));
    if (Has(l, L"space.bilibili.com/"))
        return !Has(l, L"/lists") && !Has(l, L"/favlist") && !Has(l, L"/channel/");
    if (Has(l, L"tiktok.com/@")) return !Has(l, L"/video/") && !Has(l, L"/photo/");
    if (Has(l, L"douyin.com/user/")) return !Has(l, L"modal_id=");
    if (Has(l, L"kuaishou.com/profile/") || Has(l, L"xiaohongshu.com/user/profile/")) return true;
    size_t ig = l.find(L"instagram.com/");
    if (ig != std::wstring::npos) {
        std::wstring path = l.substr(ig + 14);
        size_t q = path.find_first_of(L"?#"); if (q != std::wstring::npos) path.resize(q);
        while (!path.empty() && path.back() == L'/') path.pop_back();
        static const wchar_t* notUser[] = { L"p", L"reel", L"reels", L"tv", L"stories", L"explore", L"accounts" };
        if (path.empty() || path.find(L'/') != std::wstring::npos) return false;
        for (auto n : notUser) if (path == n) return false;
        return true;
    }
    return false;
}

// Links that hold several videos: playlists, channels, channel tabs, list= on any site.
static bool LooksLikeList(const std::wstring& u) {
    if (IsProfile(u)) return true;
    std::wstring l = Lower(u);
    if (l.find(L"list=") != std::wstring::npos || l.find(L"/playlist") != std::wstring::npos) return true;
    if (IsYouTube(u) && !IsYouTubeVideo(u)) return true;   // @handle, /channel/, /c/, /user/, tabs
    if (l.find(L"bilibili.com") != std::wstring::npos &&
        (l.find(L"/lists/") != std::wstring::npos || l.find(L"/favlist") != std::wstring::npos ||
         l.find(L"/channel/") != std::wstring::npos || l.find(L"space.bilibili.com") != std::wstring::npos)) return true;
    return false;
}

static std::wstring SafeFolder(std::wstring n) {
    for (auto& c : n) if (c < 32 || wcschr(L"<>:\"/\\|?*", c)) c = L'_';
    n = Trim(n);
    while (!n.empty() && (n.back() == L'.' || n.back() == L' ')) n.pop_back();
    if (n.size() > 100) n.resize(100);
    return n;
}

struct Entry { std::wstring url, title, list, owner; };

// Decodes a value printed with yt-dlp's %(...)j (a JSON string); newlines become spaces.
static std::wstring JsonStr(const std::wstring& j) {
    if (j.size() < 2 || j.front() != L'"' || j.back() != L'"') return {};   // null / NA
    std::wstring r;
    for (size_t i = 1; i + 1 < j.size(); ++i) {
        wchar_t c = j[i];
        if (c != L'\\' || i + 2 >= j.size()) { r += c; continue; }
        wchar_t e = j[++i];
        switch (e) {
        case L'n': case L'r': case L't': r += L' '; break;
        case L'b': case L'f': break;
        case L'u':
            if (i + 4 < j.size()) { r += (wchar_t)wcstoul(j.substr(i + 1, 4).c_str(), nullptr, 16); i += 4; }
            break;
        default: r += e; break;
        }
    }
    std::wstring o; bool sp = false;               // collapse runs of spaces
    for (wchar_t c : r) { if (c == L' ') { if (!sp) o += c; sp = true; } else { o += c; sp = false; } }
    return Trim(o);
}

static std::wstring CommonArgs(std::wstring* ckTmp) {
    std::wstring a;
    if (!g_set.proxy.empty())   a += L" --proxy " + Q(g_set.proxy);
    a += CookieArgs(ckTmp);
    return a;
}

// Lists the entries of a playlist without downloading. Nested lists (a channel's
// Playlists tab, a channel's Videos/Shorts/Live tabs) are opened one level deeper.
static bool ListEntries(const std::wstring& exe, const std::wstring& url, int depth,
                        const std::shared_ptr<Task>& t, std::vector<Entry>& out, std::wstring& err) {
    std::wstring ckTmp;
    std::wstring args = L"--flat-playlist --yes-playlist --no-colors --encoding utf-8 --no-warnings"
                        L" --print " + Q(L"[E]%(url)s\t%(title)j\t%(playlist_title)j\t%(playlist_uploader,playlist_channel,channel,uploader)j") + CommonArgs(&ckTmp) + L" -- " + Q(url);
    std::wstring text;
    DWORD code = RunCapture(exe, args, text);
    if (!ckTmp.empty()) DeleteFileW(ckTmp.c_str());
    std::vector<Entry> found;
    size_t s = 0;
    while (s < text.size()) {
        size_t e = text.find_first_of(L"\r\n", s);
        if (e == std::wstring::npos) e = text.size();
        std::wstring line = text.substr(s, e - s);
        s = e + 1;
        if (line.compare(0, 3, L"[E]") == 0) {
            Entry en; size_t a = 3, b = line.find(L'\t', a);
            en.url = line.substr(a, b == std::wstring::npos ? std::wstring::npos : b - a);
            if (b != std::wstring::npos) {
                size_t c = line.find(L'\t', b + 1);
                en.title = line.substr(b + 1, c == std::wstring::npos ? std::wstring::npos : c - b - 1);
                if (c != std::wstring::npos) {
                    size_t d = line.find(L'\t', c + 1);
                    en.list = line.substr(c + 1, d == std::wstring::npos ? std::wstring::npos : d - c - 1);
                    if (d != std::wstring::npos) en.owner = line.substr(d + 1);
                }
            }
            en.title = JsonStr(en.title);
            en.list = JsonStr(en.list);
            en.owner = JsonStr(en.owner);
            if (en.url.compare(0, 4, L"http") == 0) found.push_back(en);
        } else if (line.compare(0, 6, L"ERROR:") == 0 && err.empty()) {
            err = Trim(line.substr(6));
        }
    }
    if (found.empty()) {
        if (err.empty()) err = code == 0 ? T(L"列表里没有视频") : T(L"yt-dlp 退出代码 ") + std::to_wstring((int)code);
        return false;
    }
    for (auto& en : found) {
        { std::lock_guard<std::mutex> lk(g_mx); if (t->cancel) return false; }
        if (depth < 1 && IsYouTube(en.url) && !IsYouTubeVideo(en.url)) {
            std::wstring sub;
            if (!ListEntries(exe, en.url, depth + 1, t, out, sub)) continue;   // empty tab etc.
        } else {
            out.push_back(en);
        }
    }
    return true;
}

// Links being read are kept out of the list; the status bar shows how many are pending.
static std::vector<std::shared_ptr<Task>> g_pending;   // guarded by g_mx

static void Expander(std::shared_ptr<Task> t, std::wstring exe, HWND hwnd) {
    std::vector<Entry> v; std::wstring err;
    bool ok = ListEntries(exe, t->url, 0, t, v, err);
    int added = 0;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        auto it = std::find(g_pending.begin(), g_pending.end(), t);
        if (it == g_pending.end() || t->cancel) { PostMessageW(hwnd, WM_APP_EXPANDED, 0, 0); return; }
        g_pending.erase(it);
        t->title.clear(); t->expand = false; t->st = TS_QUEUED; t->dirty = true;
        if (!ok || v.empty() || (v.size() == 1 && !IsYouTube(t->url))) {
            // Could not list it (or it is a single video): download the link as is, yt-dlp reports any error.
            g_tasks.push_back(t);
            added = 1;
        } else {
            // A creator's home page: everything goes into <save folder>\<creator>.
            std::wstring owner;
            if (IsProfile(t->url)) for (auto& e : v) if (!(owner = SafeFolder(e.owner)).empty()) break;
            if (!owner.empty()) {
                std::wstring dir = t->outDir + L"\\" + owner;
                int n = (int)v.size(), i = 0;
                for (auto& e : v) {
                    auto k = std::make_shared<Task>();
                    k->id = g_nextId++;
                    k->url = e.url; k->title = e.title; k->outDir = dir; k->fmt = t->fmt;
                    k->playlist = false; k->plIdx = ++i; k->plCount = n;
                    g_tasks.push_back(k);
                    ++added;
                }
                PostMessageW(hwnd, WM_APP_EXPANDED, 0, added);
                return;
            }
            std::vector<std::wstring> lists;
            for (auto& e : v) if (std::find(lists.begin(), lists.end(), e.list) == lists.end()) lists.push_back(e.list);
            for (auto& l : lists) {
                int n = 0; for (auto& e : v) if (e.list == l) ++n;
                int i = 0;
                std::wstring dir = t->outDir;
                std::wstring f = SafeFolder(l);
                if (v.size() > 1 && !f.empty()) dir += L"\\" + f;
                for (auto& e : v) {
                    if (e.list != l) continue;
                    auto k = std::make_shared<Task>();
                    k->id = g_nextId++;
                    k->url = e.url; k->title = e.title; k->outDir = dir; k->fmt = t->fmt;
                    k->playlist = false; k->plIdx = ++i; k->plCount = n;
                    g_tasks.push_back(k);
                    ++added;
                }
            }
        }
    }
    PostMessageW(hwnd, WM_APP_EXPANDED, 0, added);
}

static void StartExpand(const std::shared_ptr<Task>& t) {   // caller holds g_mx
    t->st = TS_EXPANDING; t->cancel = false; t->err.clear();
    g_pending.push_back(t);
    std::thread(Expander, t, g_ytdlp, g_hwnd).detach();
}

// ---------------------------------------------------------------------------
// Task list persistence (tasks.txt next to settings.ini): survives restarts
// ---------------------------------------------------------------------------
static std::string ToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static std::wstring Esc(const std::wstring& v) {
    std::wstring r;
    for (wchar_t c : v) {
        if (c == L'\\') r += L"\\\\"; else if (c == L'\t') r += L"\\t";
        else if (c == L'\n') r += L"\\n"; else if (c == L'\r') r += L"\\r"; else r += c;
    }
    return r;
}
static std::wstring Unesc(const std::wstring& v) {
    std::wstring r;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] != L'\\' || i + 1 >= v.size()) { r += v[i]; continue; }
        wchar_t e = v[++i];
        r += e == L't' ? L'\t' : e == L'n' ? L'\n' : e == L'r' ? L'\r' : e;
    }
    return r;
}
static std::wstring TasksFile() { return DirOf(g_ini) + L"\\tasks.txt"; }
static bool g_saveDue = false;
static DWORD g_lastSave = 0;

static void SaveTasks() {
    std::wstring out = L"YtGrabTasks 1\n";
    {
        std::lock_guard<std::mutex> lk(g_mx);
        auto line = [&](wchar_t kind, const Task& t) {
            out += kind;
            for (auto& f : { t.url, t.outDir, std::to_wstring(t.fmt), std::to_wstring((int)t.playlist), t.title,
                             t.filepath, t.err, std::to_wstring((long long)t.total), std::to_wstring(t.plIdx),
                             std::to_wstring(t.plCount), t.mediaId })
                out += L"\t" + Esc(f);
            std::wstring m;
            for (auto& u : t.media) m += (m.empty() ? L"" : L" ") + u;
            out += L"\t" + Esc(m);
            out += L"\n";
        };
        for (auto& t : g_tasks) {
            // D done, F failed, C canceled, Q anything unfinished (resumes on next start)
            wchar_t k = t->st == TS_DONE ? L'D' : t->st == TS_FAILED ? L'F' : t->st == TS_CANCELED ? L'C' : L'Q';
            line(k, *t);
        }
        for (auto& t : g_pending) line(L'L', *t);          // links still being read
    }
    std::wstring tmp = TasksFile() + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    std::string u = ToUtf8(out);
    DWORD w = 0;
    BOOL ok = WriteFile(h, u.data(), (DWORD)u.size(), &w, nullptr);
    CloseHandle(h);
    if (ok && w == u.size()) MoveFileExW(tmp.c_str(), TasksFile().c_str(), MOVEFILE_REPLACE_EXISTING);
    else DeleteFileW(tmp.c_str());
    g_saveDue = false; g_lastSave = GetTickCount();
}

static void LoadTasks() {
    HANDLE h = CreateFileW(TasksFile().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    std::string raw; char buf[65536]; DWORD n = 0;
    while (ReadFile(h, buf, sizeof(buf), &n, nullptr) && n) raw.append(buf, n);
    CloseHandle(h);
    std::wstring text = FromUtf8(raw);
    std::lock_guard<std::mutex> lk(g_mx);
    size_t s = 0;
    while (s < text.size()) {
        size_t e = text.find(L'\n', s);
        if (e == std::wstring::npos) e = text.size();
        std::wstring ln = text.substr(s, e - s);
        s = e + 1;
        if (ln.empty() || !wcschr(L"DFCQL", ln[0]) || ln.size() < 2 || ln[1] != L'\t') continue;
        std::vector<std::wstring> f;
        size_t a = 2, b;
        while ((b = ln.find(L'\t', a)) != std::wstring::npos) { f.push_back(Unesc(ln.substr(a, b - a))); a = b + 1; }
        f.push_back(Unesc(ln.substr(a)));
        if (f.size() < 10 || f[0].empty()) continue;
        auto t = std::make_shared<Task>();
        t->id = g_nextId++;
        t->url = f[0]; t->outDir = f[1]; t->fmt = _wtoi(f[2].c_str()); t->playlist = f[3] == L"1";
        t->title = f[4]; t->filepath = f[5]; t->err = f[6]; t->total = _wtof(f[7].c_str());
        if (t->total <= 0) t->total = -1;
        t->plIdx = _wtoi(f[8].c_str()); t->plCount = _wtoi(f[9].c_str());
        if (f.size() >= 12 && !f[10].empty() && !f[11].empty()) {
            t->mediaId = f[10];
            size_t a2 = 0, b2;
            while ((b2 = f[11].find(L' ', a2)) != std::wstring::npos) { t->media.push_back(f[11].substr(a2, b2 - a2)); a2 = b2 + 1; }
            t->media.push_back(f[11].substr(a2));
        }
        switch (ln[0]) {
        case L'D': t->st = TS_DONE; t->pct = 100; break;
        case L'F': t->st = TS_FAILED; break;
        case L'C': t->st = TS_CANCELED; break;
        case L'L':                       // a list that was still being read: download it whole
            t->playlist = true; t->st = TS_PAUSED; break;
        default: t->st = TS_PAUSED; break;   // never auto-start on launch; "开始全部" resumes
        }
        g_tasks.push_back(t);
    }
}

// ---------------------------------------------------------------------------
// Component download (yt-dlp.exe / ffmpeg)
// ---------------------------------------------------------------------------
static void PostComp(int kind, const std::wstring& msg) {
    PostMessageW(g_hwnd, WM_APP_COMP, kind, (LPARAM)new std::wstring(msg));
}

class DlCallback : public IBindStatusCallback {
    const wchar_t* what_; DWORD last_ = 0;
public:
    explicit DlCallback(const wchar_t* what) : what_(what) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IBindStatusCallback) { *ppv = this; return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return 2; }
    STDMETHODIMP_(ULONG) Release() override { return 1; }
    STDMETHODIMP OnStartBinding(DWORD, IBinding*) override { return S_OK; }
    STDMETHODIMP GetPriority(LONG*) override { return E_NOTIMPL; }
    STDMETHODIMP OnLowResource(DWORD) override { return S_OK; }
    STDMETHODIMP OnProgress(ULONG cur, ULONG max, ULONG, LPCWSTR) override {
        DWORD now = GetTickCount();
        if (now - last_ > 250) {
            last_ = now;
            std::wstring m = std::wstring(L"正在下载 ") + what_ + L"：" + FmtBytes(cur);
            if (max) m += L" / " + FmtBytes(max);
            PostComp(0, m);
        }
        return S_OK;
    }
    STDMETHODIMP OnStopBinding(HRESULT, LPCWSTR) override { return S_OK; }
    STDMETHODIMP GetBindInfo(DWORD*, BINDINFO*) override { return E_NOTIMPL; }
    STDMETHODIMP OnDataAvailable(DWORD, DWORD, FORMATETC*, STGMEDIUM*) override { return S_OK; }
    STDMETHODIMP OnObjectAvailable(REFIID, IUnknown*) override { return S_OK; }
};

static bool Fetch(const wchar_t* url, const std::wstring& dst, const wchar_t* what) {
    DeleteUrlCacheEntryW(url);
    DlCallback cb(what);
    DeleteFileW(dst.c_str());
    return SUCCEEDED(URLDownloadToFileW(nullptr, url, dst.c_str(), 0, &cb));
}

static void DeleteTree(const std::wstring& dir) {
    std::wstring from = dir; from.push_back(L'\0');
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE; op.pFrom = from.c_str();
    op.fFlags = FOF_NO_UI;
    SHFileOperationW(&op);
}

static void ComponentThread(int which) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::wstring dir = ToolDir();
    if (which == ID_GET_YTDLP) {
        std::wstring dst = dir + L"\\yt-dlp.exe", part = dst + L".part";
        if (Fetch(kYtdlpUrl, part, L"yt-dlp") && MoveFileExW(part.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING))
            PostComp(1, T(L"yt-dlp 已安装到 ") + dir);
        else { DeleteFileW(part.c_str()); PostComp(2, T(L"yt-dlp 下载失败，请检查网络或代理（使用系统代理设置）。")); }
    } else if (which == ID_GET_FFMPEG) {
        wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
        std::wstring zip = std::wstring(tmp) + L"ytgrab-ffmpeg.zip";
        std::wstring ex  = std::wstring(tmp) + L"ytgrab-ffmpeg";
        if (!Fetch(kFfmpegUrl, zip, L"ffmpeg")) { PostComp(2, T(L"ffmpeg 下载失败，请检查网络或代理。")); CoUninitialize(); return; }
        PostComp(0, T(L"正在解压 ffmpeg…"));
        DeleteTree(ex);
        CreateDirectoryW(ex.c_str(), nullptr);
        wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
        std::wstring out;
        DWORD code = RunCapture(std::wstring(sys) + L"\\tar.exe", L"-xf " + Q(zip) + L" -C " + Q(ex), out);
        bool ok = false;
        if (code == 0) {
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW((ex + L"\\*").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
                    std::wstring bin = ex + L"\\" + fd.cFileName + L"\\bin\\";
                    if (FileExists(bin + L"ffmpeg.exe")) {
                        ok = CopyFileW((bin + L"ffmpeg.exe").c_str(), (dir + L"\\ffmpeg.exe").c_str(), FALSE) != 0;
                        CopyFileW((bin + L"ffprobe.exe").c_str(), (dir + L"\\ffprobe.exe").c_str(), FALSE);
                        break;
                    }
                } while (FindNextFileW(h, &fd));
                FindClose(h);
            }
        }
        DeleteFileW(zip.c_str());
        DeleteTree(ex);
        if (ok) PostComp(1, T(L"ffmpeg 已安装到 ") + dir);
        else    PostComp(2, T(L"ffmpeg 解压失败（需要 Windows 10 1803 及以上自带的 tar）。"));
    } else if (which == ID_UPDATE_YTDLP) {
        if (g_ytdlp.empty()) { CoUninitialize(); ComponentThread(ID_GET_YTDLP); return; }
        PostComp(0, T(L"正在更新 yt-dlp…"));
        std::wstring out;
        DWORD code = RunCapture(g_ytdlp, L"-U", out);
        out = Trim(out);
        size_t k = out.find_last_of(L"\n");
        std::wstring last = k == std::wstring::npos ? out : Trim(out.substr(k + 1));
        PostComp(code == 0 ? 1 : 2, last.empty() ? T(L"yt-dlp 更新完成") : last);
    }
    CoUninitialize();
}

static void StartComponent(int which) {
    if (g_compBusy) return;
    g_compBusy = true;
    std::thread(ComponentThread, which).detach();
}

// ---------------------------------------------------------------------------
// UI helpers
// ---------------------------------------------------------------------------
static std::wstring g_sbText[5];
static void FitStatusParts();
static void SetStatusPart(int part, const std::wstring& s) {
    bool refit = part > 0 && part < 5 && g_sbText[part].size() != s.size();
    if (part >= 0 && part < 5) g_sbText[part] = s;
    SendMessageW(g_hStatus, SB_SETTEXTW, part, (LPARAM)s.c_str());
    if (refit) FitStatusParts();
}

static void UpdateToolStatus() {
    SetStatusPart(1, g_ytdlp.empty() ? T(L"yt-dlp：未找到") : (L"yt-dlp " + (g_ytdlpVer.empty() ? std::wstring(L"…") : g_ytdlpVer)));
    SetStatusPart(2, g_ffmpeg.empty() ? T(L"ffmpeg：未找到") : T(L"ffmpeg：就绪"));
}

static void ApplyFont(HWND parent) {
    EnumChildWindows(parent, [](HWND h, LPARAM f) -> BOOL { SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE); return TRUE; }, (LPARAM)g_font);
}

typedef BOOL(WINAPI* PFN_SPIForDpi)(UINT, UINT, PVOID, UINT, UINT);

static void CreateUiFont() {
    if (g_font) DeleteObject(g_font);
    NONCLIENTMETRICSW ncm{}; ncm.cbSize = sizeof(ncm);
    auto spi = (PFN_SPIForDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi");
    if (spi && spi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, g_dpi)) {
        g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    } else {
        g_font = CreateFontW(-MulDiv(9, g_dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                             CLEARTYPE_QUALITY, 0, L"Segoe UI");
    }
}

static void UpdateRowHeight() {
    if (g_rowIml) ImageList_Destroy(g_rowIml);
    g_rowIml = ImageList_Create(1, S(28), ILC_COLOR32, 1, 0);
    ListView_SetImageList(g_hList, g_rowIml, LVSIL_SMALL);
}

static void FitNameColumn();
static void UpdateColumns() {
    for (int i = 0; i < kCols; ++i) ListView_SetColumnWidth(g_hList, i, S(kColW[i]));
    FitNameColumn();
}

// Status bar: parts 1-4 are as wide as their text, part 0 takes what is left
static void FitStatusParts() {
    if (!g_hStatus) return;
    RECT rc; GetClientRect(g_hStatus, &rc);
    int W = rc.right;
    HDC dc = GetDC(g_hStatus);
    HGDIOBJ old = SelectObject(dc, (HFONT)SendMessageW(g_hStatus, WM_GETFONT, 0, 0));
    int w[5] = {};
    for (int i = 1; i < 5; ++i) {
        SIZE sz{}; const std::wstring& t = g_sbText[i];
        GetTextExtentPoint32W(dc, t.c_str(), (int)t.size(), &sz);
        w[i] = std::max(S(60), (int)sz.cx + S(20));
    }
    SelectObject(dc, old); ReleaseDC(g_hStatus, dc);
    w[4] += GetSystemMetrics(SM_CXVSCROLL);   // size grip
    int parts[5]; int x = W;
    parts[4] = -1;
    for (int i = 4; i >= 1; --i) { x -= w[i]; parts[i - 1] = std::max(x, S(40)); }
    SendMessageW(g_hStatus, SB_SETPARTS, 5, (LPARAM)parts);
}

// Name column takes the width the other columns leave
static void FitNameColumn() {
    if (!g_hList) return;
    RECT rc; GetClientRect(g_hList, &rc);
    int others = 0;
    for (int i = 1; i < kCols; ++i) others += ListView_GetColumnWidth(g_hList, i);
    int w = std::max(S(160), (int)rc.right - others);
    if (ListView_GetColumnWidth(g_hList, 0) != w) ListView_SetColumnWidth(g_hList, 0, w);
}

static void Layout() {
    if (!g_hList) return;
    RECT rc; GetClientRect(g_hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    SendMessageW(g_hStatus, WM_SIZE, 0, 0);
    RECT sr; GetWindowRect(g_hStatus, &sr);
    int sh = sr.bottom - sr.top;
    FitStatusParts();

    RECT cr; GetWindowRect(g_hFmt, &cr);
    int ch = cr.bottom - cr.top;                 // closed combo height drives row height
    int m = S(14), g = S(8), bw = S(112), bh = std::max(ch + S(2), S(30)), lh = S(20);
    HDWP dw = BeginDeferWindowPos(19);
    auto mv = [&](HWND h, int x, int y, int w, int hh) { dw = DeferWindowPos(dw, h, nullptr, x, y, w, hh, SWP_NOZORDER | SWP_NOACTIVATE); };

    int y = m;
    mv(g_hUrlLbl, m, y, W - 2 * m, lh);
    y += lh + S(4);
    int eh = bh * 2 + g;
    mv(g_hUrl, m, y, W - 2 * m, eh);
    y += eh + S(12);

    int lblW = S(44);
    int fmtW = S(230);
    int cy = y + (bh - ch) / 2;
    int ly = y + (bh - lh) / 2 + S(1);
    mv(g_hFmtLbl, m, ly, lblW, lh);
    mv(g_hFmt, m + lblW, cy, fmtW, S(320));
    int x2 = m + lblW + fmtW + S(18);
    mv(g_hDirLbl, x2, ly, S(56), lh);
    int dx = x2 + S(56);
    mv(g_hDir, dx, cy, std::max(S(60), W - m - bw - g - dx), ch);
    mv(g_hBrowse, W - m - bw, y, bw, bh);
    y += bh + S(10);

    int aw = S(150);
    mv(g_hAdd, m, y, aw, bh);
    int px = m + aw + S(14);
    int sw = S(100);
    mv(g_hStartAll, px, y, sw, bh);
    mv(g_hStopAll, px + sw + g, y, sw, bh);
    {
        int bx = px + 2 * sw + g + S(14), bwid = S(90), bhh = S(8);
        int tx = bx + bwid + S(10);
        mv(g_hBusy, bx, y + (bh - bhh) / 2, bwid, bhh);
        mv(g_hBusyTxt, tx, y + (bh - lh) / 2, std::max(S(10), W - m - 3 * bw - 3 * g - tx), lh);
    }
    mv(g_hSettings, W - m - bw, y, bw, bh);
    mv(g_hOpenDir, W - m - 2 * bw - g, y, bw, bh);
    mv(g_hLogin, W - m - 3 * bw - 2 * g, y, bw, bh);
    y += bh + S(12);

    mv(g_hList, m, y, W - 2 * m, std::max(S(40), H - sh - y - S(10)));
    EndDeferWindowPos(dw);
    FitNameColumn();
}

static std::vector<std::wstring> ExtractUrls(const std::wstring& s) {
    std::vector<std::wstring> r;
    size_t i = 0;
    for (;;) {
        size_t a = s.find(L"http", i);
        if (a == std::wstring::npos) break;
        size_t b = a;
        while (b < s.size()) {
            wchar_t c = s[b];
            if (c <= 32 || c > 126 || c == L'"' || c == L'<' || c == L'>' || c == L'\'' || c == L'`' || c == L'^' || c == L'{' || c == L'}' || c == L'|' || c == L'\\') break;
            ++b;
        }
        std::wstring u = s.substr(a, b - a);
        while (!u.empty() && wcschr(L".,;:!?)]", u.back())) u.pop_back();
        if ((u.compare(0, 7, L"http://") == 0 && u.size() > 8) || (u.compare(0, 8, L"https://") == 0 && u.size() > 9))
            r.push_back(u);
        i = std::max(b, a + 4);
    }
    return r;
}

static void ReadUiSettings() {
    std::wstring d = Trim(GetText(g_hDir));
    if (!d.empty()) g_set.outDir = d;
    int f = (int)SendMessageW(g_hFmt, CB_GETCURSEL, 0, 0);
    if (f >= 0) g_set.fmt = f;
}

static int TaskCount() { std::lock_guard<std::mutex> lk(g_mx); return (int)g_tasks.size(); }

static bool g_saveDue2 = false;
static void RefreshListCount() {
    g_saveDue2 = true;
    ListView_SetItemCountEx(g_hList, TaskCount(), LVSICF_NOSCROLL);
    InvalidateRect(g_hList, nullptr, FALSE);
}

static bool Ask(HWND owner, const std::wstring& head, const std::wstring& body,
                const std::wstring& ok, const std::wstring& cancel, bool danger);
static void PromptYtdlp() {
    if (g_compBusy) return;
    if (Ask(g_hwnd, T(L"没有找到 yt-dlp"),
            T(L"要现在从 GitHub 下载最新版吗（约 18 MB）？也可以手动把 yt-dlp.exe 放在本程序同一目录下。"),
            T(L"立即下载"), T(L"取消"), false))
        StartComponent(ID_GET_YTDLP);
}

// A single video link that also carries a playlist (YouTube watch?v=…&list=…, youtu.be/…?list=…).
static bool IsVideoInList(const std::wstring& u) {
    std::wstring l = Lower(u);
    return IsYouTubeVideo(u) && l.find(L"list=") != std::wstring::npos;
}

enum { ID_PL_ONE = 101, ID_PL_ALL = 102 };
// Shared look for our silent panels (no system sound, no title bar).
static HFONT MakeHeadFont(HWND d) {
    LOGFONTW lf{};
    GetObjectW((HFONT)SendMessageW(d, WM_GETFONT, 0, 0), sizeof(lf), &lf);
    lf.lfWeight = FW_SEMIBOLD; lf.lfHeight = lf.lfHeight * 13 / 10;
    return CreateFontIndirectW(&lf);
}
static void PanelChrome(HWND d, HWND owner) {
    DWORD corner = 2;   // DWMWCP_ROUND (Windows 11)
    DwmSetWindowAttribute(d, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &corner, sizeof(corner));
    RECT rp, rd; GetWindowRect(owner ? owner : g_hwnd, &rp); GetWindowRect(d, &rd);
    SetWindowPos(d, nullptr, rp.left + ((rp.right - rp.left) - (rd.right - rd.left)) / 2,
                 rp.top + ((rp.bottom - rp.top) - (rd.bottom - rd.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

struct MsgArgs { std::wstring head, body, ok, cancel; bool danger; HWND owner; };
static INT_PTR CALLBACK MsgProc(HWND d, UINT msg, WPARAM wp, LPARAM lp) {
    static HFONT head;
    switch (msg) {
    case WM_INITDIALOG: {
        auto* a = (MsgArgs*)lp;
        SetWindowLongPtrW(d, DWLP_USER, lp);
        head = MakeHeadFont(d);
        HWND hH = GetDlgItem(d, IDC_PL_HEAD), hB = GetDlgItem(d, IDC_PL_BODY);
        HWND hOk = GetDlgItem(d, IDOK), hCa = GetDlgItem(d, IDCANCEL);
        SendMessageW(hH, WM_SETFONT, (WPARAM)head, TRUE);
        SetWindowTextW(hH, a->head.c_str()); SetWindowTextW(hB, a->body.c_str());
        SetWindowTextW(hOk, a->ok.c_str());
        // layout in dialog units, then grow the body to fit its text
        RECT u{ 14, 12, 240, 0 }; MapDialogRect(d, &u);   // u.left=margin, u.top=top, u.right=width
        RECT gap{ 0, 0, 8, 20 }; MapDialogRect(d, &gap);
        RECT btn{ 0, 0, 56, 16 }; MapDialogRect(d, &btn);
        int W = u.right, M = u.left, textW = W - 2 * M;
        auto measure = [&](HWND h, HFONT f, const std::wstring& t) {
            if (t.empty()) return 0;
            HDC dc = GetDC(h); HGDIOBJ o = SelectObject(dc, f);
            RECT rc{ 0, 0, textW, 0 };
            DrawTextW(dc, t.c_str(), -1, &rc, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            SelectObject(dc, o); ReleaseDC(h, dc); return (int)rc.bottom;
        };
        HFONT body = (HFONT)SendMessageW(d, WM_GETFONT, 0, 0);
        int y = u.top, hh = measure(hH, head, a->head);
        MoveWindow(hH, M, y, textW, hh, FALSE); y += hh;
        int bh = measure(hB, body, a->body);
        if (bh) { y += gap.right; MoveWindow(hB, M, y, textW, bh, FALSE); y += bh; }
        else ShowWindow(hB, SW_HIDE);
        y += gap.bottom;
        int x = W - M - btn.right;
        if (a->cancel.empty()) { ShowWindow(hCa, SW_HIDE); MoveWindow(hOk, x, y, btn.right, btn.bottom, FALSE); }
        else {
            SetWindowTextW(hCa, a->cancel.c_str());
            MoveWindow(hCa, x, y, btn.right, btn.bottom, FALSE);
            MoveWindow(hOk, x - gap.right - btn.right, y, btn.right, btn.bottom, FALSE);
        }
        y += btn.bottom + u.top;
        RECT wr{ 0, 0, W, y }; AdjustWindowRectEx(&wr, GetWindowLongW(d, GWL_STYLE), FALSE, GetWindowLongW(d, GWL_EXSTYLE));
        SetWindowPos(d, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top, SWP_NOMOVE | SWP_NOZORDER);
        PanelChrome(d, a->owner);
        if (a->danger && !a->cancel.empty()) {   // destructive: focus Cancel, Enter = Cancel
            SendMessageW(d, DM_SETDEFID, IDCANCEL, 0);
            SetFocus(hCa);
        } else SetFocus(hOk);
        return FALSE;
    }
    case WM_LBUTTONDOWN:
        ReleaseCapture(); SendMessageW(d, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) { EndDialog(d, LOWORD(wp)); return TRUE; }
        break;
    case WM_DESTROY:
        if (head) { DeleteObject(head); head = nullptr; }
        break;
    }
    return FALSE;
}
// Silent replacement for MessageBox. cancel empty = single button. Returns true for the primary button.
static bool Ask(HWND owner, const std::wstring& head, const std::wstring& body,
                const std::wstring& ok, const std::wstring& cancel, bool danger) {
    MsgArgs a{ head, body, ok, cancel, danger, owner };
    return DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_MSG), owner ? owner : g_hwnd, MsgProc, (LPARAM)&a) == IDOK;
}

// "This link is part of a playlist": custom panel (bold heading, two choices, blue OK)
static INT_PTR CALLBACK PlaylistProc(HWND d, UINT msg, WPARAM wp, LPARAM lp) {
    static HFONT head;
    switch (msg) {
    case WM_INITDIALOG: {
        wchar_t b[256];
        for (int id : { IDC_PL_HEAD, IDC_PL_BODY, IDC_PL_ONE, IDC_PL_ALL, IDOK, IDCANCEL }) {
            GetDlgItemTextW(d, id, b, 256); SetDlgItemTextW(d, id, T(b));
        }
        head = MakeHeadFont(d);
        SendDlgItemMessageW(d, IDC_PL_HEAD, WM_SETFONT, (WPARAM)head, TRUE);
        CheckRadioButton(d, IDC_PL_ONE, IDC_PL_ALL, IDC_PL_ONE);
        PanelChrome(d, g_hwnd);
        SetFocus(GetDlgItem(d, IDC_PL_ONE));
        return FALSE;
    }
    case WM_LBUTTONDOWN:   // no title bar: drag anywhere on the panel
        ReleaseCapture(); SendMessageW(d, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) { EndDialog(d, IsDlgButtonChecked(d, IDC_PL_ALL) == BST_CHECKED ? ID_PL_ALL : ID_PL_ONE); return TRUE; }
        if (LOWORD(wp) == IDCANCEL) { EndDialog(d, IDCANCEL); return TRUE; }
        break;
    case WM_DESTROY:
        if (head) { DeleteObject(head); head = nullptr; }
        break;
    }
    return FALSE;
}

static int AskPlaylist() {
    return (int)DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_PLAYLIST), g_hwnd, PlaylistProc, 0);
}

static void PumpOrPrompt() { if (g_ytdlp.empty()) PromptYtdlp(); else Pump(); }
static void StartAll() {
    int n = 0;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        for (auto& t : g_tasks) {
            if (t->st != TS_PAUSED && t->st != TS_FAILED && t->st != TS_CANCELED) continue;
            t->st = TS_QUEUED; t->speed = -1; t->eta = -1;
            t->err.clear(); t->cancel = false; t->pause = false; t->dirty = true; ++n;
        }
    }
    g_statusMsg = n ? T(L"已开始 ") + std::to_wstring(n) + T(L" 个任务") : std::wstring(T(L"没有可开始的任务"));
    SetStatusPart(0, g_statusMsg);
    if (n) PumpOrPrompt();
}
static void StopAll() {
    int n = 0;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        for (auto& t : g_tasks) {
            if (t->st == TS_QUEUED) { t->st = TS_PAUSED; t->dirty = true; ++n; }
            else if (IsActive(t->st)) {
                t->cancel = true; t->pause = true; ++n;
                if (t->job) TerminateJobObject(t->job, 1);
                t->dirty = true;
            }
        }
    }
    g_saveDue = true;
    g_statusMsg = n ? T(L"已暂停 ") + std::to_wstring(n) + T(L" 个任务") : std::wstring(T(L"没有正在下载的任务"));
    SetStatusPart(0, g_statusMsg);
    InvalidateRect(g_hList, nullptr, FALSE);
}

#include "browser.h"

static void AddFromText(const std::wstring& text) {
    auto urls = ExtractUrls(text);
    if (urls.empty()) {
        g_statusMsg = T(L"没有识别到 http/https 链接");
        SetStatusPart(0, g_statusMsg);
        return;
    }
    ReadUiSettings();
    // A video that sits inside a playlist (watch?v=…&list=…): ask once per batch.
    bool anyMixed = false;
    for (auto& u : urls) if (IsVideoInList(u)) anyMixed = true;
    bool wholeList = false;
    if (anyMixed) {
        int r = AskPlaylist();
        if (r == IDCANCEL) return;
        wholeList = r == ID_PL_ALL;
    }
    int last, added = 0, dup = 0;
    std::vector<std::wstring> xhs;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        for (auto& u : urls) {
            if (IsXhs(u)) { xhs.push_back(u); continue; }   // collected in the built-in browser
            bool seen = false;
            for (auto& p : g_pending) if (p->url == u) seen = true;
            for (auto& x : g_tasks)
                if (x->url == u && x->st != TS_DONE && x->st != TS_FAILED && x->st != TS_CANCELED) seen = true;
            if (seen) { ++dup; continue; }
            ++added;
            auto t = std::make_shared<Task>();
            t->id = g_nextId++;
            bool list = IsVideoInList(u) ? wholeList : LooksLikeList(u);
            t->url = u; t->outDir = g_set.outDir; t->fmt = g_set.fmt; t->playlist = list;
            t->expand = list && !g_ytdlp.empty();
            if (t->expand) StartExpand(t);          // kept out of the list until its videos are known
            else g_tasks.push_back(t);
        }
        last = (int)g_tasks.size() - 1;
    }
    for (auto& u : xhs) OpenBrowser(u, true);
    if (!xhs.empty() && !added && !dup) {
        g_statusMsg = T(L"小红书链接已交给内置浏览器抓取");
        SetStatusPart(0, g_statusMsg);
        SetWindowTextW(g_hAdd, T(L"✓ 已添加"));
        SetTimer(g_hwnd, 2, 1500, nullptr);
        return;
    }
    RefreshListCount();
    if (last >= 0) ListView_EnsureVisible(g_hList, last, FALSE);
    g_statusMsg = !added ? std::wstring(L"这个链接已在下载列表中")
                : T(L"已添加 ") + std::to_wstring(added) + T(L" 个链接") + (dup ? L"，" + std::to_wstring(dup) + T(L" 个已在列表中") : L"");
    SetStatusPart(0, g_statusMsg);
    SetWindowTextW(g_hAdd, added ? T(L"✓ 已添加") : T(L"已在列表中"));
    SetTimer(g_hwnd, 2, 1500, nullptr);
    if (!added) return;
    if (g_ytdlp.empty()) PromptYtdlp(); else Pump();
}

static std::wstring ClipboardText(HWND h) {
    std::wstring s;
    if (!OpenClipboard(h)) return s;
    if (HANDLE d = GetClipboardData(CF_UNICODETEXT)) {
        if (auto p = (const wchar_t*)GlobalLock(d)) { s = p; GlobalUnlock(d); }
    }
    CloseClipboard();
    return s;
}

static void SetClipboardText(HWND h, const std::wstring& s) {
    if (!OpenClipboard(h)) return;
    EmptyClipboard();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (s.size() + 1) * sizeof(wchar_t));
    if (g) {
        memcpy(GlobalLock(g), s.c_str(), (s.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(g);
        SetClipboardData(CF_UNICODETEXT, g);
    }
    CloseClipboard();
}

static std::vector<int> SelectedIndices() {
    std::vector<int> v;
    int i = -1;
    while ((i = ListView_GetNextItem(g_hList, i, LVNI_SELECTED)) != -1) v.push_back(i);
    return v;
}

static std::vector<std::shared_ptr<Task>> SelectedTasks() {
    auto idx = SelectedIndices();
    std::vector<std::shared_ptr<Task>> v;
    std::lock_guard<std::mutex> lk(g_mx);
    for (int i : idx) if (i >= 0 && i < (int)g_tasks.size()) v.push_back(g_tasks[i]);
    return v;
}

static void CancelTask(const std::shared_ptr<Task>& t) {
    std::lock_guard<std::mutex> lk(g_mx);
    if (t->st == TS_QUEUED || t->st == TS_PAUSED) { t->st = TS_CANCELED; t->dirty = true; return; }
    if (t->st == TS_EXPANDING) { t->cancel = true; t->st = TS_CANCELED; t->title.clear(); t->dirty = true; return; }
    if (IsActive(t->st)) {
        t->cancel = true;
        if (t->job) TerminateJobObject(t->job, 1);
        t->dirty = true;
    }
}

static void RemoveTasks(const std::vector<std::shared_ptr<Task>>& sel) {
    for (auto& t : sel) CancelTask(t);
    {
        std::lock_guard<std::mutex> lk(g_mx);
        g_tasks.erase(std::remove_if(g_tasks.begin(), g_tasks.end(), [&](const std::shared_ptr<Task>& t) {
            return std::find(sel.begin(), sel.end(), t) != sel.end();
        }), g_tasks.end());
    }
    ListView_SetItemState(g_hList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    RefreshListCount();
}

static void OpenTaskFolder(const std::shared_ptr<Task>& t) {
    std::wstring fp, dir;
    { std::lock_guard<std::mutex> lk(g_mx); fp = t->filepath; dir = t->outDir; }
    if (!fp.empty() && FileExists(fp)) {
        if (PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(fp.c_str())) {
            SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
            ILFree(pidl);
            return;
        }
    }
    ShellExecuteW(g_hwnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void OpenTaskFile(const std::shared_ptr<Task>& t) {
    std::wstring fp;
    { std::lock_guard<std::mutex> lk(g_mx); fp = t->filepath; }
    if (!fp.empty() && FileExists(fp)) ShellExecuteW(g_hwnd, L"open", fp.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    else OpenTaskFolder(t);
}

static void BrowseFolder(HWND owner, HWND edit) {
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return;
    DWORD opt = 0; dlg->GetOptions(&opt);
    dlg->SetOptions(opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(L"选择保存位置");
    std::wstring cur = GetText(edit);
    IShellItem* init = nullptr;
    if (!cur.empty() && SUCCEEDED(SHCreateItemFromParsingName(cur.c_str(), nullptr, IID_PPV_ARGS(&init)))) {
        dlg->SetFolder(init); init->Release();
    }
    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(dlg->GetResult(&it))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { SetWindowTextW(edit, p); CoTaskMemFree(p); }
            it->Release();
        }
    }
    dlg->Release();
}

// ---------------------------------------------------------------------------
// ListView: virtual data + custom-drawn themed progress bar
// ---------------------------------------------------------------------------
static const wchar_t* StateText(TaskState s) {
    switch (s) {
    case TS_EXPANDING: return T(L"读取列表…");
    case TS_PAUSED: return T(L"已暂停");
    case TS_QUEUED: return T(L"等待中");
    case TS_STARTING: return T(L"准备中…");
    case TS_DOWNLOADING: return T(L"下载中");
    case TS_PROCESSING: return T(L"处理中…");
    case TS_DONE: return T(L"已完成");
    case TS_FAILED: return T(L"失败");
    case TS_CANCELED: return T(L"已取消");
    }
    return L"";
}

static void OnGetDispInfo(NMLVDISPINFOW* di) {
    LVITEMW& it = di->item;
    if (!(it.mask & LVIF_TEXT) || !it.pszText || it.cchTextMax <= 0) return;
    std::wstring s;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        if (it.iItem < 0 || it.iItem >= (int)g_tasks.size()) { it.pszText[0] = 0; return; }
        const Task& t = *g_tasks[it.iItem];
        switch (it.iSubItem) {
        case 0:
            if (t.plCount > 0) s = L"[" + std::to_wstring(t.plIdx) + L"/" + std::to_wstring(t.plCount) + L"] ";
            s += t.title.empty() ? t.url : t.title;
            break;
        case 1: s = StateText(t.st); break;
        case 2: break;
        case 3: s = FmtBytes(t.total); break;
        case 4: if (t.st == TS_DOWNLOADING && t.speed > 0) s = FmtBytes(t.speed) + L"/s"; break;
        case 5: if (t.st == TS_DOWNLOADING) s = FmtEta(t.eta); break;
        case 6:
            if (t.st == TS_DOWNLOADING && t.nParts == 2)
                s = ((t.part == 0) != t.part0Audio) ? T(L"正在下载视频") : T(L"正在下载音频");
            else if (t.st == TS_PROCESSING) s = t.nParts == 2 ? T(L"正在合并") : T(L"正在处理");
            else if (t.st == TS_FAILED) s = t.err;
            else if (!t.filepath.empty()) s = t.filepath;
            else s = t.url;
            break;
        }
    }
    lstrcpynW(it.pszText, s.c_str(), it.cchTextMax);
}

static void DrawProgressCell(HDC hdc, int item) {
    double pct; TaskState st;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        if (item < 0 || item >= (int)g_tasks.size()) return;
        pct = g_tasks[item]->pct; st = g_tasks[item]->st;
    }
    RECT cell;
    if (!ListView_GetSubItemRect(g_hList, item, 2, LVIR_BOUNDS, &cell)) return;
    int textW = S(48);
    int barH = S(12);
    RECT bar = { cell.left + S(6), cell.top + (cell.bottom - cell.top - barH) / 2, cell.right - textW - S(4), 0 };
    bar.bottom = bar.top + barH;
    if (bar.right - bar.left < S(16)) { bar.right = cell.right - S(6); textW = 0; }

    if (st == TS_DONE) pct = 100;
    int state = (st == TS_FAILED) ? PBFS_ERROR : (st == TS_CANCELED || st == TS_PAUSED) ? PBFS_PAUSED : PBFS_NORMAL;
    if (g_progTheme) {
        DrawThemeBackground(g_progTheme, hdc, PP_BAR, 0, &bar, nullptr);
        RECT in; GetThemeBackgroundContentRect(g_progTheme, hdc, PP_BAR, 0, &bar, &in);
        RECT fill = in;
        fill.right = in.left + (LONG)((in.right - in.left) * std::max(0.0, std::min(100.0, pct)) / 100.0);
        if (fill.right > fill.left) DrawThemeBackground(g_progTheme, hdc, PP_FILL, state, &fill, nullptr);
    } else {
        FrameRect(hdc, &bar, GetSysColorBrush(COLOR_BTNSHADOW));
        RECT fill = bar; InflateRect(&fill, -1, -1);
        fill.right = fill.left + (LONG)((fill.right - fill.left) * pct / 100.0);
        FillRect(hdc, &fill, GetSysColorBrush(COLOR_HIGHLIGHT));
    }
    if (textW > 0 && st != TS_QUEUED && st != TS_EXPANDING) {
        wchar_t s[16];
        _snwprintf(s, 16, L"%.1f%%", pct); s[15] = 0;
        RECT tr = { cell.right - textW - S(4), cell.top, cell.right - S(4), cell.bottom };
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        HGDIOBJ old = SelectObject(hdc, g_font);
        DrawTextW(hdc, s, -1, &tr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, old);
    }
}

static LRESULT OnListCustomDraw(NMLVCUSTOMDRAW* cd) {
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT: return CDRF_NOTIFYSUBITEMDRAW;
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
        return cd->iSubItem == 2 ? CDRF_NOTIFYPOSTPAINT : CDRF_DODEFAULT;
    case CDDS_ITEMPOSTPAINT | CDDS_SUBITEM:
        if (cd->iSubItem == 2) DrawProgressCell(cd->nmcd.hdc, (int)cd->nmcd.dwItemSpec);
        return CDRF_DODEFAULT;
    }
    return CDRF_DODEFAULT;
}

static void ShowSelectionInfo() {
    auto sel = SelectedTasks();
    std::wstring m = g_statusMsg;
    if (sel.size() == 1) {
        std::lock_guard<std::mutex> lk(g_mx);
        auto& t = *sel[0];
        if (t.st == TS_FAILED && !t.err.empty()) m = T(L"错误：") + t.err;
        else if (!t.filepath.empty()) m = t.filepath;
        else m = t.url;
    } else if (sel.size() > 1) {
        m = g_en ? std::to_wstring(sel.size()) + L" selected" : L"已选择 " + std::to_wstring(sel.size()) + L" 项";
    }
    SetStatusPart(0, m);
}

static void OnTimer() {
    FitNameColumn();   // the vertical scroll bar may have come or gone
    std::vector<int> dirty;
    int active = 0, queued = 0, reading = 0;
    double speed = 0, pctSum = 0;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        reading = (int)g_pending.size();
        for (int i = 0; i < (int)g_tasks.size(); ++i) {
            Task& t = *g_tasks[i];
            if (t.dirty) { dirty.push_back(i); t.dirty = false; g_saveDue = true; }
            if (IsActive(t.st)) { ++active; pctSum += t.pct; if (t.speed > 0 && t.st == TS_DOWNLOADING) speed += t.speed; }
            else if (t.st == TS_QUEUED) ++queued;
        }
    }
    if (!dirty.empty()) {
        if (dirty.size() > 32) InvalidateRect(g_hList, nullptr, FALSE);
        else for (int i : dirty) ListView_RedrawItems(g_hList, i, i);
    }
    static std::wstring lastCnt, lastSpd;
    std::wstring cnt = T(L"下载中 ") + std::to_wstring(active) + T(L" · 等待 ") + std::to_wstring(queued);
    if (reading) cnt = T(L"读取列表 ") + std::to_wstring(reading) + L" · " + cnt;
    {
        static int lastBusy = -1;
        int busy = reading;           // links already in the list are not shown as parsing
        if (busy != lastBusy) {
            if (busy) {
                std::wstring b = T(L"正在解析 ") + std::to_wstring(busy) + T(L" 个链接…");
                SetWindowTextW(g_hBusyTxt, b.c_str());
                if (lastBusy <= 0) {
                    ShowWindow(g_hBusy, SW_SHOWNA); ShowWindow(g_hBusyTxt, SW_SHOWNA);
                    SendMessageW(g_hBusy, PBM_SETMARQUEE, TRUE, 30);
                }
            } else {
                SendMessageW(g_hBusy, PBM_SETMARQUEE, FALSE, 0);
                ShowWindow(g_hBusy, SW_HIDE); ShowWindow(g_hBusyTxt, SW_HIDE);
            }
            lastBusy = busy;
        }
    }
    if (g_saveDue2) { g_saveDue = true; g_saveDue2 = false; }
    if (g_saveDue && GetTickCount() - g_lastSave > 3000) SaveTasks();
    std::wstring spd = active ? FmtBytes(speed) + L"/s" : L"";
    if (cnt != lastCnt) { SetStatusPart(3, cnt); lastCnt = cnt; }
    if (spd != lastSpd) { SetStatusPart(4, spd); lastSpd = spd; }

    if (g_taskbar) {
        if (active) {
            g_taskbar->SetProgressState(g_hwnd, TBPF_NORMAL);
            g_taskbar->SetProgressValue(g_hwnd, (ULONGLONG)(pctSum / active * 10), 1000);
        } else if (g_lastActive) {
            g_taskbar->SetProgressState(g_hwnd, TBPF_NOPROGRESS);
        }
    }
    if (g_lastActive && !active && !queued && GetForegroundWindow() != g_hwnd) {
        FLASHWINFO fi{ sizeof(fi), g_hwnd, FLASHW_TRAY | FLASHW_TIMERNOFG, 3, 0 };
        FlashWindowEx(&fi);
    }
    g_lastActive = active;
}

static void ShowContextMenu(int x, int y) {
    auto sel = SelectedTasks();
    bool any = !sel.empty(), canRetry = false, canCancel = false, hasFile = false;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        for (auto& t : sel) {
            if (IsActive(t->st) || t->st == TS_QUEUED || t->st == TS_EXPANDING || t->st == TS_PAUSED) canCancel = true;
            if (t->st == TS_FAILED || t->st == TS_CANCELED || t->st == TS_DONE || t->st == TS_PAUSED) canRetry = true;
            if (!t->filepath.empty()) hasFile = true;
        }
    }
    if (x == -1 && y == -1) {
        int i = ListView_GetNextItem(g_hList, -1, LVNI_FOCUSED);
        RECT r{};
        if (i >= 0 && ListView_GetItemRect(g_hList, i, &r, LVIR_LABEL)) {}
        else GetClientRect(g_hList, &r);
        POINT p = { r.left + S(16), r.bottom };
        ClientToScreen(g_hList, &p);
        x = p.x; y = p.y;
    }
    HMENU m = CreatePopupMenu();
    UINT on = MF_STRING, off = MF_STRING | MF_GRAYED;
    AppendMenuW(m, (any && hasFile) ? on : off, ID_OPENFILE, T(L"打开文件\tEnter"));
    AppendMenuW(m, any ? on : off, ID_OPENFOLDER, T(L"打开所在文件夹"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, canRetry ? on : off, ID_RETRY, T(L"重新下载"));
    AppendMenuW(m, canCancel ? on : off, ID_CANCELTASK, T(L"取消"));
    AppendMenuW(m, any ? on : off, ID_COPYURL, T(L"复制链接"));
    AppendMenuW(m, any ? on : off, ID_LOGINSITE, T(L"在内置浏览器中登录"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, any ? on : off, ID_REMOVE, T(L"移除\tDel"));
    AppendMenuW(m, (any && hasFile) ? on : off, ID_DELFILE, T(L"从硬盘删除文件"));
    AppendMenuW(m, on, ID_CLEARDONE, T(L"清除已完成"));
    AppendMenuW(m, on, ID_SELALL, T(L"全选\tCtrl+A"));
    TrackPopupMenuEx(m, TPM_RIGHTBUTTON, x, y, g_hwnd, nullptr);
    DestroyMenu(m);
}

// ---------------------------------------------------------------------------
// Settings dialog
// ---------------------------------------------------------------------------
static const wchar_t* kBrowsers[] = { L"不使用", L"builtin", L"firefox", L"chrome", L"edge", L"brave", L"opera", L"vivaldi", L"chromium" };

static bool g_langChanged = false, g_restart = false;
// Settings > Subtitle languages: a check box per language, two columns. lParam: std::wstring* (in/out)
static INT_PTR CALLBACK SubLangsProc(HWND d, UINT msg, WPARAM wp, LPARAM lp) {
    static HFONT bold;
    switch (msg) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(d, DWLP_USER, lp);
        auto* list = (std::wstring*)lp;
        wchar_t b[256];
        GetWindowTextW(d, b, 256); SetWindowTextW(d, T(b));
        for (int id : { IDC_SL_HEAD, IDC_SL_ALL, IDC_SL_NONE, IDOK, IDCANCEL }) {
            GetDlgItemTextW(d, id, b, 256); SetDlgItemTextW(d, id, T(b));
        }
        LOGFONTW lf{};
        GetObjectW((HFONT)SendMessageW(d, WM_GETFONT, 0, 0), sizeof(lf), &lf);
        lf.lfWeight = FW_SEMIBOLD;
        bold = CreateFontIndirectW(&lf);
        SendDlgItemMessageW(d, IDC_SL_HEAD, WM_SETFONT, (WPARAM)bold, TRUE);
        for (int i = 0; i < kSubLangCount; ++i) {
            SetDlgItemTextW(d, IDC_SL_FIRST + i, g_en ? kSubLangs[i].en : kSubLangs[i].zh);
            CheckDlgButton(d, IDC_SL_FIRST + i, SubLangOn(*list, kSubLangs[i].code) ? BST_CHECKED : BST_UNCHECKED);
        }
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_SL_ALL: case IDC_SL_NONE:
            for (int i = 0; i < kSubLangCount; ++i)
                CheckDlgButton(d, IDC_SL_FIRST + i, LOWORD(wp) == IDC_SL_ALL ? BST_CHECKED : BST_UNCHECKED);
            return TRUE;
        case IDOK: {
            std::wstring r;
            for (int i = 0; i < kSubLangCount; ++i)
                if (IsDlgButtonChecked(d, IDC_SL_FIRST + i) == BST_CHECKED) r += (r.empty() ? L"" : L",") + std::wstring(kSubLangs[i].code);
            if (r.empty()) {
                Ask(d, T(L"请至少选择一种字幕语言"), L"", T(L"确定"), L"", false);
                return TRUE;
            }
            *(std::wstring*)GetWindowLongPtrW(d, DWLP_USER) = r;
            EndDialog(d, IDOK);
            return TRUE;
        }
        case IDCANCEL: EndDialog(d, IDCANCEL); return TRUE;
        }
        break;
    case WM_DESTROY:
        if (bold) { DeleteObject(bold); bold = nullptr; }
        break;
    }
    return FALSE;
}

// Settings > Components: version / status of the bundled tools
static void ShowCompInfo(HWND d) {
    auto bundled = [](const std::wstring& p) {
        std::wstring t = ExeDir() + L"\\tools\\";
        return !p.empty() && _wcsnicmp(p.c_str(), t.c_str(), t.size()) == 0;
    };
    std::wstring y = g_ytdlp.empty() ? T(L"未找到") : (g_ytdlpVer.empty() ? std::wstring(L"…") : g_ytdlpVer);
    if (!g_ytdlp.empty() && bundled(g_ytdlp)) y += T(L"（内置）");
    std::wstring f = g_ffmpeg.empty() ? T(L"未找到（合并高清需要）") : std::wstring(T(L"已就绪"));
    if (!g_ffmpeg.empty() && bundled(g_ffmpeg)) f += T(L"（内置）");
    SetDlgItemTextW(d, IDC_YTVER, y.c_str());
    SetDlgItemTextW(d, IDC_FFINFO, f.c_str());
}

// Edit that takes a number with at most one decimal point (Settings > speed limit)
static LRESULT CALLBACK NumberEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR) {
    if (msg == WM_CHAR && wp >= 32) {
        if (iswdigit((wint_t)wp)) return DefSubclassProc(h, msg, wp, lp);
        if (wp == L'.') {
            wchar_t b[64]; GetWindowTextW(h, b, 64);
            if (!wcschr(b, L'.')) return DefSubclassProc(h, msg, wp, lp);
        }
        return 0;
    }
    if (msg == WM_PASTE) {
        if (OpenClipboard(h)) {
            std::wstring t;
            if (HANDLE hd = GetClipboardData(CF_UNICODETEXT)) { if (auto* p = (const wchar_t*)GlobalLock(hd)) { t = p; GlobalUnlock(hd); } }
            CloseClipboard();
            std::wstring keep; bool dot = false;
            for (wchar_t c : t) if (iswdigit(c) || (c == L'.' && !dot)) { keep += c; if (c == L'.') dot = true; }
            SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)keep.c_str());
        }
        return 0;
    }
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(h, NumberEditProc, id);
    return DefSubclassProc(h, msg, wp, lp);
}

static std::wstring g_subLangsEdit;   // the choice while Settings is open

static INT_PTR CALLBACK SettingsProc(HWND d, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        g_hSettingsDlg = d;
        {
            wchar_t b[256];
            GetWindowTextW(d, b, 256); SetWindowTextW(d, T(b));
            EnumChildWindows(d, [](HWND h, LPARAM) -> BOOL {
                wchar_t cls[32], t[256];
                GetClassNameW(h, cls, 32);
                if (_wcsicmp(cls, L"Edit") && _wcsicmp(cls, L"ComboBox") && GetWindowTextW(h, t, 256) > 0) SetWindowTextW(h, T(t));
                return TRUE;
            }, 0);
            // line each label up with the box beside it (same top, same height, text centred vertically)
            EnumChildWindows(d, [](HWND h, LPARAM lp) -> BOOL {
                HWND dlg = (HWND)lp;
                wchar_t cls[32];
                GetClassNameW(h, cls, 32);
                int cid = GetDlgCtrlID(h);
                if (_wcsicmp(cls, L"Static") || cid == IDC_COMPINFO || cid == IDC_YTVER || cid == IDC_FFINFO) return TRUE;
                RECT rl; GetWindowRect(h, &rl); MapWindowPoints(nullptr, dlg, (POINT*)&rl, 2);
                HWND best = nullptr; RECT rb{}; int bestDx = 1 << 30;
                for (HWND c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
                    GetClassNameW(c, cls, 32);
                    if (_wcsicmp(cls, L"ComboBox") && _wcsicmp(cls, L"Edit")) continue;
                    RECT rc; GetWindowRect(c, &rc); MapWindowPoints(nullptr, dlg, (POINT*)&rc, 2);
                    if (rc.left <= rl.left || rc.bottom <= rl.top || rc.top >= rl.bottom) continue;
                    if (rc.left - rl.left < bestDx) { bestDx = rc.left - rl.left; best = c; rb = rc; }
                }
                if (!best) return TRUE;
                SetWindowLongW(h, GWL_STYLE, GetWindowLongW(h, GWL_STYLE) | SS_CENTERIMAGE);
                SetWindowPos(h, nullptr, rl.left, rb.top, rl.right - rl.left, rb.bottom - rb.top, SWP_NOZORDER | SWP_NOACTIVATE);
                return TRUE;
            }, (LPARAM)d);
            HWND lc = GetDlgItem(d, IDC_LANG);
            SendMessageW(lc, CB_ADDSTRING, 0, (LPARAM)T(L"跟随系统"));
            SendMessageW(lc, CB_ADDSTRING, 0, (LPARAM)L"简体中文");
            SendMessageW(lc, CB_ADDSTRING, 0, (LPARAM)L"English");
            SendMessageW(lc, CB_SETCURSEL, g_set.lang, 0);
        }
        HWND c = GetDlgItem(d, IDC_CONC);
        for (int i = 1; i <= 10; ++i) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)std::to_wstring(i).c_str());
        SendMessageW(c, CB_SETCURSEL, g_set.conc - 1, 0);
        c = GetDlgItem(d, IDC_FRAG);
        for (int i = 1; i <= 16; ++i) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)std::to_wstring(i).c_str());
        SendMessageW(c, CB_SETCURSEL, g_set.frag - 1, 0);
        c = GetDlgItem(d, IDC_COOKIES);
        int sel = 0;
        for (int i = 0; i < (int)(sizeof(kBrowsers) / sizeof(*kBrowsers)); ++i) {
            SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)(i == 1 ? T(L"内置浏览器（推荐）") : T(kBrowsers[i])));
            if (i && g_set.cookies == kBrowsers[i]) sel = i;
        }
        SendMessageW(c, CB_SETCURSEL, sel, 0);
        {   // "5M" / "500K" / "1.5M" -> number + unit
            std::wstring r = Trim(g_set.rate), num; int unit = 1;
            size_t k = 0;
            while (k < r.size() && (iswdigit(r[k]) || r[k] == L'.')) num += r[k++];
            wchar_t u = k < r.size() ? (wchar_t)towupper(r[k]) : 0;
            if (u == L'K') unit = 0;
            else if (u == 0 && !num.empty()) {          // plain bytes per second
                double v = _wtof(num.c_str()) / 1024.0; unit = 0;
                wchar_t b[32]; swprintf(b, 32, L"%.0f", v < 1 ? 1.0 : v); num = b;
            }
            SetDlgItemTextW(d, IDC_RATE, num.c_str());
            SendDlgItemMessageW(d, IDC_RATEUNIT, CB_ADDSTRING, 0, (LPARAM)L"KB/s");
            SendDlgItemMessageW(d, IDC_RATEUNIT, CB_ADDSTRING, 0, (LPARAM)L"MB/s");
            SendDlgItemMessageW(d, IDC_RATEUNIT, CB_SETCURSEL, unit, 0);
            SetWindowSubclass(GetDlgItem(d, IDC_RATE), NumberEditProc, 1, 0);
            {   // same top and height as the unit drop-down beside it
                RECT re, ru; HWND he = GetDlgItem(d, IDC_RATE), hu = GetDlgItem(d, IDC_RATEUNIT);
                GetWindowRect(he, &re); GetWindowRect(hu, &ru);
                MapWindowPoints(nullptr, d, (POINT*)&re, 2); MapWindowPoints(nullptr, d, (POINT*)&ru, 2);
                SetWindowPos(he, nullptr, re.left, ru.top, re.right - re.left, ru.bottom - ru.top, SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        SetDlgItemTextW(d, IDC_PROXY, g_set.proxy.c_str());
        SendDlgItemMessageW(d, IDC_RATE, EM_SETCUEBANNER, TRUE, (LPARAM)T(L"不限速"));
        SendDlgItemMessageW(d, IDC_PROXY, EM_SETCUEBANNER, TRUE, (LPARAM)T(L"例如 http://127.0.0.1:7890"));
        CheckDlgButton(d, IDC_SUBS, g_set.subs ? BST_CHECKED : BST_UNCHECKED);
        g_subLangsEdit = g_set.subLangs;
        EnableWindow(GetDlgItem(d, IDC_SUBLANGS), g_set.subs);
        CheckDlgButton(d, IDC_THUMB, g_set.thumb ? BST_CHECKED : BST_UNCHECKED);
        ShowCompInfo(d);
        SetDlgItemTextW(d, IDC_COMPINFO, T(L"网站常变，下载失败时先点这个"));
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_SUBS:
            EnableWindow(GetDlgItem(d, IDC_SUBLANGS), IsDlgButtonChecked(d, IDC_SUBS) == BST_CHECKED);
            return TRUE;
        case IDC_SUBLANGS:
            DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_SUBLANGS), d, SubLangsProc, (LPARAM)&g_subLangsEdit);
            return TRUE;
        case IDC_UPDATE:
            StartComponent(ID_UPDATE_YTDLP);
            SetDlgItemTextW(d, IDC_COMPINFO, T(L"进度见主窗口状态栏"));
            return TRUE;
        case IDOK: {
            g_set.conc = (int)SendDlgItemMessageW(d, IDC_CONC, CB_GETCURSEL, 0, 0) + 1;
            g_set.frag = (int)SendDlgItemMessageW(d, IDC_FRAG, CB_GETCURSEL, 0, 0) + 1;
            int ck = (int)SendDlgItemMessageW(d, IDC_COOKIES, CB_GETCURSEL, 0, 0);
            g_set.cookies = ck > 0 ? kBrowsers[ck] : L"";
            wchar_t b[2048];
            GetDlgItemTextW(d, IDC_RATE, b, 2048);
            {
                std::wstring num = Trim(b);
                while (!num.empty() && num.back() == L'.') num.pop_back();
                bool kb = SendDlgItemMessageW(d, IDC_RATEUNIT, CB_GETCURSEL, 0, 0) == 0;
                g_set.rate = (num.empty() || _wtof(num.c_str()) <= 0) ? L"" : num + (kb ? L"K" : L"M");
            }
            GetDlgItemTextW(d, IDC_PROXY, b, 2048);  g_set.proxy = Trim(b);
            g_set.subs = IsDlgButtonChecked(d, IDC_SUBS) == BST_CHECKED;
            g_set.subLangs = g_subLangsEdit;
            g_set.thumb = IsDlgButtonChecked(d, IDC_THUMB) == BST_CHECKED;
            {
                int lg = (int)SendDlgItemMessageW(d, IDC_LANG, CB_GETCURSEL, 0, 0);
                if (lg >= 0 && lg != g_set.lang) { g_set.lang = lg; g_langChanged = true; }
            }
            SaveSettings();
            EndDialog(d, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(d, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        g_hSettingsDlg = nullptr;
        break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Main window
// ---------------------------------------------------------------------------
static HWND MakeCtl(const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0) {
    return CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, g_hwnd,
                           (HMENU)(INT_PTR)id, g_hInst, nullptr);
}

// Edit boxes get a short Chinese context menu instead of the system one
// (which adds reading order / Unicode control characters / IME entries).
static LRESULT CALLBACK EditMenuProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_CONTEXTMENU) {
        SetFocus(h);
        DWORD a = 0, b = 0;
        SendMessageW(h, EM_GETSEL, (WPARAM)&a, (LPARAM)&b);
        bool sel = b > a, ro = (GetWindowLongW(h, GWL_STYLE) & ES_READONLY) != 0;
        bool any = GetWindowTextLengthW(h) > 0;
        bool clip = IsClipboardFormatAvailable(CF_UNICODETEXT) != 0;
        HMENU m = CreatePopupMenu();
        auto on = [](bool e) { return MF_STRING | (e ? 0u : (UINT)MF_GRAYED); };
        AppendMenuW(m, on(!ro && SendMessageW(h, EM_CANUNDO, 0, 0)), 1, T(L"撤销"));
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, on(sel && !ro), 2, T(L"剪切"));
        AppendMenuW(m, on(sel), 3, T(L"复制"));
        AppendMenuW(m, on(clip && !ro), 4, T(L"粘贴"));
        AppendMenuW(m, on(sel && !ro), 5, T(L"删除"));
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, on(any), 6, T(L"全选"));
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        if (x == -1 && y == -1) { RECT r; GetWindowRect(h, &r); x = r.left + S(8); y = r.top + S(8); }
        int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, h, nullptr);
        DestroyMenu(m);
        switch (c) {
        case 1: SendMessageW(h, WM_UNDO, 0, 0); break;
        case 2: SendMessageW(h, WM_CUT, 0, 0); break;
        case 3: SendMessageW(h, WM_COPY, 0, 0); break;
        case 4: SendMessageW(h, WM_PASTE, 0, 0); break;
        case 5: SendMessageW(h, WM_CLEAR, 0, 0); break;
        case 6: SendMessageW(h, EM_SETSEL, 0, -1); break;
        }
        return 0;
    }
    return DefSubclassProc(h, msg, wp, lp);
}

static void OnCreate(HWND hwnd) {
    g_hwnd = hwnd;
    g_dpi = GetDpiForWindow(hwnd);
    CreateUiFont();

    g_hUrlLbl  = MakeCtl(WC_STATICW, T(L"视频链接（支持一次粘贴多个，按 Enter 添加）"), SS_LEFT | SS_NOPREFIX, IDC_URLLBL);
    g_hUrl     = MakeCtl(WC_EDITW, L"", WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, IDC_URL, WS_EX_CLIENTEDGE);
    g_hAdd     = MakeCtl(WC_BUTTONW, T(L"解析并下载"), WS_TABSTOP | BS_DEFPUSHBUTTON, IDC_ADD);
    g_hFmtLbl  = MakeCtl(WC_STATICW, T(L"格式"), SS_LEFT, IDC_FMTLBL);
    g_hFmt     = MakeCtl(WC_COMBOBOXW, L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, IDC_FMT);
    g_hDirLbl  = MakeCtl(WC_STATICW, T(L"保存到"), SS_LEFT, IDC_DIRLBL);
    g_hDir     = MakeCtl(WC_EDITW, g_set.outDir.c_str(), WS_TABSTOP | ES_AUTOHSCROLL, IDC_DIR, WS_EX_CLIENTEDGE);
    g_hBrowse  = MakeCtl(WC_BUTTONW, T(L"浏览…"), WS_TABSTOP | BS_PUSHBUTTON, IDC_BROWSE);
    g_hStartAll= MakeCtl(WC_BUTTONW, T(L"开始全部"), WS_TABSTOP | BS_PUSHBUTTON, IDC_STARTALL);
    g_hStopAll = MakeCtl(WC_BUTTONW, T(L"停止全部"), WS_TABSTOP | BS_PUSHBUTTON, IDC_STOPALL);
    g_hLogin   = MakeCtl(WC_BUTTONW, T(L"登录网站…"), WS_TABSTOP | BS_PUSHBUTTON, IDC_LOGIN);
    g_hOpenDir = MakeCtl(WC_BUTTONW, T(L"打开文件夹"), WS_TABSTOP | BS_PUSHBUTTON, IDC_OPENDIR);
    g_hSettings= MakeCtl(WC_BUTTONW, T(L"设置…"), WS_TABSTOP | BS_PUSHBUTTON, IDC_SETTINGS);
    g_hList    = MakeCtl(WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS, IDC_LIST, WS_EX_CLIENTEDGE);
    g_hStatus  = MakeCtl(STATUSCLASSNAMEW, L"", SBARS_SIZEGRIP, IDC_STATUS);
    // "parsing" indicator: visible while links are read, so every click gets feedback
    g_hBusy    = MakeCtl(PROGRESS_CLASSW, L"", PBS_MARQUEE, IDC_BUSY);
    g_hBusyTxt = MakeCtl(WC_STATICW, L"", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, IDC_BUSYTXT);
    ShowWindow(g_hBusy, SW_HIDE); ShowWindow(g_hBusyTxt, SW_HIDE);

    SetWindowSubclass(g_hUrl, EditMenuProc, 1, 0);
    SetWindowSubclass(g_hDir, EditMenuProc, 1, 0);
    for (int i = 0; i < kFormatCount; ++i) SendMessageW(g_hFmt, CB_ADDSTRING, 0, (LPARAM)T(g_formats[i].name));
    SendMessageW(g_hFmt, CB_SETCURSEL, (g_set.fmt >= 0 && g_set.fmt < kFormatCount) ? g_set.fmt : 0, 0);
    SendMessageW(g_hDir, EM_SETCUEBANNER, TRUE, (LPARAM)T(L"选择保存目录"));

    SetWindowTheme(g_hList, L"Explorer", nullptr);
    ListView_SetExtendedListViewStyle(g_hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP);
    for (int i = 0; i < kCols; ++i) {
        LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        c.fmt = (i >= 3 && i <= 5) ? LVCFMT_RIGHT : LVCFMT_LEFT;
        c.cx = S(kColW[i]); c.pszText = (LPWSTR)T(kColName[i]);
        ListView_InsertColumn(g_hList, i, &c);
    }
    UpdateRowHeight();
    g_progTheme = OpenThemeData(g_hList, L"PROGRESS");

    ApplyFont(hwnd);
    Layout();
    DetectTools();
    UpdateToolStatus();
    SetStatusPart(0, T(L"就绪"));
    LoadTasks();
    RefreshListCount();
    { int n = TaskCount(); if (n) { g_statusMsg = T(L"已恢复 ") + std::to_wstring(n) + T(L" 个任务") + T(L"，未完成的点「开始全部」继续"); SetStatusPart(0, g_statusMsg); } }
    Pump();
    SetTimer(hwnd, 1, 250, nullptr);
    SetFocus(g_hUrl);
}

static void OnDpiChanged(HWND hwnd, UINT dpi, const RECT* r) {
    g_dpi = dpi;
    CreateUiFont();
    ApplyFont(hwnd);
    UpdateColumns();
    UpdateRowHeight();
    SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
    Layout();
}

static void CommitUiAndSave() {
    ReadUiSettings();
    WINDOWPLACEMENT wpl{ sizeof(wpl) };
    if (GetWindowPlacement(g_hwnd, &wpl)) {
        g_set.x = wpl.rcNormalPosition.left; g_set.y = wpl.rcNormalPosition.top;
        g_set.w = wpl.rcNormalPosition.right - wpl.rcNormalPosition.left;
        g_set.h = wpl.rcNormalPosition.bottom - wpl.rcNormalPosition.top;
        g_set.maximized = wpl.showCmd == SW_SHOWMAXIMIZED;
    }
    SaveSettings();
}

static void OnCommand(HWND hwnd, int id, int code) {
    switch (id) {
    case IDC_ADD: {
        if (GetFocus() == g_hList) { auto s = SelectedTasks(); if (!s.empty()) OpenTaskFile(s[0]); return; }
        std::wstring text = GetText(g_hUrl);
        bool has = !ExtractUrls(text).empty();
        AddFromText(text);
        if (has) SetWindowTextW(g_hUrl, L"");
        SetFocus(g_hUrl);
        break;
    }
    case IDC_PASTE:
        AddFromText(ClipboardText(hwnd));
        break;
    case IDC_BROWSE:
        BrowseFolder(hwnd, g_hDir);
        ReadUiSettings();
        break;
    case IDC_OPENDIR: {
        ReadUiSettings();
        SHCreateDirectoryExW(nullptr, g_set.outDir.c_str(), nullptr);
        ShellExecuteW(hwnd, L"open", g_set.outDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    }
    case IDC_SETTINGS:
        g_langChanged = false;
        if (DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_SETTINGS), hwnd, SettingsProc, 0) == IDOK) {
            DetectTools(); UpdateToolStatus(); Pump();
            if (g_langChanged &&
                Ask(hwnd, L"重启 YtGrab？  Restart YtGrab?",
                    L"界面语言将在重启后生效，未完成的下载会保留。\nThe new language applies after a restart. Unfinished downloads are kept.",
                    L"重启 Restart", L"稍后 Later", false)) {
                g_restart = true;
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            }
        }
        break;
    case IDC_FMT:
        if (code == CBN_SELCHANGE) ReadUiSettings();
        break;
    case IDC_STARTALL: StartAll(); break;
    case IDC_STOPALL:  StopAll(); break;
    case ID_OPENFILE:   { auto s = SelectedTasks(); if (!s.empty()) OpenTaskFile(s[0]); break; }
    case ID_OPENFOLDER: { auto s = SelectedTasks(); if (!s.empty()) OpenTaskFolder(s[0]); break; }
    case IDC_LOGIN: OpenBrowser(L"", false); break;
    case ID_LOGINSITE: {
        auto s = SelectedTasks();
        if (s.empty()) break;
        std::wstring u = s[0]->url;              // open the site's front page: that is where people sign in
        size_t k = u.find(L"://");
        if (k != std::wstring::npos) { size_t e = u.find(L'/', k + 3); if (e != std::wstring::npos) u.resize(e + 1); }
        OpenBrowser(u, false);
        break;
    }
    case ID_COPYURL: {
        std::wstring all;
        for (auto& t : SelectedTasks()) { if (!all.empty()) all += L"\r\n"; all += t->url; }
        if (!all.empty()) SetClipboardText(hwnd, all);
        break;
    }
    case ID_RETRY: {
        auto sel = SelectedTasks();
        {
            std::lock_guard<std::mutex> lk(g_mx);
            for (auto& t : sel) {
                if (IsActive(t->st) || t->st == TS_QUEUED || t->st == TS_EXPANDING) continue;
                t->st = TS_QUEUED; t->pct = 0; t->total = -1; t->speed = -1; t->eta = -1;
                t->err.clear(); t->filepath.clear(); t->cancel = false; t->dirty = true;
                if (t->playlist) t->plIdx = t->plCount = 0;
            }
        }
        if (g_ytdlp.empty()) PromptYtdlp(); else Pump();
        break;
    }
    case ID_CANCELTASK:
        for (auto& t : SelectedTasks()) CancelTask(t);
        break;
    case ID_REMOVE:
        RemoveTasks(SelectedTasks());
        break;
    case ID_DELFILE: {
        // Files of the selected tasks (and any leftover .part) go to the Recycle Bin; the tasks leave the list.
        auto sel = SelectedTasks();
        std::vector<std::wstring> files;
        for (auto& t : sel) {
            std::wstring fp;
            { std::lock_guard<std::mutex> lk(g_mx); fp = t->filepath; }
            if (fp.empty()) continue;
            if (FileExists(fp)) files.push_back(fp);
            if (FileExists(fp + L".part")) files.push_back(fp + L".part");
        }
        if (files.empty()) break;
        int n = 0;
        for (auto& f : files) if (f.size() < 5 || f.compare(f.size() - 5, 5, L".part") != 0) ++n;
        if (!n) n = (int)files.size();
        std::wstring q = T(L"删除 ") + std::to_wstring(n) + T(L" 个文件？");
        if (!Ask(hwnd, q, T(L"文件会移到回收站，这些任务也会从列表中移除。"), T(L"删除"), T(L"取消"), true)) break;
        RemoveTasks(sel);           // stops any that are still running
        Sleep(300);                 // let a stopped yt-dlp release its file
        std::wstring list;
        for (auto& f : files) { list += f; list.push_back(L'\0'); }
        list.push_back(L'\0');
        SHFILEOPSTRUCTW op{};
        op.hwnd = hwnd; op.wFunc = FO_DELETE; op.pFrom = list.c_str();
        op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
        SHFileOperationW(&op);
        g_statusMsg = T(L"已删除 ") + std::to_wstring(n) + T(L" 个文件");
        SetStatusPart(0, g_statusMsg);
        break;
    }
    case ID_CLEARDONE: {
        std::vector<std::shared_ptr<Task>> done;
        {
            std::lock_guard<std::mutex> lk(g_mx);
            for (auto& t : g_tasks) if (t->st == TS_DONE) done.push_back(t);
        }
        RemoveTasks(done);
        break;
    }
    case ID_SELALL:
        ListView_SetItemState(g_hList, -1, LVIS_SELECTED, LVIS_SELECTED);
        break;
    case ID_UPDATE_YTDLP:
    case ID_GET_FFMPEG:
    case ID_GET_YTDLP:
        StartComponent(id);
        break;
    }
}

static LRESULT OnNotify(NMHDR* nh) {
    if (nh->hwndFrom != g_hList) return 0;
    switch (nh->code) {
    case LVN_GETDISPINFOW:
        OnGetDispInfo((NMLVDISPINFOW*)nh);
        return 0;
    case NM_CUSTOMDRAW:
        return OnListCustomDraw((NMLVCUSTOMDRAW*)nh);
    case NM_DBLCLK: {
        auto* ia = (NMITEMACTIVATE*)nh;
        if (ia->iItem >= 0) { auto s = SelectedTasks(); if (!s.empty()) OpenTaskFile(s[0]); }
        return 0;
    }
    case LVN_ITEMCHANGED:
    case LVN_ODSTATECHANGED:
        ShowSelectionInfo();
        return 0;
    case LVN_KEYDOWN: {
        auto* kd = (NMLVKEYDOWN*)nh;
        bool ctrl = GetKeyState(VK_CONTROL) < 0;
        if (kd->wVKey == VK_DELETE) OnCommand(g_hwnd, ID_REMOVE, 0);
        else if (ctrl && kd->wVKey == 'A') OnCommand(g_hwnd, ID_SELALL, 0);
        else if (ctrl && kd->wVKey == 'C') OnCommand(g_hwnd, ID_COPYURL, 0);
        else if (ctrl && kd->wVKey == 'V') AddFromText(ClipboardText(g_hwnd));
        return 0;
    }
    case LVN_GETEMPTYMARKUP: {
        auto* em = (NMLVEMPTYMARKUP*)nh;
        em->dwFlags = EMF_CENTERED;
        lstrcpynW(em->szMarkup, T(L"还没有下载任务。粘贴视频链接后点击「解析并下载」，或在列表中按 Ctrl+V。"), L_MAX_URL_LENGTH);
        return TRUE;
    }
    }
    return 0;
}

static bool ConfirmClose() {
    int active = 0;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        for (auto& t : g_tasks) if (IsActive(t->st)) ++active;
    }
    if (!active) return true;
    std::wstring m = std::wstring(T(L"还有 ")) + std::to_wstring(active) + T(L" 个任务正在下载");
    return Ask(g_hwnd, m, T(L"退出会暂停它们，下次打开后点「开始全部」继续。"), T(L"退出"), T(L"取消"), false);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_TASKBARCREATED && msg == WM_TASKBARCREATED) {
        if (!g_taskbar) CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_taskbar));
        if (g_taskbar && FAILED(g_taskbar->HrInit())) { g_taskbar->Release(); g_taskbar = nullptr; }
        return 0;
    }
    switch (msg) {
    case DM_GETDEFID:
        return MAKELRESULT(IDOK, DC_HASDEFID);
    case WM_CREATE:
        OnCreate(hwnd);
        return 0;
    case WM_SIZE:
        Layout();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = S(720);
        mm->ptMinTrackSize.y = S(460);
        return 0;
    }
    case WM_DPICHANGED:
        OnDpiChanged(hwnd, HIWORD(wp), (const RECT*)lp);
        return 0;
    case WM_THEMECHANGED:
        if (g_progTheme) CloseThemeData(g_progTheme);
        g_progTheme = g_hList ? OpenThemeData(g_hList, L"PROGRESS") : nullptr;
        break;
    case WM_SETTINGCHANGE:
        if (wp == SPI_SETNONCLIENTMETRICS) { CreateUiFont(); ApplyFont(hwnd); Layout(); }
        break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, GetSysColor(COLOR_WINDOW));
        SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_COMMAND:
        OnCommand(hwnd, LOWORD(wp), HIWORD(wp));
        return 0;
    case WM_NOTIFY:
        return OnNotify((NMHDR*)lp);
    case WM_CONTEXTMENU:
        if ((HWND)wp == g_hList) { ShowContextMenu(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); return 0; }
        break;
    case WM_TIMER:
        if (wp == 2) { KillTimer(hwnd, 2); SetWindowTextW(g_hAdd, T(L"解析并下载")); return 0; }
        OnTimer();
        return 0;
    case WM_APP_EXPANDED:
        RefreshListCount();
        if (lp > 1) { g_statusMsg = T(L"列表已展开为 ") + std::to_wstring((int)lp) + T(L" 个下载任务"); SetStatusPart(0, g_statusMsg); }
        if (lp > 0) { int n = TaskCount(); ListView_EnsureVisible(g_hList, n - 1, FALSE); }
        Pump();
        OnTimer();
        ShowSelectionInfo();
        return 0;
    case WM_APP_DONE:
        Pump();
        OnTimer();
        ShowSelectionInfo();
        return 0;
    case WM_APP_TOOLS: {
        std::unique_ptr<std::wstring> v((std::wstring*)lp);
        g_ytdlpVer = *v;
        UpdateToolStatus();
        if (g_hSettingsDlg) ShowCompInfo(g_hSettingsDlg);
        return 0;
    }
    case WM_APP_COMP: {
        std::unique_ptr<std::wstring> m((std::wstring*)lp);
        g_statusMsg = *m;
        SetStatusPart(0, g_statusMsg);
        if (wp != 0) {
            g_compBusy = false;
            DetectTools();
            UpdateToolStatus();
            if (g_hSettingsDlg) ShowCompInfo(g_hSettingsDlg);
            if (g_hSettingsDlg)
                SetDlgItemTextW(g_hSettingsDlg, IDC_COMPINFO, wp == 1 ? T(L"完成") : T(L"失败，详见状态栏"));
            if (wp == 2) Ask(g_hSettingsDlg ? g_hSettingsDlg : hwnd, T(L"操作没有完成"), *m, T(L"确定"), L"", false);
            Pump();
        }
        return 0;
    }
    case WM_CLOSE:
        if (!g_restart && !ConfirmClose()) return 0;
        CommitUiAndSave();
        SaveTasks();                       // unfinished downloads continue next time
        {
            std::lock_guard<std::mutex> lk(g_mx);
            for (auto& t : g_tasks) { t->cancel = true; if (t->job) TerminateJobObject(t->job, 1); }
            for (auto& t : g_pending) t->cancel = true;
        }
        if (g_hBrowser) DestroyWindow(g_hBrowser);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (g_progTheme) CloseThemeData(g_progTheme);
        if (g_taskbar) { g_taskbar->Release(); g_taskbar = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
    g_hInst = hInst;
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    // Ensure Python-based yt-dlp emits UTF-8 and flushes promptly.
    SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");
    SetEnvironmentVariableW(L"PYTHONUNBUFFERED", L"1");
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);
    LoadSettings();

    // Single instance: bring the existing window forward.
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Local\\YtGrab.SingleInstance");
    if (mtx && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND other = FindWindowW(kClass, nullptr)) {
            if (IsIconic(other)) ShowWindow(other, SW_RESTORE);
            SetForegroundWindow(other);
        }
        return 0;
    }

    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarButtonCreated");
    ChangeWindowMessageFilter(WM_TASKBARCREATED, MSGFLT_ADD);

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kClass;
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    RegisterClassExW(&wc);

    // Initial size scaled for the monitor the window will land on.
    POINT pt = { g_set.x == (int)CW_USEDEFAULT ? 0 : g_set.x, g_set.y == (int)CW_USEDEFAULT ? 0 : g_set.y };
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    UINT dx = 96, dy = 96;
    typedef HRESULT(WINAPI* PFN_GDFM)(HMONITOR, int, UINT*, UINT*);
    if (HMODULE sh = LoadLibraryExW(L"shcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))
        if (auto f = (PFN_GDFM)GetProcAddress(sh, "GetDpiForMonitor")) f(mon, 0, &dx, &dy);
    int w = g_set.w > 0 ? g_set.w : MulDiv(1080, dx, 96);
    int h = g_set.h > 0 ? g_set.h : MulDiv(680, dx, 96);
    int x = g_set.w > 0 ? g_set.x : CW_USEDEFAULT;
    int y = g_set.w > 0 ? g_set.y : CW_USEDEFAULT;

    HWND hwnd = CreateWindowExW(0, kClass, T(kAppName), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                x, y, w, h, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, g_set.maximized ? SW_SHOWMAXIMIZED : nShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (m.message == WM_KEYDOWN && m.wParam == 'A' && GetKeyState(VK_CONTROL) < 0 && m.hwnd == g_hUrl) {
            SendMessageW(g_hUrl, EM_SETSEL, 0, -1);
            continue;
        }
        if (m.message == WM_KEYDOWN && m.wParam == VK_TAB && m.hwnd == g_hUrl) {
            // Multiline edit would swallow Tab; move focus like a dialog instead.
            if (HWND next = GetNextDlgTabItem(hwnd, g_hUrl, GetKeyState(VK_SHIFT) < 0)) SetFocus(next);
            continue;
        }
        if (IsDialogMessageW(hwnd, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    if (g_restart) {
        if (mtx) { ReleaseMutex(mtx); CloseHandle(mtx); }
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        STARTUPINFOW si{ sizeof(si) }; PROCESS_INFORMATION pi{};
        if (CreateProcessW(exe, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        }
    }
    // Detached workers may still be unwinding; skip CRT static destructors.
    ExitProcess(0);
}
