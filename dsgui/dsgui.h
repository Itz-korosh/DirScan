// ============================================================================
// dsgui.h — DIRSCAN GUI Library  v1.0.0
// Public header. Include this from your app.
// Windows 7+  |  C++11  |  DLL
// ============================================================================
#pragma once

#ifdef DSGUI_EXPORTS
    #define DSG_API __declspec(dllexport)
#else
    #define DSG_API __declspec(dllimport)
#endif

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace dsg {

// ---- Callbacks -----------------------------------------------------------
using VoidCb   = std::function<void()>;
using StrCb    = std::function<void(const std::string&)>;
using PathCb   = std::function<void(const std::string&)>;
using ToggleCb = std::function<void(int, bool)>;
using ResizeCb = std::function<void(int, int)>;

// ---- Log severity --------------------------------------------------------
enum class LogLevel : int {
    Info = 0, Success = 1, Warn = 2, Error = 3, Debug = 4,
};

// ---- Opaque widget handle ------------------------------------------------
using Handle = void*;

// ==========================================================================
class DSG_API App {
public:
    App();
    ~App();

    // Lifecycle
    bool create(const std::string& title = "DIRSCAN",
                int width  = 960,
                int height = 640);
    int  run();
    void close();
    void* nativeWindow() const;

    // ---- Build UI ----
    Handle buildTopBar(const std::string& initialDir,
                       VoidCb onBrowse, VoidCb onScan, VoidCb onCancel);

    std::vector<Handle> buildOptions(bool recursive      = true,
                                     bool followSymlinks = false,
                                     bool hashFiles      = false,
                                     bool hidden         = true,
                                     ToggleCb onToggle   = nullptr);

    std::vector<Handle> buildProgress();   // {progress, status, stats}
    Handle              buildResults();    // tree
    Handle              buildLog();        // log

    // ---- Runtime ----
    void setProgress(int percent);         // 0..100, -1 = indeterminate
    void setStatus(const std::string& text);
    void setStats(const std::string& text);
    void log(LogLevel lvl, const std::string& text);
    void clearLog();
    void clearResults();

    Handle addResult(const std::string& path, const std::string& info);
    Handle addResultChild(Handle parent,
                          const std::string& path,
                          const std::string& info);

    // ---- Widget ops ----
    std::string getText(Handle h);
    void        setText(Handle h, const std::string& text);
    void        setEnabled(Handle h, bool enabled);
    void        setVisible(Handle h, bool visible);
    bool        isChecked(Handle h);
    void        setChecked(Handle h, bool checked);

    void setScanning(bool scanning);

    // ---- Events ----
    void onClose(VoidCb cb);
    void onResize(ResizeCb cb);
    void onDirectoryChosen(PathCb cb);

    // ---- UI-thread post ----
    void post(VoidCb cb);
    void setLogHeight(int px);

    struct Impl;
private:
    Impl* p;
    App(const App&);
    App& operator=(const App&);
};

// ---- Utilities ----
DSG_API std::string pickFolder(void* parentHwnd,
                               const std::string& initialDir = "");
DSG_API void        messageBox(void* parentHwnd,
                               const std::string& title,
                               const std::string& text,
                               bool isError = false);
DSG_API const char* versionString();

} // namespace dsg