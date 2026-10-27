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
    IDC_BROWSE, IDC_PLAYLIST, IDC_SETTINGS, IDC_OPENDIR, IDC_LIST, IDC_STATUS,
    ID_OPENFILE = 300, ID_OPENFOLDER, ID_COPYURL, ID_RETRY, ID_CANCELTASK, ID_REMOVE,
    ID_CLEARDONE, ID_SELALL, ID_UPDATE_YTDLP, ID_GET_FFMPEG, ID_GET_YTDLP
};
#define IDC_ADD IDOK

enum {
    WM_APP_DONE = WM_APP + 1,   // a worker finished
    WM_APP_TOOLS,               // lParam: std::wstring* yt-dlp version
    WM_APP_COMP,                // wParam: 0 progress, 1 ok, 2 fail ; lParam: std::wstring*
    WM_APP_EXPANDED,            // a playlist placeholder was expanded; lParam: number of new tasks
};

static const wchar_t* kAppName  = L"YtGrab 视频下载器";
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
struct Settings {
    std::wstring outDir, proxy, cookies, rate, ytdlpPath;
    int fmt = 0, conc = 3, frag = 4;
    bool playlist = false, subs = false, thumb = false;
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
    g_set.cookies  = IniStr(L"Cookies");
    g_set.rate     = IniStr(L"RateLimit");
    g_set.ytdlpPath= IniStr(L"YtdlpPath");
    g_set.fmt      = IniInt(L"Format", 0);
    g_set.conc     = std::max(1, std::min(10, IniInt(L"Concurrency", 3)));
    g_set.frag     = std::max(1, std::min(16, IniInt(L"Fragments", 4)));
    g_set.playlist = IniInt(L"Playlist", 0) != 0;
    g_set.subs     = IniInt(L"Subtitles", 0) != 0;
    g_set.thumb    = IniInt(L"Thumbnail", 0) != 0;
    g_set.x = IniInt(L"WinX", (int)CW_USEDEFAULT); g_set.y = IniInt(L"WinY", (int)CW_USEDEFAULT);
    g_set.w = IniInt(L"WinW", 0); g_set.h = IniInt(L"WinH", 0);
    g_set.maximized = IniInt(L"WinMax", 0) != 0;
}
static void SaveSettings() {
    IniPut(L"OutDir", g_set.outDir);   IniPut(L"Proxy", g_set.proxy);
    IniPut(L"Cookies", g_set.cookies); IniPut(L"RateLimit", g_set.rate);
    IniPut(L"YtdlpPath", g_set.ytdlpPath);
    IniPut(L"Format", g_set.fmt);      IniPut(L"Concurrency", g_set.conc);
    IniPut(L"Fragments", g_set.frag);  IniPut(L"Playlist", g_set.playlist);
    IniPut(L"Subtitles", g_set.subs);  IniPut(L"Thumbnail", g_set.thumb);
    IniPut(L"WinX", g_set.x); IniPut(L"WinY", g_set.y); IniPut(L"WinW", g_set.w); IniPut(L"WinH", g_set.h);
    IniPut(L"WinMax", g_set.maximized);
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
enum TaskState { TS_EXPANDING, TS_QUEUED, TS_STARTING, TS_DOWNLOADING, TS_PROCESSING, TS_DONE, TS_FAILED, TS_CANCELED };

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
    bool expand = false;      // playlist/channel link: list its videos as separate tasks first
    HANDLE job = nullptr;
    bool cancel = false;
    bool dirty = true;
};

static std::vector<std::shared_ptr<Task>> g_tasks;
static std::mutex g_mx;
static int g_nextId = 1;

static bool IsActive(TaskState s) { return s == TS_STARTING || s == TS_DOWNLOADING || s == TS_PROCESSING; }

