// Built-in browser (WebView2): sign in to any site once, YtGrab hands the cookies to yt-dlp.
// On Xiaohongshu it also collects every note of a creator's home page (xhs.js) and downloads
// the media directly. Included once from main.cpp (single translation unit).
#pragma once
#include "WebView2.h"

// ---------------------------------------------------------------------------
// COM callback helper (no WRL with MinGW)
// ---------------------------------------------------------------------------
template<class I, class M> struct WvHandlerImpl;
template<class I, class... A> struct WvHandlerImpl<I, HRESULT (I::*)(A...)> : I {
    std::function<HRESULT(A...)> f; LONG rc = 1;
    explicit WvHandlerImpl(std::function<HRESULT(A...)> g) : f(std::move(g)) {}
    virtual ~WvHandlerImpl() {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** p) override {
        if (IsEqualIID(riid, IID_IMarshal)) { *p = nullptr; return E_NOINTERFACE; }
        *p = this; AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&rc); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&rc); if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(A... a) override { return f(a...); }
};
template<class I> using WvH = WvHandlerImpl<I, decltype(&I::Invoke)>;
template<class I, class F> static I* MakeH(F f) { return new WvH<I>(f); }

static const IID kIID_ICoreWebView2_2 = { 0x9E8F0CF8, 0xE670, 0x4B5E, { 0xB2, 0xBC, 0x73, 0xE0, 0x61, 0xE3, 0x18, 0x4C } };

enum { IDC_B_ADDR = 400, IDC_B_GO, IDC_B_CRAWL, IDC_B_STATUS };
static const wchar_t* kBrowserClass = L"YtGrabBrowserWnd";
static HWND g_hBrowser, g_hBAddr, g_hBGo, g_hBCrawl, g_hBStatus;
static ICoreWebView2Environment* g_wvEnv;
static ICoreWebView2Controller* g_wvCtl;
static ICoreWebView2* g_wv;
static bool g_wvFailed = false;
static std::wstring g_wvStartUrl;
static std::vector<std::wstring> g_crawlQueue;
static bool g_crawling = false, g_crawlPending = false;
static int g_crawlAdded = 0;

static std::wstring CookieFile() { return KnownFolder(FOLDERID_LocalAppData) + L"\\YtGrab\\cookies.txt"; }
static std::wstring WebDataDir() { return KnownFolder(FOLDERID_LocalAppData) + L"\\YtGrab\\WebView2"; }

static bool IsXhs(const std::wstring& u) {
    std::wstring l = Lower(u);
    return Has(l, L"xiaohongshu.com/") || Has(l, L"xhslink.com/") || Has(l, L"xhslink.cn/");
}

static void BStatus(const std::wstring& s) { if (g_hBStatus) SetWindowTextW(g_hBStatus, s.c_str()); }
static void BCrawlButton() {
    if (g_hBCrawl) SetWindowTextW(g_hBCrawl, g_crawling ? T(L"停止抓取") : T(L"抓取本页全部"));
}

