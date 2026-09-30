// ============================================================================
// DirscanBase.cpp — scan engine. Ported from the original console DirScan.
// ============================================================================
#include "DirscanBase.h"

#include <windows.h>
#include <vector>

namespace dsb {

// ---- UTF helpers ----------------------------------------------------------
std::string WideToUTF8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                        &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring UTF8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// ---- Public: IsDirectory --------------------------------------------------
bool IsDirectory(const std::string& utf8Path) {
    std::wstring w = UTF8ToWide(utf8Path);
    if (w.empty()) return false;
    DWORD a = GetFileAttributesW(w.c_str());
    return (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
}

// ---- Recursive size -------------------------------------------------------
static u64 DirSizeRec(const std::wstring& path, const Options& opts) {
    u64 total = 0;
    std::wstring search = path + L"\\*";

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(search.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        bool hidden = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
        if (!opts.showHidden && hidden) continue;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            bool isLink = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
            if (isLink && !opts.followSymlinks) continue;
            std::wstring sub = path + L"\\" + fd.cFileName;
            total += DirSizeRec(sub, opts);
        } else {
            LARGE_INTEGER li;
            li.HighPart = (LONG)fd.nFileSizeHigh;
            li.LowPart  = fd.nFileSizeLow;
            total += (u64)li.QuadPart;
        }
    } while (FindNextFileW(h, &fd) != 0);

    FindClose(h);
    return total;
}

u64 DirectorySize(const std::string& utf8Path) {
    Options def;
    return DirSizeRec(UTF8ToWide(utf8Path), def);
}

// ---- Recursive scan -------------------------------------------------------
struct Item {
    std::wstring full;
    std::wstring name;
    u64          size;
    bool         isDir;
};

static bool ScanRec(const std::wstring& path, int depth,
                    const Options& opts, const EntryFn& onEntry) {
    std::wstring search = path + L"\\*";

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(search.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return true;   // unreadable dir → skip

    std::vector<Item> items;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        bool hidden = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
        if (!opts.showHidden && hidden) continue;

        Item it;
        it.full  = path + L"\\" + fd.cFileName;
        it.name  = fd.cFileName;
        it.isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (it.isDir) {
            bool isLink = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
            if (isLink && !opts.followSymlinks) continue;
            it.size = 0;
        } else {
            LARGE_INTEGER li;
            li.HighPart = (LONG)fd.nFileSizeHigh;
            li.LowPart  = fd.nFileSizeLow;
            it.size = (u64)li.QuadPart;
        }
        items.push_back(it);
    } while (FindNextFileW(h, &fd) != 0);

    FindClose(h);

    // Directories first (and recurse), then files.
    for (size_t i = 0; i < items.size(); ++i) {
        const Item& it = items[i];
        if (!it.isDir) continue;

        u64 size = DirSizeRec(it.full, opts);
        std::string nameA = WideToUTF8(it.name);
        std::string pathA = WideToUTF8(it.full);

        if (!onEntry(nameA, pathA, size, EntryKind::Directory, depth))
            return false;

        if (opts.recursive) {
            if (!ScanRec(it.full, depth + 1, opts, onEntry))
                return false;
        }
    }
    for (size_t i = 0; i < items.size(); ++i) {
        const Item& it = items[i];
        if (it.isDir) continue;

        std::string nameA = WideToUTF8(it.name);
        std::string pathA = WideToUTF8(it.full);
        if (!onEntry(nameA, pathA, it.size, EntryKind::File, depth))
            return false;
    }
    return true;
}

bool Scan(const std::string& rootPath,
          const Options& opts,
          const EntryFn& onEntry) {
    std::wstring w = UTF8ToWide(rootPath);
    if (w.empty()) return false;
    return ScanRec(w, 0, opts, onEntry);
}

} // namespace dsb