// ---------------------------------------------------------------------------
// Globals: UI
// ---------------------------------------------------------------------------
static HINSTANCE g_hInst;
static HWND g_hwnd, g_hUrlLbl, g_hUrl, g_hAdd, g_hPaste, g_hFmtLbl, g_hFmt, g_hDirLbl, g_hDir, g_hBrowse,
            g_hPlaylist, g_hSettings, g_hOpenDir, g_hList, g_hStatus;
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
static std::wstring BuildArgs(const Task& t) {
    bool ff = !g_ffmpeg.empty();
    std::wstring a;
    a += L"--newline --progress --no-simulate --no-colors --encoding utf-8 --no-mtime --windows-filenames";
    a += L" --print " + Q(L"before_dl:[T]%(title)s");
    a += L" --print " + Q(L"after_move:[F]%(filepath)s");
    a += L" --progress-template " + Q(L"download:[P]%(progress.downloaded_bytes)s|%(progress.total_bytes)s|"
                                      L"%(progress.total_bytes_estimate)s|%(progress.speed)s|%(progress.eta)s|"
                                      L"%(info.playlist_index)s|%(info.n_entries)s|%(progress.status)s");
    a += L" -P " + Q(t.outDir);
    a += L" -o " + Q(L"%(title).180B [%(id)s].%(ext)s");
    int f = (t.fmt >= 0 && t.fmt < kFormatCount) ? t.fmt : 0;
    a += L" ";
    a += ff ? g_formats[f].withFF : g_formats[f].noFF;
    a += t.playlist ? L" --yes-playlist" : L" --no-playlist";
    a += L" -N " + std::to_wstring(g_set.frag);
    if (ff) a += L" --ffmpeg-location " + Q(DirOf(g_ffmpeg));
    if (!g_set.proxy.empty())   a += L" --proxy " + Q(g_set.proxy);
    if (!g_set.cookies.empty()) a += L" --cookies-from-browser " + Q(g_set.cookies);
    if (!g_set.rate.empty())    a += L" -r " + Q(g_set.rate);
    if (g_set.subs) {
        a += L" --write-subs --sub-langs " + Q(L"zh.*,en.*");
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
        if (tot > 0 && dl >= 0) t.pct = std::min(100.0, dl * 100.0 / tot);
        if (tot > 0) t.total = tot;
        t.speed = Num(f[3]);
        t.eta = Num(f[4]);
        double idx = Num(f[5]), cnt = Num(f[6]);
        if (idx > 0 && cnt > 0) { t.plIdx = (int)idx; t.plCount = (int)cnt; }
        if (f[7] == L"finished") { t.pct = 100; t.st = TS_PROCESSING; t.speed = -1; t.eta = -1; }
        else if (!t.cancel) t.st = TS_DOWNLOADING;
        t.dirty = true;
    } else if (line.compare(0, 3, L"[T]") == 0) {
        t.title = line.substr(3);
        t.pct = 0; t.total = -1;
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
        t->st = TS_FAILED; t->err = L"无法启动 yt-dlp：" + exe; t->dirty = true;
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
        if (t->cancel) t->st = TS_CANCELED;
        else if (code == 0) { t->st = TS_DONE; t->pct = 100; }
        else {
            t->st = TS_FAILED;
            if (t->err.empty()) t->err = L"yt-dlp 退出代码 " + std::to_wstring((int)code);
        }
        t->speed = -1; t->eta = -1; t->dirty = true;
    }
    if (c.job) CloseHandle(c.job);
    CloseHandle(c.proc);
    CloseHandle(c.out);
    PostMessageW(hwnd, WM_APP_DONE, 0, 0);
}

static void Pump() {
    if (g_ytdlp.empty()) return;
    std::vector<std::shared_ptr<Task>> start;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        int running = 0;
        for (auto& t : g_tasks) if (IsActive(t->st)) ++running;
        for (auto& t : g_tasks) {
            if (running >= g_set.conc) break;
            if (t->st == TS_QUEUED) {
                t->st = TS_STARTING; t->cancel = false; t->err.clear(); t->dirty = true;
                start.push_back(t); ++running;
            }
        }
    }
    for (auto& t : start) std::thread(Worker, t, g_ytdlp, BuildArgs(*t), g_hwnd).detach();
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

