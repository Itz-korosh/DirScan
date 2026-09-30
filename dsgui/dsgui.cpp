// ============================================================================
// dsgui.cpp — DIRSCAN GUI Library  v1.0.0
// ============================================================================

#include "dsgui.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>

#include <map>
#include <deque>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <cwchar>

// ---------------------------------------------------------------------------
// Common controls structs (no commctrl.h needed)
// ---------------------------------------------------------------------------
typedef struct {
    DWORD dwSize;
    DWORD dwICC;
} DSG_ICC;

#define DSG_ICC_TREEVIEW_CLASSES   0x00000002
#define DSG_ICC_BAR_CLASSES        0x00000004
#define DSG_ICC_STANDARD_CLASSES   0x00004000
#define DSG_ICC_PROGRESS_CLASS     0x00000020

typedef BOOL (WINAPI *PFN_InitCommonControlsEx)(const DSG_ICC*);

#ifndef PROGRESS_CLASSW
#define PROGRESS_CLASSW L"msctls_progress32"
#endif

#define DSG_PBM_SETRANGE      (WM_USER + 1)
#define DSG_PBM_SETPOS        (WM_USER + 2)
#define DSG_PBM_SETMARQUEE    (WM_USER + 10)

#define DSG_PBS_SMOOTH        0x0001
#define DSG_PBS_MARQUEE       0x0008

namespace dsg {

// Forward declaration — INSIDE namespace dsg, matches the definition below
LRESULT CALLBACK dsgWndProc(HWND, UINT, WPARAM, LPARAM);

namespace {

const wchar_t* kWndClass = L"DsguiWnd_v1";
const UINT     WM_DSG_CB = WM_APP + 0x100;

enum : int {
    ID_EDIT    = 100,
    ID_BROWSE  = 101,
    ID_SCAN    = 102,
    ID_CANCEL  = 103,
    ID_OPT0    = 200,
    ID_PROGRESS= 300,
    ID_STATUS  = 301,
    ID_STATS   = 302,
    ID_TREE    = 400,
    ID_LOG     = 500,
};

std::wstring toW(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
std::string toA(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                        &s[0], n, nullptr, nullptr);
    return s;
}
std::wstring getTextW(HWND h) {
    if (!h) return std::wstring();
    int n = GetWindowTextLengthW(h);
    if (n <= 0) return std::wstring();
    std::wstring s((size_t)n, L'\0');
    GetWindowTextW(h, &s[0], n + 1);
    return s;
}

void copyW(wchar_t* dst, size_t dstCount, const wchar_t* src) {
    if (!dst || dstCount == 0) return;
    if (!src) { dst[0] = L'\0'; return; }
    size_t i = 0;
    for (; i + 1 < dstCount && src[i]; ++i) dst[i] = src[i];
    dst[i] = L'\0';
}

class Mutex {
public:
    Mutex()  { InitializeCriticalSection(&cs_); }
    ~Mutex() { DeleteCriticalSection(&cs_); }
    void lock()   { EnterCriticalSection(&cs_); }
    void unlock() { LeaveCriticalSection(&cs_); }
private:
    CRITICAL_SECTION cs_;
    Mutex(const Mutex&);
    Mutex& operator=(const Mutex&);
};
class LockGuard {
public:
    explicit LockGuard(Mutex& m) : m_(&m) { m_->lock(); }
    ~LockGuard() { m_->unlock(); }
private:
    Mutex* m_;
    LockGuard(const LockGuard&);
    LockGuard& operator=(const LockGuard&);
};

struct Fonts {
    HFONT normal = nullptr;
    HFONT bold   = nullptr;
    HFONT mono   = nullptr;
    void ensure() {
        if (normal) return;
        HDC hdc = GetDC(nullptr);
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(nullptr, hdc);
        normal = mk(L"Segoe UI", 10, false, dpi);
        bold   = mk(L"Segoe UI", 10, true,  dpi);
        mono   = mk(L"Consolas", 10, false, dpi);
    }
    static HFONT mk(const wchar_t* face, int pt, bool b, int dpi) {
        LOGFONTW lf = {};
        lf.lfHeight  = -MulDiv(pt, dpi, 72);
        lf.lfWeight  = b ? FW_BOLD : FW_NORMAL;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;
        copyW(lf.lfFaceName, LF_FACESIZE, face);
        return CreateFontIndirectW(&lf);
    }
};
Fonts g_fonts;

bool ensureClass(HINSTANCE hinst) {
    static bool done = false;
    if (done) return true;

    if (HMODULE cc = LoadLibraryW(L"comctl32.dll")) {
        PFN_InitCommonControlsEx fn =
            (PFN_InitCommonControlsEx)GetProcAddress(cc, "InitCommonControlsEx");
        if (fn) {
            DSG_ICC icc;
            icc.dwSize = sizeof(icc);
            icc.dwICC  = DSG_ICC_TREEVIEW_CLASSES | DSG_ICC_BAR_CLASSES |
                         DSG_ICC_STANDARD_CLASSES | DSG_ICC_PROGRESS_CLASS;
            fn(&icc);
        }
    }

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = dsgWndProc;
    wc.hInstance     = hinst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = kWndClass;
    if (!RegisterClassExW(&wc)) return false;
    done = true;
    return true;
}

} // anon

// ===========================================================================
struct App::Impl {
    HWND hwnd = nullptr;
    HINSTANCE hinst = nullptr;

