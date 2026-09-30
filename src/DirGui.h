// ============================================================================
// DirGui.h — thin wrapper over dsgui. No scanning logic here.
// ============================================================================
#pragma once

#include "../dsgui/dsgui.h"

#include <string>
#include <functional>
#include <vector>

namespace dirgui {

using VoidCb = std::function<void()>;

class Window {
public:
    Window();
    ~Window();

    // Build the DIRSCAN window. Returns the directory-edit handle (or nullptr).
    dsg::Handle create(const std::string& initialDir,
                       VoidCb onBrowse,
                       VoidCb onScan,
                       VoidCb onCancel);

    int  run();
    void close();

    // ---- App-facing helpers (all UI-thread) ----
    std::string directory() const;
    void        setDirectory(const std::string& dir);

    void setProgress(int pct);                     // 0..100, -1 = marquee
    void setStatus(const std::string& s);
    void setStats(unsigned long long files,
                  unsigned long long dirs,
                  unsigned long long bytes);

    // Log
    void info (const std::string& s);
    void ok   (const std::string& s);
    void warn (const std::string& s);
    void error(const std::string& s);
    void debug(const std::string& s);

    // Results tree
    void clearResults();
    void addDirectory(const std::string& name,
                      unsigned long long size,
                      const std::string& fullPath);
    void addFile(const std::string& name,
                 unsigned long long size,
                 const std::string& fullPath);

    void setScanning(bool scanning);

    bool option(int idx) const;                    // 0..3

    std::string pickFolder(const std::string& startDir);

    void post(std::function<void()> fn);           // run on UI thread

    void* native() const;

    dsg::App& app() { return app_; }

private:
    dsg::App app_;
    dsg::Handle dirEdit_  = nullptr;
    dsg::Handle progress_ = nullptr;
    dsg::Handle status_   = nullptr;
    dsg::Handle stats_    = nullptr;
    dsg::Handle tree_     = nullptr;
    dsg::Handle log_      = nullptr;
    std::vector<dsg::Handle> options_;

    Window(const Window&);
    Window& operator=(const Window&);
};

// "1.2 MB", "843 B", etc.
std::string FormatBytes(unsigned long long bytes);

} // namespace dirgui