// Links that hold several videos: playlists, channels, channel tabs, list= on any site.
static bool LooksLikeList(const std::wstring& u) {
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

struct Entry { std::wstring url, title, list; };

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

static std::wstring CommonArgs() {
    std::wstring a;
    if (!g_set.proxy.empty())   a += L" --proxy " + Q(g_set.proxy);
    if (!g_set.cookies.empty()) a += L" --cookies-from-browser " + Q(g_set.cookies);
    return a;
}

// Lists the entries of a playlist without downloading. Nested lists (a channel's
// Playlists tab, a channel's Videos/Shorts/Live tabs) are opened one level deeper.
static bool ListEntries(const std::wstring& exe, const std::wstring& url, int depth,
                        const std::shared_ptr<Task>& t, std::vector<Entry>& out, std::wstring& err) {
    std::wstring args = L"--flat-playlist --yes-playlist --no-colors --encoding utf-8 --no-warnings"
                        L" --print " + Q(L"[E]%(url)s\t%(title)j\t%(playlist_title)j") + CommonArgs() + L" -- " + Q(url);
    std::wstring text;
    DWORD code = RunCapture(exe, args, text);
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
                if (c != std::wstring::npos) en.list = line.substr(c + 1);
            }
            en.title = JsonStr(en.title);
            en.list = JsonStr(en.list);
            if (en.url.compare(0, 4, L"http") == 0) found.push_back(en);
        } else if (line.compare(0, 6, L"ERROR:") == 0 && err.empty()) {
            err = Trim(line.substr(6));
        }
    }
    if (found.empty()) {
        if (err.empty()) err = code == 0 ? L"列表里没有视频" : L"yt-dlp 退出代码 " + std::to_wstring((int)code);
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
                             std::to_wstring(t.plCount) })
                out += L"\t" + Esc(f);
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
        switch (ln[0]) {
        case L'D': t->st = TS_DONE; t->pct = 100; break;
        case L'F': t->st = TS_FAILED; break;
        case L'C': t->st = TS_CANCELED; break;
        case L'L':
            t->expand = true;
            if (!g_ytdlp.empty()) { t->title.clear(); StartExpand(t); continue; }
            t->st = TS_QUEUED; break;
        default: t->st = TS_QUEUED; break;
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
            PostComp(1, L"yt-dlp 已安装到 " + dir);
        else { DeleteFileW(part.c_str()); PostComp(2, L"yt-dlp 下载失败，请检查网络或代理（使用系统代理设置）。"); }
    } else if (which == ID_GET_FFMPEG) {
        wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
        std::wstring zip = std::wstring(tmp) + L"ytgrab-ffmpeg.zip";
        std::wstring ex  = std::wstring(tmp) + L"ytgrab-ffmpeg";
        if (!Fetch(kFfmpegUrl, zip, L"ffmpeg")) { PostComp(2, L"ffmpeg 下载失败，请检查网络或代理。"); CoUninitialize(); return; }
        PostComp(0, L"正在解压 ffmpeg…");
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
        if (ok) PostComp(1, L"ffmpeg 已安装到 " + dir);
        else    PostComp(2, L"ffmpeg 解压失败（需要 Windows 10 1803 及以上自带的 tar）。");
    } else if (which == ID_UPDATE_YTDLP) {
        if (g_ytdlp.empty()) { CoUninitialize(); ComponentThread(ID_GET_YTDLP); return; }
        PostComp(0, L"正在更新 yt-dlp…");
        std::wstring out;
        DWORD code = RunCapture(g_ytdlp, L"-U", out);
        out = Trim(out);
        size_t k = out.find_last_of(L"\n");
        std::wstring last = k == std::wstring::npos ? out : Trim(out.substr(k + 1));
        PostComp(code == 0 ? 1 : 2, last.empty() ? L"yt-dlp 更新完成" : last);
    }
    CoUninitialize();
}

static void StartComponent(int which) {
    if (g_compBusy) { MessageBeep(MB_ICONWARNING); return; }
    g_compBusy = true;
    std::thread(ComponentThread, which).detach();
}

// ---------------------------------------------------------------------------
// UI helpers
// ---------------------------------------------------------------------------
static void SetStatusPart(int part, const std::wstring& s) {
    SendMessageW(g_hStatus, SB_SETTEXTW, part, (LPARAM)s.c_str());
}