    HWND dirEdit = nullptr, btnBrowse = nullptr, btnScan = nullptr, btnCancel = nullptr;
    std::vector<HWND> optionBoxes;
    HWND progress = nullptr, status = nullptr, stats = nullptr;
    HWND tree = nullptr, log = nullptr;

    VoidCb   onBrowse, onScan, onCancel, onClose;
    ToggleCb onToggle;
    PathCb   onDirectoryChosen;
    ResizeCb onResize;

    int padding    = 10;
    int rowH       = 28;
    int topY       = 10;
    int optionsY   = 48;
    int progressY  = 82;
    int splitY     = 118;
    int logHeight  = 170;

    Mutex              qMutex;
    std::deque<VoidCb> qCallbacks;
};

static std::map<HWND, App::Impl*>& mapRef() {
    static std::map<HWND, App::Impl*>* m = new std::map<HWND, App::Impl*>();
    return *m;
}

// ===========================================================================
static void doLayout(App::Impl* p) {
    if (!p || !p->hwnd) return;
    RECT rc; GetClientRect(p->hwnd, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    int pad = p->padding;

    int btnW = 92;
    int editW = W - pad*2 - btnW*3 - 12;
    if (editW < 120) editW = 120;

    int x = pad;
    if (p->dirEdit)   MoveWindow(p->dirEdit,   x, p->topY, editW, p->rowH, TRUE);
    x += editW + 6;
    if (p->btnBrowse) MoveWindow(p->btnBrowse, x, p->topY, btnW, p->rowH, TRUE);
    x += btnW + 4;
    if (p->btnScan)   MoveWindow(p->btnScan,   x, p->topY, btnW, p->rowH, TRUE);
    x += btnW + 4;
    if (p->btnCancel) MoveWindow(p->btnCancel, x, p->topY, btnW, p->rowH, TRUE);

    int ox = pad, oy = p->optionsY;
    for (HWND cb : p->optionBoxes) {
        RECT r; GetWindowRect(cb, &r);
        int cw = (r.right - r.left) + 6;
        MoveWindow(cb, ox, oy, cw, 22, TRUE);
        ox += cw + 12;
    }

    int progY  = p->progressY;
    int statsW = 220;
    if (p->progress) MoveWindow(p->progress, pad, progY, W - pad*2 - statsW - 8, 22, TRUE);
    if (p->stats)    MoveWindow(p->stats, W - pad - statsW, progY, statsW, 22, TRUE);
    if (p->status)   MoveWindow(p->status, pad, progY + 24, W - pad*2, 20, TRUE);

    int splitTop = p->splitY + 30;
    int logH     = p->logHeight;
    int splitH   = H - splitTop - logH - pad - 6;
    if (splitH < 80) splitH = 80;
    if (p->tree) MoveWindow(p->tree, pad, splitTop, W - pad*2, splitH, TRUE);

    int logY = splitTop + splitH + 6;
    int realLogH = H - logY - pad;
    if (realLogH < 60) realLogH = 60;
    if (p->log) MoveWindow(p->log, pad, logY, W - pad*2, realLogH, TRUE);
}

static void appendLog(App::Impl* p, const std::string& line) {
    if (!p || !p->log) return;
    std::wstring w = toW(line);
    w += L"\r\n";
    int len = GetWindowTextLengthW(p->log);
    SendMessageW(p->log, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(p->log, EM_REPLACESEL, FALSE, (LPARAM)w.c_str());
    SendMessageW(p->log, EM_SCROLLCARET, 0, 0);
}

#ifndef TV_FIRST
#define TV_FIRST 0x1100
#endif
#define DSG_TVM_INSERTITEMW   (TV_FIRST + 13)
#define DSG_TVM_DELETEITEM    (TV_FIRST + 2)
#define DSG_TVI_ROOT          ((HTREEITEM)(ULONG_PTR)0xFFFF0000)
#define DSG_TVI_LAST          ((HTREEITEM)(ULONG_PTR)0xFFFF0003)
#define DSG_TVIF_TEXT         0x0001

struct DSG_TVITEMW {
    UINT      mask;
    HTREEITEM hItem;
    UINT      state;
    UINT      stateMask;
    LPWSTR    pszText;
    int       cchTextMax;
    int       iImage;
    int       iSelectedImage;
    int       cChildren;
    LPARAM    lParam;
};
struct DSG_TVINSERTSTRUCTW {
    HTREEITEM hParent;
    HTREEITEM hInsertAfter;
    DSG_TVITEMW item;
};

static HTREEITEM insertTreeItem(HWND tree, HTREEITEM parent,
                                const std::wstring& c1, const std::wstring& c2) {
    if (!tree) return nullptr;
    std::wstring shown = c1;
    if (!c2.empty()) shown += L"   \u2014   " + c2;

    DSG_TVINSERTSTRUCTW tv;
    memset(&tv, 0, sizeof(tv));
    tv.hParent      = parent ? parent : DSG_TVI_ROOT;
    tv.hInsertAfter = DSG_TVI_LAST;
    tv.item.mask    = DSG_TVIF_TEXT;
    tv.item.pszText = (LPWSTR)shown.c_str();
    return (HTREEITEM)SendMessageW(tree, DSG_TVM_INSERTITEMW, 0, (LPARAM)&tv);
}

// ===========================================================================
// Definition — matches the global-scope forward declaration above.
// Because the class is in an anon namespace it still links to the same TU.
// ===========================================================================
static LRESULT CALLBACK dsgWndProcImpl(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

// Actual implementation, at global scope
LRESULT CALLBACK dsgWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return dsgWndProcImpl(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK dsgWndProcImpl(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto& M = mapRef();
    auto it = M.find(hwnd);
    App::Impl* p = (it != M.end()) ? it->second : nullptr;

    switch (msg) {
    case WM_SIZE:
        if (p) { doLayout(p); if (p->onResize) p->onResize(LOWORD(lp), HIWORD(lp)); }
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = 640;
        mmi->ptMinTrackSize.y = 480;
        return 0;
    }
    case WM_COMMAND: {
        if (!p) return 0;
        int id   = LOWORD(wp);
        int code = HIWORD(wp);
        if (id == ID_BROWSE && code == BN_CLICKED) { if (p->onBrowse) p->onBrowse(); return 0; }
        if (id == ID_SCAN   && code == BN_CLICKED) { if (p->onScan)   p->onScan();   return 0; }
        if (id == ID_CANCEL && code == BN_CLICKED) { if (p->onCancel) p->onCancel(); return 0; }
        if (id >= ID_OPT0 && id < ID_OPT0 + 16 && code == BN_CLICKED) {
            HWND cb = (HWND)lp;
            int idx = id - ID_OPT0;
            bool on = SendMessageW(cb, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (p->onToggle) p->onToggle(idx, on);
            return 0;
        }
        return 0;
    }
    case WM_DSG_CB: {
        if (!p) return 0;
        for (;;) {
            VoidCb cb;
            {
                LockGuard lk(p->qMutex);
                if (p->qCallbacks.empty()) break;
                cb = p->qCallbacks.front();
                p->qCallbacks.pop_front();
            }
            if (cb) cb();
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_CLOSE:
        if (p && p->onClose) p->onClose();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (p) { p->hwnd = nullptr; M.erase(hwnd); }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ===========================================================================
App::App() : p(new Impl()) {}
App::~App() { delete p; }

bool App::create(const std::string& title, int width, int height) {
    typedef BOOL (WINAPI *SetDpiAwareFn)(void);
    if (HMODULE u32 = GetModuleHandleW(L"user32.dll")) {
        auto fn = (SetDpiAwareFn)GetProcAddress(u32, "SetProcessDPIAware");
        if (fn) fn();
    }
    g_fonts.ensure();
    p->hinst = GetModuleHandleW(nullptr);
    if (!ensureClass(p->hinst)) return false;
    p->hwnd = CreateWindowExW(0, kWndClass, toW(title).c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, p->hinst, nullptr);
    if (!p->hwnd) return false;
    mapRef()[p->hwnd] = p;
    return true;
}

int App::run() {
    if (!p->hwnd) return -1;
    ShowWindow(p->hwnd, SW_SHOW);
    UpdateWindow(p->hwnd);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(p->hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}

void App::close() { if (p->hwnd) PostMessageW(p->hwnd, WM_CLOSE, 0, 0); }
void* App::nativeWindow() const { return p->hwnd; }

Handle App::buildTopBar(const std::string& initialDir,
                        VoidCb onBrowse, VoidCb onScan, VoidCb onCancel) {
    p->onBrowse = onBrowse;
    p->onScan   = onScan;
    p->onCancel = onCancel;

    p->dirEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", toW(initialDir).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0,0,10,10, p->hwnd, (HMENU)(INT_PTR)ID_EDIT, p->hinst, nullptr);
    SendMessageW(p->dirEdit, WM_SETFONT, (WPARAM)g_fonts.normal, TRUE);

    p->btnBrowse = CreateWindowExW(0, L"BUTTON", L"Browse...",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0,0,10,10, p->hwnd, (HMENU)(INT_PTR)ID_BROWSE, p->hinst, nullptr);
    SendMessageW(p->btnBrowse, WM_SETFONT, (WPARAM)g_fonts.normal, TRUE);

    p->btnScan = CreateWindowExW(0, L"BUTTON", L"Scan",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        0,0,10,10, p->hwnd, (HMENU)(INT_PTR)ID_SCAN, p->hinst, nullptr);
    SendMessageW(p->btnScan, WM_SETFONT, (WPARAM)g_fonts.bold, TRUE);

    p->btnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        0,0,10,10, p->hwnd, (HMENU)(INT_PTR)ID_CANCEL, p->hinst, nullptr);
    SendMessageW(p->btnCancel, WM_SETFONT, (WPARAM)g_fonts.normal, TRUE);
    EnableWindow(p->btnCancel, FALSE);

    doLayout(p);
    return (Handle)p->dirEdit;
}

std::vector<Handle> App::buildOptions(bool recursive, bool followSymlinks,
                                      bool hashFiles, bool hidden, ToggleCb onToggle) {
    p->onToggle = onToggle;
    p->optionBoxes.clear();
    struct Opt { const wchar_t* label; bool on; };
    Opt opts[4] = {
        { L"Recursive",       recursive },
        { L"Follow symlinks", followSymlinks },
        { L"Compute hashes",  hashFiles },
        { L"Show hidden",     hidden },
    };
    std::vector<Handle> out;
    for (int i = 0; i < 4; ++i) {
        HWND cb = CreateWindowExW(0, L"BUTTON", opts[i].label,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0,0,10,10, p->hwnd, (HMENU)(INT_PTR)(ID_OPT0 + i), p->hinst, nullptr);
        SendMessageW(cb, WM_SETFONT, (WPARAM)g_fonts.normal, TRUE);
        SendMessageW(cb, BM_SETCHECK, opts[i].on ? BST_CHECKED : BST_UNCHECKED, 0);
        p->optionBoxes.push_back(cb);
        out.push_back((Handle)cb);
    }
    doLayout(p);
    return out;
}

std::vector<Handle> App::buildProgress() {
    p->progress = CreateWindowExW(0, PROGRESS_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | DSG_PBS_SMOOTH,
        0,0,10,10, p->hwnd, (HMENU)(INT_PTR)ID_PROGRESS, p->hinst, nullptr);
    SendMessageW(p->progress, DSG_PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(p->progress, DSG_PBM_SETPOS, 0, 0);

    p->stats = CreateWindowExW(0, L"STATIC", L"files: 0  dirs: 0",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        0,0,10,10, p->hwnd, (HMENU)(INT_PTR)ID_STATS, p->hinst, nullptr);
    SendMessageW(p->stats, WM_SETFONT, (WPARAM)g_fonts.normal, TRUE);

    p->status = CreateWindowExW(0, L"STATIC", L"Ready.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        0,0,10,10, p->hwnd, (HMENU)(INT_PTR)ID_STATUS, p->hinst, nullptr);
    SendMessageW(p->status, WM_SETFONT, (WPARAM)g_fonts.normal, TRUE);

    doLayout(p);
    return { (Handle)p->progress, (Handle)p->status, (Handle)p->stats };
}

Handle App::buildResults() {
    p->tree = CreateWindowExW(WS_EX_CLIENTEDGE, L"SysTreeView32", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP |
        0x0002 | 0x0001 | 0x0008 | 0x0010,
        0,0,10,10, p->hwnd, (HMENU)(INT_PTR)ID_TREE, p->hinst, nullptr);
    SendMessageW(p->tree, WM_SETFONT, (WPARAM)g_fonts.normal, TRUE);
    doLayout(p);
    return (Handle)p->tree;
}

Handle App::buildLog() {
    p->log = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0,0,10,10, p->hwnd, (HMENU)(INT_PTR)ID_LOG, p->hinst, nullptr);
    SendMessageW(p->log, WM_SETFONT, (WPARAM)g_fonts.mono, TRUE);
    SendMessageW(p->log, EM_SETLIMITTEXT, 0, 0);
    doLayout(p);
    return (Handle)p->log;
}

void App::setProgress(int percent) {
    if (!p->progress) return;
    LONG style = GetWindowLongW(p->progress, GWL_STYLE);
    if (percent < 0) {
        if (!(style & DSG_PBS_MARQUEE))
            SetWindowLongW(p->progress, GWL_STYLE, style | DSG_PBS_MARQUEE);
        SendMessageW(p->progress, DSG_PBM_SETMARQUEE, TRUE, 30);
    } else {
        if (style & DSG_PBS_MARQUEE) {
            SendMessageW(p->progress, DSG_PBM_SETMARQUEE, FALSE, 0);
            SetWindowLongW(p->progress, GWL_STYLE, style & ~DSG_PBS_MARQUEE);
        }
        if (percent > 100) percent = 100;
        SendMessageW(p->progress, DSG_PBM_SETPOS, (WPARAM)percent, 0);
    }
}

void App::setStatus(const std::string& text) {
    if (p->status) SetWindowTextW(p->status, toW(text).c_str());
}
void App::setStats(const std::string& text) {
    if (p->stats) SetWindowTextW(p->stats, toW(text).c_str());
}

void App::log(LogLevel lvl, const std::string& text) {
    SYSTEMTIME st; GetLocalTime(&st);
    char ts[32];
    snprintf(ts, sizeof(ts), "%02d:%02d:%02d ",
             (int)st.wHour, (int)st.wMinute, (int)st.wSecond);
    const char* tag = "INFO ";
    switch (lvl) {
        case LogLevel::Info:    tag = "INFO "; break;
        case LogLevel::Success: tag = "OK   "; break;
        case LogLevel::Warn:    tag = "WARN "; break;
        case LogLevel::Error:   tag = "ERR  "; break;
        case LogLevel::Debug:   tag = "DBG  "; break;
    }
    appendLog(p, std::string(ts) + tag + text);
}

void App::clearLog() { if (p->log) SetWindowTextW(p->log, L""); }
void App::clearResults() {
    if (p->tree) SendMessageW(p->tree, DSG_TVM_DELETEITEM, 0, (LPARAM)DSG_TVI_ROOT);
}

Handle App::addResult(const std::string& path, const std::string& info) {
    if (!p->tree) return nullptr;
    return (Handle)insertTreeItem(p->tree, nullptr, toW(path), toW(info));
}
Handle App::addResultChild(Handle parent, const std::string& path, const std::string& info) {
    if (!p->tree) return nullptr;
    return (Handle)insertTreeItem(p->tree, (HTREEITEM)parent, toW(path), toW(info));
}

std::string App::getText(Handle h) {
    if (!h) return std::string();
    return toA(getTextW((HWND)h));
}
void App::setText(Handle h, const std::string& text) {
    if (h) SetWindowTextW((HWND)h, toW(text).c_str());
}
void App::setEnabled(Handle h, bool enabled) {
    if (h) EnableWindow((HWND)h, enabled ? TRUE : FALSE);
}
void App::setVisible(Handle h, bool visible) {
    if (h) ShowWindow((HWND)h, visible ? SW_SHOW : SW_HIDE);
}
bool App::isChecked(Handle h) {
    if (!h) return false;
    return SendMessageW((HWND)h, BM_GETCHECK, 0, 0) == BST_CHECKED;
}
void App::setChecked(Handle h, bool checked) {
    if (h) SendMessageW((HWND)h, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

void App::setScanning(bool scanning) {
    if (p->btnScan)   EnableWindow(p->btnScan,   scanning ? FALSE : TRUE);
    if (p->btnCancel) EnableWindow(p->btnCancel, scanning ? TRUE  : FALSE);
    if (p->dirEdit)   EnableWindow(p->dirEdit,   scanning ? FALSE : TRUE);
    if (p->btnBrowse) EnableWindow(p->btnBrowse, scanning ? FALSE : TRUE);
}

void App::onClose(VoidCb cb) { p->onClose = cb; }
void App::onResize(ResizeCb cb) { p->onResize = cb; }
void App::onDirectoryChosen(PathCb cb) { p->onDirectoryChosen = cb; }

void App::post(VoidCb cb) {
    if (!p->hwnd) return;
    {
        LockGuard lk(p->qMutex);
        p->qCallbacks.push_back(cb);
    }
    PostMessageW(p->hwnd, WM_DSG_CB, 0, 0);
}

void App::setLogHeight(int px) {
    if (px < 60) px = 60;
    p->logHeight = px;
    doLayout(p);
}

// ===========================================================================
std::string pickFolder(void* parentHwnd, const std::string& initialDir) {
    (void)initialDir;
    BROWSEINFOW bi;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = (HWND)parentHwnd;
    bi.lpszTitle = L"Select a folder to scan:";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return "";
    wchar_t path[MAX_PATH] = {};
    BOOL ok = SHGetPathFromIDListW(pidl, path);
    CoTaskMemFree(pidl);
    if (!ok) return "";
    return toA(path);
}

void messageBox(void* parentHwnd, const std::string& title,
                const std::string& text, bool isError) {
    MessageBoxW((HWND)parentHwnd, toW(text).c_str(), toW(title).c_str(),
                MB_OK | (isError ? MB_ICONERROR : MB_ICONINFORMATION));
}

const char* versionString() { return "1.0.0"; }

} // namespace dsg