// ---- cookies → Netscape cookies.txt (read by yt-dlp --cookies)
static void ExportCookies() {
    if (!g_wv) return;
    ICoreWebView2_2* w2 = nullptr;
    if (FAILED(g_wv->QueryInterface(kIID_ICoreWebView2_2, (void**)&w2)) || !w2) return;
    ICoreWebView2CookieManager* cm = nullptr;
    if (SUCCEEDED(w2->get_CookieManager(&cm)) && cm) {
        auto h = MakeH<ICoreWebView2GetCookiesCompletedHandler>([](HRESULT hr, ICoreWebView2CookieList* list) -> HRESULT {
            if (FAILED(hr) || !list) return S_OK;
            UINT n = 0; list->get_Count(&n);
            std::wstring out = L"# Netscape HTTP Cookie File\n# Exported by YtGrab from its built-in browser\n\n";
            auto str = [](LPWSTR p) { std::wstring s = p ? p : L""; if (p) CoTaskMemFree(p); return s; };
            for (UINT i = 0; i < n; ++i) {
                ICoreWebView2Cookie* c = nullptr;
                if (FAILED(list->GetValueAtIndex(i, &c)) || !c) continue;
                LPWSTR p = nullptr;
                c->get_Name(&p);   std::wstring name = str(p); p = nullptr;
                c->get_Value(&p);  std::wstring value = str(p); p = nullptr;
                c->get_Domain(&p); std::wstring dom = str(p); p = nullptr;
                c->get_Path(&p);   std::wstring path = str(p);
                double exp = 0; BOOL http = FALSE, sec = FALSE, sess = FALSE;
                c->get_Expires(&exp); c->get_IsHttpOnly(&http); c->get_IsSecure(&sec); c->get_IsSession(&sess);
                c->Release();
                if (name.empty() || dom.empty()) continue;
                if (path.empty()) path = L"/";
                out += (http ? L"#HttpOnly_" : L"") + dom + L"\t" + (dom[0] == L'.' ? L"TRUE" : L"FALSE") + L"\t" + path +
                       L"\t" + (sec ? L"TRUE" : L"FALSE") + L"\t" + std::to_wstring(sess || exp <= 0 ? 0LL : (long long)exp) +
                       L"\t" + name + L"\t" + value + L"\n";
            }
            std::wstring f = CookieFile();
            SHCreateDirectoryExW(nullptr, DirOf(f).c_str(), nullptr);
            std::wstring tmp = f + L".tmp";
            HANDLE fh = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (fh == INVALID_HANDLE_VALUE) return S_OK;
            std::string u = ToUtf8(out); DWORD wr = 0;
            BOOL ok = WriteFile(fh, u.data(), (DWORD)u.size(), &wr, nullptr);
            CloseHandle(fh);
            if (ok) MoveFileExW(tmp.c_str(), f.c_str(), MOVEFILE_REPLACE_EXISTING); else DeleteFileW(tmp.c_str());
            return S_OK;
        });
        cm->GetCookies(L"", h);
        h->Release();
        cm->Release();
    }
    w2->Release();
}

// yt-dlp arguments for cookies. The built-in browser's cookies are copied per run, since yt-dlp
// writes its cookie file back when it exits and several downloads run at once.
static std::wstring CookieArgs(std::wstring* tmpOut) {
    if (g_set.cookies.empty()) return {};
    if (g_set.cookies != L"builtin") return L" --cookies-from-browser " + Q(g_set.cookies);
    std::wstring f = CookieFile();
    if (!FileExists(f)) return {};
    wchar_t tp[MAX_PATH]; GetTempPathW(MAX_PATH, tp);
    static LONG seq = 0;
    std::wstring tmp = std::wstring(tp) + L"YtGrab-cookies-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                       std::to_wstring(InterlockedIncrement(&seq)) + L".txt";
    if (!CopyFileW(f.c_str(), tmp.c_str(), FALSE)) return L" --cookies " + Q(f);
    if (tmpOut) *tmpOut = tmp;
    return L" --cookies " + Q(tmp);
}

// ---- Xiaohongshu media tasks (direct download, no yt-dlp)
static void AddMediaTask(const std::wstring& id, const std::wstring& title, const std::wstring& owner,
                         const std::wstring& page, const std::vector<std::wstring>& urls) {
    ReadUiSettings();
    {
        std::lock_guard<std::mutex> lk(g_mx);
        for (auto& x : g_tasks)
            if (x->mediaId == id && x->st != TS_FAILED && x->st != TS_CANCELED) return;   // already in the list
        auto t = std::make_shared<Task>();
        t->id = g_nextId++;
        t->url = page; t->title = title; t->mediaId = id; t->media = urls;
        t->outDir = g_set.outDir;
        std::wstring f = SafeFolder(owner);
        if (!f.empty()) t->outDir += L"\\" + f;
        t->fmt = g_set.fmt;
        g_tasks.push_back(t);
    }
    ++g_crawlAdded;
    g_saveDue = true;
    RefreshListCount();
    Pump();
}

static std::wstring ExtFor(const std::wstring& type, const std::wstring& url) {
    std::wstring t = Lower(type);
    if (Has(t, L"video/mp4")) return L"mp4";
    if (Has(t, L"video/quicktime")) return L"mov";
    if (Has(t, L"video/")) return L"mp4";
    if (Has(t, L"image/jpeg") || Has(t, L"image/jpg")) return L"jpg";
    if (Has(t, L"image/png")) return L"png";
    if (Has(t, L"image/webp")) return L"webp";
    if (Has(t, L"image/heic")) return L"heic";
    if (Has(t, L"image/gif")) return L"gif";
    if (Has(t, L"image/avif")) return L"avif";
    std::wstring l = Lower(url);
    if (Has(l, L".mp4")) return L"mp4";
    if (Has(l, L"webp")) return L"webp";
    if (Has(l, L"png")) return L"png";
    return L"jpg";
}