static void UpdateToolStatus() {
    SetStatusPart(1, g_ytdlp.empty() ? L"yt-dlp：未找到" : (L"yt-dlp " + (g_ytdlpVer.empty() ? std::wstring(L"…") : g_ytdlpVer)));
    SetStatusPart(2, g_ffmpeg.empty() ? L"ffmpeg：未找到" : L"ffmpeg：就绪");
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

static void UpdateColumns() {
    for (int i = 0; i < kCols; ++i) ListView_SetColumnWidth(g_hList, i, S(kColW[i]));
}

static void Layout() {
    if (!g_hList) return;
    RECT rc; GetClientRect(g_hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    SendMessageW(g_hStatus, WM_SIZE, 0, 0);
    RECT sr; GetWindowRect(g_hStatus, &sr);
    int sh = sr.bottom - sr.top;
    int parts[5] = { W - S(470), W - S(330), W - S(220), W - S(100), -1 };
    for (int& p : parts) if (p != -1) p = std::max(p, S(40));
    SendMessageW(g_hStatus, SB_SETPARTS, 5, (LPARAM)parts);

    RECT cr; GetWindowRect(g_hFmt, &cr);
    int ch = cr.bottom - cr.top;                 // closed combo height drives row height
    int m = S(14), g = S(8), bw = S(112), bh = std::max(ch + S(2), S(30)), lh = S(20);
    HDWP dw = BeginDeferWindowPos(16);
    auto mv = [&](HWND h, int x, int y, int w, int hh) { dw = DeferWindowPos(dw, h, nullptr, x, y, w, hh, SWP_NOZORDER | SWP_NOACTIVATE); };

    int y = m;
    mv(g_hUrlLbl, m, y, W - 2 * m, lh);
    y += lh + S(4);
    int eh = bh * 2 + g;
    mv(g_hUrl, m, y, W - 2 * m - bw - g, eh);
    mv(g_hAdd, W - m - bw, y, bw, bh);
    mv(g_hPaste, W - m - bw, y + eh - bh, bw, bh);
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

    mv(g_hPlaylist, m, y + (bh - lh) / 2, S(240), lh);
    mv(g_hSettings, W - m - bw, y, bw, bh);
    mv(g_hOpenDir, W - m - 2 * bw - g, y, bw, bh);
    y += bh + S(12);

    mv(g_hList, m, y, W - 2 * m, std::max(S(40), H - sh - y - S(10)));
    EndDeferWindowPos(dw);
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
    g_set.playlist = Button_GetCheck(g_hPlaylist) == BST_CHECKED;
}

static int TaskCount() { std::lock_guard<std::mutex> lk(g_mx); return (int)g_tasks.size(); }

static bool g_saveDue2 = false;
static void RefreshListCount() {
    g_saveDue2 = true;
    ListView_SetItemCountEx(g_hList, TaskCount(), LVSICF_NOSCROLL);
    InvalidateRect(g_hList, nullptr, FALSE);
}

static void PromptYtdlp() {
    if (g_compBusy) return;
    int r = MessageBoxW(g_hwnd, L"没有找到 yt-dlp.exe。\n\n是否现在从 GitHub 自动下载最新版（约 18 MB）？\n"
                                L"也可以手动把 yt-dlp.exe 放在本程序同一目录下。",
                        kAppName, MB_YESNO | MB_ICONQUESTION);
    if (r == IDYES) StartComponent(ID_GET_YTDLP);
}

static void AddFromText(const std::wstring& text) {
    auto urls = ExtractUrls(text);
    if (urls.empty()) {
        MessageBeep(MB_ICONWARNING);
        g_statusMsg = L"没有识别到 http/https 链接";
        SetStatusPart(0, g_statusMsg);
        return;
    }
    ReadUiSettings();
    int last;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        for (auto& u : urls) {
            auto t = std::make_shared<Task>();
            t->id = g_nextId++;
            t->url = u; t->outDir = g_set.outDir; t->fmt = g_set.fmt; t->playlist = g_set.playlist;
            t->expand = g_set.playlist && LooksLikeList(u) && !g_ytdlp.empty();
            if (t->expand) StartExpand(t);          // kept out of the list until its videos are known
            else g_tasks.push_back(t);
        }
        last = (int)g_tasks.size() - 1;
    }
    RefreshListCount();
    if (last >= 0) ListView_EnsureVisible(g_hList, last, FALSE);
    g_statusMsg = L"已添加 " + std::to_wstring(urls.size()) + L" 个链接";
    SetStatusPart(0, g_statusMsg);
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
    if (t->st == TS_QUEUED) { t->st = TS_CANCELED; t->dirty = true; return; }
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
    case TS_EXPANDING: return L"读取列表…";
    case TS_QUEUED: return L"等待中";
    case TS_STARTING: return L"解析中…";
    case TS_DOWNLOADING: return L"下载中";
    case TS_PROCESSING: return L"处理中…";
    case TS_DONE: return L"已完成";
    case TS_FAILED: return L"失败";
    case TS_CANCELED: return L"已取消";
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
            if (t.st == TS_FAILED) s = t.err;
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
    int state = (st == TS_FAILED) ? PBFS_ERROR : (st == TS_CANCELED) ? PBFS_PAUSED : PBFS_NORMAL;
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
        if (t.st == TS_FAILED && !t.err.empty()) m = L"错误：" + t.err;
        else if (!t.filepath.empty()) m = t.filepath;
        else m = t.url;
    } else if (sel.size() > 1) {
        m = L"已选择 " + std::to_wstring(sel.size()) + L" 项";
    }
    SetStatusPart(0, m);
}

