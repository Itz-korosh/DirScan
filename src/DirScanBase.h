// ============================================================================
// DirscanBase.h — DIRSCAN scan engine. No GUI. No deps. Win32 + STL only.
// ============================================================================
#pragma once

#include <string>
#include <functional>
#include <cstdint>

namespace dsb {

using u64 = unsigned long long;

// What kind of entry we found
enum class EntryKind { File, Directory };

// Fired for every entry during a scan.
// Return false to request cancellation (checked per-entry).
using EntryFn = std::function<bool(const std::string& name,   // "foo.txt"
                                   const std::string& path,   // full UTF-8 path
                                   u64 size,                  // bytes
                                   EntryKind kind,
                                   int depth)>;               // 0 = top level

// Scan options — matches the 4 checkboxes in the GUI
struct Options {
    bool recursive      = true;
    bool followSymlinks = false;
    bool hashFiles      = false;   // reserved for v2
    bool showHidden     = true;
};

// UTF-8 <-> UTF-16 helpers
std::string  WideToUTF8(const std::wstring& w);
std::wstring UTF8ToWide(const std::string& s);

// Scan `rootPath` (UTF-8). Calls `onEntry` for every item found.
// Returns true on clean finish, false if the callback cancelled.
bool Scan(const std::string& rootPath,
          const Options& opts,
          const EntryFn& onEntry);

// Is this path an existing directory?
bool IsDirectory(const std::string& utf8Path);

// Total recursive size of a directory, in bytes.
u64 DirectorySize(const std::string& utf8Path);

} // namespace dsb