static void DirectWorker(std::shared_ptr<Task> t, HWND hwnd) {
    SHCreateDirectoryExW(nullptr, t->outDir.c_str(), nullptr);
    std::wstring proxy;
    if (!g_set.proxy.empty()) {
        std::wstring p = g_set.proxy, l = Lower(p);
        bool socks = Has(l, L"socks");
        size_t k = p.find(L"://"); if (k != std::wstring::npos) p = p.substr(k + 3);
        while (!p.empty() && p.back() == L'/') p.pop_back();
        proxy = socks ? L"socks=" + p : L"http=http://" + p + L" https=http://" + p;
    }
    HINTERNET net = InternetOpenW(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                                  L"Chrome/130.0.0.0 Safari/537.36 Edg/130.0.0.0",
                                  proxy.empty() ? INTERNET_OPEN_TYPE_PRECONFIG : INTERNET_OPEN_TYPE_PROXY,
                                  proxy.empty() ? nullptr : proxy.c_str(), nullptr, 0);
    std::vector<std::wstring> urls; std::wstring base, err;
    {
        std::lock_guard<std::mutex> lk(g_mx);
        urls = t->media;
        std::wstring name = SafeFolder(t->title);
        if (name.size() > 80) name.resize(80);
        base = (name.empty() ? std::wstring() : name + L" ") + L"[" + t->mediaId + L"]";
        t->st = TS_DOWNLOADING; t->pct = 0; t->plCount = 0; t->plIdx = 0; t->dirty = true;
    }
    int n = (int)urls.size(), okCount = 0;
    double doneBytes = 0;
    bool canceled = false;
    for (int i = 0; i < n && !canceled; ++i) {
        { std::lock_guard<std::mutex> lk(g_mx); if (t->cancel) { canceled = true; break; } }
        HINTERNET u = net ? InternetOpenUrlW(net, urls[i].c_str(), L"Referer: https://www.xiaohongshu.com/\r\n", (DWORD)-1,
                                             INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES, 0) : nullptr;
        if (!u) { err = T(L"无法连接：") + urls[i]; continue; }
        DWORD code = 0, sz = sizeof(code);
        HttpQueryInfoW(u, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &code, &sz, nullptr);
        if (code >= 400) { err = L"HTTP " + std::to_wstring(code); InternetCloseHandle(u); continue; }
        wchar_t buf[256] = L""; sz = sizeof(buf);
        std::wstring type = HttpQueryInfoW(u, HTTP_QUERY_CONTENT_TYPE, buf, &sz, nullptr) ? buf : L"";
        wchar_t lb[64] = L""; sz = sizeof(lb);
        double len = HttpQueryInfoW(u, HTTP_QUERY_CONTENT_LENGTH, lb, &sz, nullptr) ? _wtof(lb) : -1;
        std::wstring file = t->outDir + L"\\" + base + (n > 1 ? L"_" + std::to_wstring(i + 1) : L"") + L"." + ExtFor(type, urls[i]);
        WIN32_FILE_ATTRIBUTE_DATA fa{};
        if (GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &fa) &&
            (len <= 0 || (((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow) == (ULONGLONG)len)) {
            InternetCloseHandle(u);
            ++okCount;
            std::lock_guard<std::mutex> lk(g_mx);
            if (t->filepath.empty()) t->filepath = file;
            t->pct = (i + 1) * 100.0 / n; t->dirty = true;
            continue;
        }
        std::wstring part = file + L".part";
        HANDLE fh = CreateFileW(part.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (fh == INVALID_HANDLE_VALUE) { err = T(L"无法写入文件：") + file; InternetCloseHandle(u); continue; }
        std::vector<char> data(1 << 16);
        double got = 0, lastGot = 0; DWORD lastTick = GetTickCount(), rd = 0;
        bool ok = true;
        while (true) {
            if (!InternetReadFile(u, data.data(), (DWORD)data.size(), &rd)) { ok = false; err = T(L"下载中断"); break; }
            if (!rd) break;
            DWORD wr = 0;
            if (!WriteFile(fh, data.data(), rd, &wr, nullptr) || wr != rd) { ok = false; err = T(L"无法写入文件：") + file; break; }
            got += rd;
            DWORD now = GetTickCount();
            std::lock_guard<std::mutex> lk(g_mx);
            if (t->cancel) { canceled = true; ok = false; break; }
            double frac = len > 0 ? std::min(1.0, got / len) : 0.5;
            t->pct = (i + frac) * 100.0 / n;
            t->total = doneBytes + (len > 0 ? len : got);
            if (n > 1) { t->plIdx = i + 1; t->plCount = n; }
            if (now - lastTick >= 500) {
                t->speed = (got - lastGot) * 1000.0 / (now - lastTick);
                t->eta = (len > 0 && t->speed > 0) ? (len - got) / t->speed : -1;
                lastGot = got; lastTick = now;
            }
            t->dirty = true;
        }
        CloseHandle(fh);
        InternetCloseHandle(u);
        if (ok && (len <= 0 || got >= len)) {
            MoveFileExW(part.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING);
            doneBytes += got; ++okCount;
            std::lock_guard<std::mutex> lk(g_mx);
            if (t->filepath.empty()) t->filepath = file;
        } else {
            DeleteFileW(part.c_str());
            if (ok) err = T(L"下载不完整");
        }
    }
    if (net) InternetCloseHandle(net);
    {
        std::lock_guard<std::mutex> lk(g_mx);
        if (t->cancel) { t->st = t->pause ? TS_PAUSED : TS_CANCELED; t->pause = false; }
        else if (okCount == n) { t->st = TS_DONE; t->pct = 100; t->err.clear(); if (doneBytes > 0) t->total = doneBytes; }
        else {
            t->st = TS_FAILED;
            t->err = (okCount ? std::to_wstring(okCount) + L"/" + std::to_wstring(n) + L" " : std::wstring()) + err;
        }
        t->speed = -1; t->eta = -1; t->dirty = true;
    }
    PostMessageW(hwnd, WM_APP_DONE, 0, 0);
}

// ---- crawl queue
static void BNavigate(const std::wstring& url) {
    if (!g_wv || url.empty()) return;
    std::wstring u = url;
    if (u.find(L"://") == std::wstring::npos) u = L"https://" + u;
    g_wv->Navigate(u.c_str());
    SetWindowTextW(g_hBAddr, u.c_str());
}

static void NextCrawl() {
    if (g_crawlQueue.empty()) {
        g_crawling = false; g_crawlPending = false;
        BCrawlButton();
        return;
    }
    std::wstring u = g_crawlQueue.front();
    g_crawlQueue.erase(g_crawlQueue.begin());
    g_crawling = true; g_crawlPending = true; g_crawlAdded = 0;
    BCrawlButton();
    BStatus(T(L"正在打开：") + u);
    BNavigate(u);
}

static std::wstring CurrentUrl() {
    std::wstring s;
    if (!g_wv) return s;
    LPWSTR p = nullptr;
    if (SUCCEEDED(g_wv->get_Source(&p)) && p) { s = p; CoTaskMemFree(p); }
    return s;
}

static void StartCrawlHere() {
    if (!g_wv) return;
    g_crawling = true; g_crawlPending = false; g_crawlAdded = 0;
    BCrawlButton();
    g_wv->ExecuteScript(L"window.__ytgrabGo&&window.__ytgrabGo()", nullptr);
}

static std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep) {
    std::vector<std::wstring> r; size_t a = 0, b;
    while ((b = s.find(sep, a)) != std::wstring::npos) { r.push_back(s.substr(a, b - a)); a = b + 1; }
    r.push_back(s.substr(a));
    return r;
}

static void OnPageMessage(const std::wstring& msg) {
    auto f = Split(msg, 0x1f);
    if (f.empty()) return;
    const std::wstring& k = f[0];
    auto at = [&](size_t i) { return i < f.size() ? f[i] : std::wstring(); };
    if (k == L"s") {
        std::wstring c = at(1);
        if (c == L"open") BStatus(T(L"正在打开主页…"));
        else if (c == L"login") BStatus(T(L"请先在这个窗口登录小红书，登录后会自动继续"));
        else if (c == L"verify") BStatus(T(L"小红书要求验证，暂停 60 秒后重试…"));
        else if (c == L"scroll") BStatus(T(L"正在浏览主页，已找到 ") + at(2) + T(L" 条笔记") + (at(3).empty() ? L"" : L" · " + at(3)));
        else if (c == L"read") BStatus(T(L"正在读取笔记 ") + at(2) + L" / " + at(3) + T(L"，已加入下载 ") + at(4));
        else if (c == L"notpage") BStatus(T(L"这个页面不是小红书主页或笔记"));
        else if (c == L"error") BStatus(T(L"出错：") + at(2));
    } else if (k == L"login") {
        if (IsIconic(g_hBrowser)) ShowWindow(g_hBrowser, SW_RESTORE);
        SetForegroundWindow(g_hBrowser);
    } else if (k == L"list") {
        BStatus(T(L"共 ") + at(1) + T(L" 条笔记，开始逐条读取…"));
    } else if (k == L"note") {
        // note, id, title, owner, kind, page, urls
        auto urls = Split(at(6), 0x1e);
        urls.erase(std::remove(urls.begin(), urls.end(), std::wstring()), urls.end());
        if (!at(1).empty() && !urls.empty()) AddMediaTask(at(1), at(2), at(3), at(5), urls);
    } else if (k == L"done") {
        ExportCookies();
        BStatus(T(L"完成，本次加入下载 ") + std::to_wstring(g_crawlAdded) + T(L" 条"));
        g_statusMsg = T(L"小红书：已加入 ") + std::to_wstring(g_crawlAdded) + T(L" 个下载任务");
        SetStatusPart(0, g_statusMsg);
        g_crawling = false;
        BCrawlButton();
        if (!g_crawlQueue.empty()) SetTimer(g_hBrowser, 2, 3000 + GetTickCount() % 3000, nullptr);
    }
}

static void InitWebView(HWND hwnd);

static void BLayout() {
    if (!g_hBrowser) return;
    RECT rc; GetClientRect(g_hBrowser, &rc);
    int W = rc.right, H = rc.bottom, m = S(8), bh = S(28), g = S(6), bw = S(70), cw = S(120);
    MoveWindow(g_hBAddr, m, m + S(2), std::max(S(50), W - 3 * m - bw - cw - g), bh - S(4), TRUE);
    MoveWindow(g_hBGo, W - m - cw - g - bw, m, bw, bh, TRUE);
    MoveWindow(g_hBCrawl, W - m - cw, m, cw, bh, TRUE);
    int sh = S(22);
    MoveWindow(g_hBStatus, m, H - sh, W - 2 * m, sh - S(3), TRUE);
    if (g_wvCtl) {
        RECT b = { 0, m + bh + m, W, H - sh };
        g_wvCtl->put_Bounds(b);
    }
}

static LRESULT CALLBACK BrowserProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0) {
            HWND h = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)id, g_hInst, nullptr);
            SendMessageW(h, WM_SETFONT, (WPARAM)g_font, TRUE);
            return h;
        };
        g_hBAddr = mk(WC_EDITW, L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_B_ADDR, WS_EX_CLIENTEDGE);
        g_hBGo = mk(WC_BUTTONW, T(L"前往"), WS_TABSTOP | BS_PUSHBUTTON, IDC_B_GO);
        g_hBCrawl = mk(WC_BUTTONW, T(L"抓取本页全部"), WS_TABSTOP | BS_PUSHBUTTON, IDC_B_CRAWL);
        g_hBStatus = mk(WC_STATICW, T(L"正在启动内置浏览器…"), SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, IDC_B_STATUS);
        SendMessageW(g_hBAddr, EM_SETCUEBANNER, TRUE, (LPARAM)T(L"输入要登录的网站，例如 youtube.com，按 Enter"));
        SetWindowSubclass(g_hBAddr, [](HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) -> LRESULT {
            if (m == WM_KEYDOWN && w == VK_RETURN) { BNavigate(Trim(GetText(h))); return 0; }
            if (m == WM_CHAR && (w == L'\r' || w == L'\n')) return 0;
            return DefSubclassProc(h, m, w, l);
        }, 1, 0);
        InitWebView(hwnd);
        return 0;
    }
    case WM_SIZE:
        BLayout();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = S(520); mm->ptMinTrackSize.y = S(400);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, GetSysColor(COLOR_WINDOW));
        SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_B_GO: BNavigate(Trim(GetText(g_hBAddr))); return 0;
        case IDC_B_CRAWL:
            if (g_crawling) {
                g_crawlQueue.clear();
                if (g_wv) g_wv->ExecuteScript(L"window.__ytgrabStop=1", nullptr);
                g_crawling = false; g_crawlPending = false;
                BCrawlButton();
                BStatus(T(L"已停止"));
            } else if (IsXhs(CurrentUrl())) {
                StartCrawlHere();
            } else {
                BStatus(T(L"目前只能抓取小红书主页或笔记；其他网站登录后直接在主窗口粘贴链接即可"));
            }
            return 0;
        }
        break;
    case WM_TIMER:
        if (wp == 1) { KillTimer(hwnd, 1); ExportCookies(); }
        if (wp == 2) { KillTimer(hwnd, 2); NextCrawl(); }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) ExportCookies();
        break;
    case WM_CLOSE:
        if (g_wv) { g_wv->ExecuteScript(L"window.__ytgrabStop=1", nullptr); ExportCookies(); }
        g_crawlQueue.clear(); g_crawling = false;
        ShowWindow(hwnd, SW_HIDE);   // keep the browser alive: cookies stay exportable, reopening is instant
        return 0;
    case WM_DESTROY:
        if (g_wvCtl) { g_wvCtl->Close(); g_wvCtl->Release(); g_wvCtl = nullptr; }
        if (g_wv) { g_wv->Release(); g_wv = nullptr; }
        if (g_wvEnv) { g_wvEnv->Release(); g_wvEnv = nullptr; }
        g_hBrowser = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static std::wstring LoadXhsScript() {
    HRSRC r = FindResourceW(g_hInst, MAKEINTRESOURCEW(IDR_XHSJS), RT_RCDATA);
    if (!r) return {};
    HGLOBAL g = LoadResource(g_hInst, r);
    const char* p = g ? (const char*)LockResource(g) : nullptr;
    return p ? FromUtf8(std::string(p, SizeofResource(g_hInst, r))) : std::wstring();
}