static void OnTimer() {
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
    std::wstring cnt = L"下载中 " + std::to_wstring(active) + L" · 等待 " + std::to_wstring(queued);
    if (reading) cnt = L"读取列表 " + std::to_wstring(reading) + L" · " + cnt;
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
            if (IsActive(t->st) || t->st == TS_QUEUED || t->st == TS_EXPANDING) canCancel = true;
            if (t->st == TS_FAILED || t->st == TS_CANCELED || t->st == TS_DONE) canRetry = true;
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
    AppendMenuW(m, (any && hasFile) ? on : off, ID_OPENFILE, L"打开文件\tEnter");
    AppendMenuW(m, any ? on : off, ID_OPENFOLDER, L"打开所在文件夹");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, canRetry ? on : off, ID_RETRY, L"重新下载");
    AppendMenuW(m, canCancel ? on : off, ID_CANCELTASK, L"取消");
    AppendMenuW(m, any ? on : off, ID_COPYURL, L"复制链接");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, any ? on : off, ID_REMOVE, L"移除\tDel");
    AppendMenuW(m, on, ID_CLEARDONE, L"清除已完成");
    AppendMenuW(m, on, ID_SELALL, L"全选\tCtrl+A");
    TrackPopupMenuEx(m, TPM_RIGHTBUTTON, x, y, g_hwnd, nullptr);
    DestroyMenu(m);
}

// ---------------------------------------------------------------------------
// Settings dialog
// ---------------------------------------------------------------------------
static const wchar_t* kBrowsers[] = { L"不使用", L"firefox", L"chrome", L"edge", L"brave", L"opera", L"vivaldi", L"chromium" };

