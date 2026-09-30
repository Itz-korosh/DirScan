// ============================================================================
// DirGui.cpp — GUI wrapper implementation.
// ============================================================================
#include "DirGui.h"

#include <cstdio>

namespace dirgui {

// ---------- byte formatting ----------
std::string FormatBytes(unsigned long long bytes) {
    char buf[64];
    const char* units[] = { "B", "KB", "MB", "GB", "TB", "PB" };
    double v = (double)bytes;
    int u = 0;
    while (v >= 1024.0 && u < 5) { v /= 1024.0; ++u; }

    if (u == 0) {
        snprintf(buf, sizeof(buf), "%llu B", bytes);
    } else if (v >= 100.0) {
        snprintf(buf, sizeof(buf), "%.0f %s", v, units[u]);
    } else if (v >= 10.0) {
        snprintf(buf, sizeof(buf), "%.1f %s", v, units[u]);
    } else {
        snprintf(buf, sizeof(buf), "%.2f %s", v, units[u]);
    }
    return std::string(buf);
}

// ---------- Window ----------
Window::Window() {}
Window::~Window() {}

dsg::Handle Window::create(const std::string& initialDir,
                           VoidCb onBrowse,
                           VoidCb onScan,
                           VoidCb onCancel) {
    if (!app_.create("DIRSCAN", 980, 660)) return nullptr;

    dirEdit_ = app_.buildTopBar(initialDir, onBrowse, onScan, onCancel);

    options_ = app_.buildOptions(true, false, false, true, nullptr);

    std::vector<dsg::Handle> pg = app_.buildProgress();
    if (pg.size() >= 3) {
        progress_ = pg[0];
        status_   = pg[1];
        stats_    = pg[2];
    }

    tree_ = app_.buildResults();
    log_  = app_.buildLog();

    app_.setStatus("Ready. Pick a folder and click Scan.");
    app_.setProgress(0);

    return dirEdit_;
}

int Window::run() { return app_.run(); }
void Window::close() { app_.close(); }

std::string Window::directory() const {
    return const_cast<dsg::App&>(app_).getText(dirEdit_);
}
void Window::setDirectory(const std::string& dir) {
    app_.setText(dirEdit_, dir);
}

void Window::setProgress(int pct) { app_.setProgress(pct); }
void Window::setStatus(const std::string& s) { app_.setStatus(s); }

void Window::setStats(unsigned long long files,
                      unsigned long long dirs,
                      unsigned long long bytes) {
    char buf[160];
    snprintf(buf, sizeof(buf), "files: %llu   dirs: %llu   size: %s",
             files, dirs, FormatBytes(bytes).c_str());
    app_.setStats(buf);
}

void Window::info (const std::string& s) { app_.log(dsg::LogLevel::Info,    s); }
void Window::ok   (const std::string& s) { app_.log(dsg::LogLevel::Success, s); }
void Window::warn (const std::string& s) { app_.log(dsg::LogLevel::Warn,    s); }
void Window::error(const std::string& s) { app_.log(dsg::LogLevel::Error,   s); }
void Window::debug(const std::string& s) { app_.log(dsg::LogLevel::Debug,   s); }

void Window::clearResults() { app_.clearResults(); }

void Window::addDirectory(const std::string& name,
                          unsigned long long size,
                          const std::string& fullPath) {
    std::string line = name + "/  [dir]  " + FormatBytes(size);
    app_.addResult(line, fullPath);
}

void Window::addFile(const std::string& name,
                     unsigned long long size,
                     const std::string& fullPath) {
    std::string line = name + "  [file]  " + FormatBytes(size);
    app_.addResult(line, fullPath);
}

void Window::setScanning(bool scanning) { app_.setScanning(scanning); }

bool Window::option(int idx) const {
    if (idx < 0 || idx >= (int)options_.size()) return false;
    return const_cast<dsg::App&>(app_).isChecked(options_[idx]);
}

std::string Window::pickFolder(const std::string& startDir) {
    return dsg::pickFolder(app_.nativeWindow(), startDir);
}

void Window::post(std::function<void()> fn) {
    app_.post(fn);
}

void* Window::native() const {
    return const_cast<dsg::App&>(app_).nativeWindow();
}

} // namespace dirgui