typedef HRESULT(STDAPICALLTYPE* PFN_CreateEnv)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
                                               ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

static void WebViewFailed(const std::wstring& why) {
    g_wvFailed = true;
    BStatus(why);
    int r = MessageBoxW(g_hBrowser, (why + T(L"\n\n内置浏览器需要 Microsoft Edge WebView2 运行时（Windows 10/11 通常已自带）。现在打开下载页面吗？")).c_str(),
                        T(kAppName), MB_YESNO | MB_ICONWARNING);
    if (r == IDYES) ShellExecuteW(nullptr, L"open", L"https://go.microsoft.com/fwlink/p/?LinkId=2124703", nullptr, nullptr, SW_SHOWNORMAL);
}

static void InitWebView(HWND hwnd) {
    HMODULE dll = nullptr;
    for (auto& p : { ExeDir() + L"\\tools\\WebView2Loader.dll", ExeDir() + L"\\WebView2Loader.dll" })
        if (!dll && FileExists(p)) dll = LoadLibraryExW(p.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    auto create = dll ? (PFN_CreateEnv)GetProcAddress(dll, "CreateCoreWebView2EnvironmentWithOptions") : nullptr;
    if (!create) { WebViewFailed(T(L"找不到 WebView2Loader.dll（应在 tools 文件夹中）")); return; }
    std::wstring data = WebDataDir();
    SHCreateDirectoryExW(nullptr, data.c_str(), nullptr);
    auto envDone = MakeH<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [hwnd](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(hr) || !env) { WebViewFailed(T(L"无法启动内置浏览器")); return S_OK; }
            g_wvEnv = env; env->AddRef();
            auto ctlDone = MakeH<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [hwnd](HRESULT hr2, ICoreWebView2Controller* ctl) -> HRESULT {
                    if (FAILED(hr2) || !ctl) { WebViewFailed(T(L"无法启动内置浏览器")); return S_OK; }
                    g_wvCtl = ctl; ctl->AddRef();
                    ctl->get_CoreWebView2(&g_wv);
                    if (!g_wv) { WebViewFailed(T(L"无法启动内置浏览器")); return S_OK; }
                    ICoreWebView2Settings* st = nullptr;
                    if (SUCCEEDED(g_wv->get_Settings(&st)) && st) {
                        st->put_IsStatusBarEnabled(TRUE);
                        st->put_AreDevToolsEnabled(FALSE);
                        st->Release();
                    }
                    std::wstring js = LoadXhsScript();
                    if (!js.empty()) g_wv->AddScriptToExecuteOnDocumentCreated(js.c_str(), nullptr);
                    EventRegistrationToken tok;
                    g_wv->add_WebMessageReceived(MakeH<ICoreWebView2WebMessageReceivedEventHandler>(
                        [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* a) -> HRESULT {
                            LPWSTR s = nullptr;
                            if (SUCCEEDED(a->TryGetWebMessageAsString(&s)) && s) { std::wstring m = s; CoTaskMemFree(s); OnPageMessage(m); }
                            return S_OK;
                        }), &tok);
                    g_wv->add_NavigationCompleted(MakeH<ICoreWebView2NavigationCompletedEventHandler>(
                        [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                            std::wstring u = CurrentUrl();
                            if (GetFocus() != g_hBAddr) SetWindowTextW(g_hBAddr, u.c_str());
                            SetTimer(g_hBrowser, 1, 1500, nullptr);          // export cookies shortly after
                            std::wstring l = Lower(u);
                            if (g_crawling && Has(l, L"xiaohongshu.com/") &&
                                (Has(l, L"/user/profile/") || Has(l, L"/explore/") || Has(l, L"/discovery/item/"))) {
                                g_crawlPending = false;
                                g_wv->ExecuteScript(L"window.__ytgrabGo&&window.__ytgrabGo()", nullptr);
                            }
                            return S_OK;
                        }), &tok);
                    g_wv->add_DocumentTitleChanged(MakeH<ICoreWebView2DocumentTitleChangedEventHandler>(
                        [](ICoreWebView2* w, IUnknown*) -> HRESULT {
                            LPWSTR p = nullptr;
                            if (SUCCEEDED(w->get_DocumentTitle(&p)) && p) {
                                std::wstring t = p; CoTaskMemFree(p);
                                SetWindowTextW(g_hBrowser, ((t.empty() ? std::wstring() : t + L" - ") + T(L"YtGrab 内置浏览器")).c_str());
                            }
                            return S_OK;
                        }), &tok);
                    BLayout();
                    BStatus(T(L"在这里登录网站后，下载时会自动使用登录状态（设置里 Cookies 选「内置浏览器」）"));
                    if (!g_crawlQueue.empty()) NextCrawl();
                    else if (!g_wvStartUrl.empty()) BNavigate(g_wvStartUrl);
                    else SetFocus(g_hBAddr);
                    return S_OK;
                });
            env->CreateCoreWebView2Controller(hwnd, ctlDone);
            ctlDone->Release();
            return S_OK;
        });
    HRESULT hr = create(nullptr, data.c_str(), nullptr, envDone);
    envDone->Release();
    if (FAILED(hr)) WebViewFailed(T(L"无法启动内置浏览器"));
}