static INT_PTR CALLBACK SettingsProc(HWND d, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        g_hSettingsDlg = d;
        HWND c = GetDlgItem(d, IDC_CONC);
        for (int i = 1; i <= 10; ++i) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)std::to_wstring(i).c_str());
        SendMessageW(c, CB_SETCURSEL, g_set.conc - 1, 0);
        c = GetDlgItem(d, IDC_FRAG);
        for (int i = 1; i <= 16; ++i) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)std::to_wstring(i).c_str());
        SendMessageW(c, CB_SETCURSEL, g_set.frag - 1, 0);
        c = GetDlgItem(d, IDC_COOKIES);
        int sel = 0;
        for (int i = 0; i < (int)(sizeof(kBrowsers) / sizeof(*kBrowsers)); ++i) {
            SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)kBrowsers[i]);
            if (i && g_set.cookies == kBrowsers[i]) sel = i;
        }
        SendMessageW(c, CB_SETCURSEL, sel, 0);
        SetDlgItemTextW(d, IDC_RATE, g_set.rate.c_str());
        SetDlgItemTextW(d, IDC_PROXY, g_set.proxy.c_str());
        SetDlgItemTextW(d, IDC_YTPATH, g_set.ytdlpPath.c_str());
        SendDlgItemMessageW(d, IDC_RATE, EM_SETCUEBANNER, TRUE, (LPARAM)L"例如 5M，留空不限速");
        SendDlgItemMessageW(d, IDC_PROXY, EM_SETCUEBANNER, TRUE, (LPARAM)L"例如 http://127.0.0.1:7890");
        SendDlgItemMessageW(d, IDC_YTPATH, EM_SETCUEBANNER, TRUE, (LPARAM)L"留空自动查找");
        CheckDlgButton(d, IDC_SUBS, g_set.subs ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(d, IDC_THUMB, g_set.thumb ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemTextW(d, IDC_COMPINFO, g_ffmpeg.empty() ? L"ffmpeg 未安装（合并高清需要）" : L"ffmpeg 已就绪");
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_YTBROWSE: {
            wchar_t file[MAX_PATH] = L"";
            OPENFILENAMEW of{}; of.lStructSize = sizeof(of); of.hwndOwner = d;
            of.lpstrFilter = L"yt-dlp (*.exe)\0*.exe\0"; of.lpstrFile = file; of.nMaxFile = MAX_PATH;
            of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
            if (GetOpenFileNameW(&of)) SetDlgItemTextW(d, IDC_YTPATH, file);
            return TRUE;
        }
        case IDC_UPDATE:
            StartComponent(ID_UPDATE_YTDLP);
            SetDlgItemTextW(d, IDC_COMPINFO, L"进度见主窗口状态栏");
            return TRUE;
        case IDC_GETFF:
            StartComponent(ID_GET_FFMPEG);
            SetDlgItemTextW(d, IDC_COMPINFO, L"约 200 MB，进度见状态栏");
            return TRUE;
        case IDOK: {
            g_set.conc = (int)SendDlgItemMessageW(d, IDC_CONC, CB_GETCURSEL, 0, 0) + 1;
            g_set.frag = (int)SendDlgItemMessageW(d, IDC_FRAG, CB_GETCURSEL, 0, 0) + 1;
            int ck = (int)SendDlgItemMessageW(d, IDC_COOKIES, CB_GETCURSEL, 0, 0);
            g_set.cookies = ck > 0 ? kBrowsers[ck] : L"";
            wchar_t b[2048];
            GetDlgItemTextW(d, IDC_RATE, b, 2048);   g_set.rate = Trim(b);
            GetDlgItemTextW(d, IDC_PROXY, b, 2048);  g_set.proxy = Trim(b);
            GetDlgItemTextW(d, IDC_YTPATH, b, 2048); g_set.ytdlpPath = Trim(b);
            g_set.subs = IsDlgButtonChecked(d, IDC_SUBS) == BST_CHECKED;
            g_set.thumb = IsDlgButtonChecked(d, IDC_THUMB) == BST_CHECKED;
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

static void OnCreate(HWND hwnd) {
    g_hwnd = hwnd;
    g_dpi = GetDpiForWindow(hwnd);
    CreateUiFont();

    g_hUrlLbl  = MakeCtl(WC_STATICW, L"视频链接（支持一次粘贴多个，按 Enter 添加）", SS_LEFT | SS_NOPREFIX, IDC_URLLBL);
    g_hUrl     = MakeCtl(WC_EDITW, L"", WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, IDC_URL, WS_EX_CLIENTEDGE);
    g_hAdd     = MakeCtl(WC_BUTTONW, L"添加下载", WS_TABSTOP | BS_DEFPUSHBUTTON, IDC_ADD);
    g_hPaste   = MakeCtl(WC_BUTTONW, L"粘贴并下载", WS_TABSTOP | BS_PUSHBUTTON, IDC_PASTE);
    g_hFmtLbl  = MakeCtl(WC_STATICW, L"格式", SS_LEFT, IDC_FMTLBL);
    g_hFmt     = MakeCtl(WC_COMBOBOXW, L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, IDC_FMT);
    g_hDirLbl  = MakeCtl(WC_STATICW, L"保存到", SS_LEFT, IDC_DIRLBL);
    g_hDir     = MakeCtl(WC_EDITW, g_set.outDir.c_str(), WS_TABSTOP | ES_AUTOHSCROLL, IDC_DIR, WS_EX_CLIENTEDGE);
    g_hBrowse  = MakeCtl(WC_BUTTONW, L"浏览…", WS_TABSTOP | BS_PUSHBUTTON, IDC_BROWSE);
    g_hPlaylist= MakeCtl(WC_BUTTONW, L"下载整个播放列表", WS_TABSTOP | BS_AUTOCHECKBOX, IDC_PLAYLIST);
    g_hOpenDir = MakeCtl(WC_BUTTONW, L"打开文件夹", WS_TABSTOP | BS_PUSHBUTTON, IDC_OPENDIR);
    g_hSettings= MakeCtl(WC_BUTTONW, L"设置…", WS_TABSTOP | BS_PUSHBUTTON, IDC_SETTINGS);
    g_hList    = MakeCtl(WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS, IDC_LIST, WS_EX_CLIENTEDGE);
    g_hStatus  = MakeCtl(STATUSCLASSNAMEW, L"", SBARS_SIZEGRIP, IDC_STATUS);

    for (int i = 0; i < kFormatCount; ++i) SendMessageW(g_hFmt, CB_ADDSTRING, 0, (LPARAM)g_formats[i].name);
    SendMessageW(g_hFmt, CB_SETCURSEL, (g_set.fmt >= 0 && g_set.fmt < kFormatCount) ? g_set.fmt : 0, 0);
    Button_SetCheck(g_hPlaylist, g_set.playlist ? BST_CHECKED : BST_UNCHECKED);
    SendMessageW(g_hDir, EM_SETCUEBANNER, TRUE, (LPARAM)L"选择保存目录");

    SetWindowTheme(g_hList, L"Explorer", nullptr);
    ListView_SetExtendedListViewStyle(g_hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP | LVS_EX_HEADERDRAGDROP);
    for (int i = 0; i < kCols; ++i) {
        LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        c.fmt = (i >= 3 && i <= 5) ? LVCFMT_RIGHT : LVCFMT_LEFT;
        c.cx = S(kColW[i]); c.pszText = (LPWSTR)kColName[i];
        ListView_InsertColumn(g_hList, i, &c);
    }
    UpdateRowHeight();
    g_progTheme = OpenThemeData(g_hList, L"PROGRESS");

    ApplyFont(hwnd);
    Layout();
    DetectTools();
    UpdateToolStatus();
    SetStatusPart(0, L"就绪");
    LoadTasks();
    RefreshListCount();
    { int n = TaskCount(); if (n) { g_statusMsg = L"已恢复 " + std::to_wstring(n) + L" 个任务"; SetStatusPart(0, g_statusMsg); } }
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
        if (DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_SETTINGS), hwnd, SettingsProc, 0) == IDOK) {
            DetectTools(); UpdateToolStatus(); Pump();
        }
        break;
    case IDC_FMT:
    case IDC_PLAYLIST:
        if (code == CBN_SELCHANGE || code == BN_CLICKED) ReadUiSettings();
        break;
    case ID_OPENFILE:   { auto s = SelectedTasks(); if (!s.empty()) OpenTaskFile(s[0]); break; }
    case ID_OPENFOLDER: { auto s = SelectedTasks(); if (!s.empty()) OpenTaskFolder(s[0]); break; }
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
        lstrcpynW(em->szMarkup, L"还没有下载任务。复制视频链接后点击“粘贴并下载”，或在列表中按 Ctrl+V。", L_MAX_URL_LENGTH);
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
    std::wstring m = L"还有 " + std::to_wstring(active) + L" 个任务正在下载，退出将暂停它们，下次打开会自动继续。确定退出吗？";
    return MessageBoxW(g_hwnd, m.c_str(), kAppName, MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2) == IDOK;
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
        OnTimer();
        return 0;
    case WM_APP_EXPANDED:
        RefreshListCount();
        if (lp > 1) { g_statusMsg = L"列表已展开为 " + std::to_wstring((int)lp) + L" 个下载任务"; SetStatusPart(0, g_statusMsg); }
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
            if (g_hSettingsDlg)
                SetDlgItemTextW(g_hSettingsDlg, IDC_COMPINFO, wp == 1 ? L"完成" : L"失败，详见状态栏");
            if (wp == 2) MessageBoxW(g_hSettingsDlg ? g_hSettingsDlg : hwnd, m->c_str(), kAppName, MB_OK | MB_ICONWARNING);
            Pump();
        }
        return 0;
    }
    case WM_CLOSE:
        if (!ConfirmClose()) return 0;
        CommitUiAndSave();
        SaveTasks();                       // unfinished downloads continue next time
        {
            std::lock_guard<std::mutex> lk(g_mx);
            for (auto& t : g_tasks) { t->cancel = true; if (t->job) TerminateJobObject(t->job, 1); }
            for (auto& t : g_pending) t->cancel = true;
        }
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

    HWND hwnd = CreateWindowExW(0, kClass, kAppName, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
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
    // Detached workers may still be unwinding; skip CRT static destructors.
    ExitProcess(0);
}