// Opens (or brings up) the built-in browser. crawl: queue a Xiaohongshu link to collect.
static void OpenBrowser(const std::wstring& url, bool crawl) {
    if (crawl && !url.empty()) g_crawlQueue.push_back(url);
    if (!g_hBrowser || g_wvFailed) {
        if (g_hBrowser) { DestroyWindow(g_hBrowser); g_hBrowser = nullptr; g_wvFailed = false; }
        static bool reg = false;
        if (!reg) {
            WNDCLASSEXW wc{ sizeof(wc) };
            wc.lpfnWndProc = BrowserProc; wc.hInstance = g_hInst;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wc.lpszClassName = kBrowserClass;
            wc.hIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
            RegisterClassExW(&wc);
            reg = true;
        }
        g_wvStartUrl = crawl ? std::wstring() : url;
        RECT r; GetWindowRect(g_hwnd, &r);
        g_hBrowser = CreateWindowExW(0, kBrowserClass, T(L"YtGrab 内置浏览器"), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                     r.left + S(40), r.top + S(40), S(1100), S(760), nullptr, nullptr, g_hInst, nullptr);
        ShowWindow(g_hBrowser, SW_SHOWNORMAL);
        return;
    }
    if (IsIconic(g_hBrowser)) ShowWindow(g_hBrowser, SW_RESTORE);
    ShowWindow(g_hBrowser, SW_SHOW);
    SetForegroundWindow(g_hBrowser);
    if (!g_wv) { if (!crawl) g_wvStartUrl = url; return; }   // still starting; picks it up when ready
    if (crawl) { if (!g_crawling) NextCrawl(); }
    else if (!url.empty()) BNavigate(url);
}
