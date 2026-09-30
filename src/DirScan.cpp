// ============================================================================
// DirScan.cpp — DIRSCAN main app.
// Wires DirscanBase (engine) + DirGui (window). Owns the worker thread.
// ============================================================================
#include "DirGui.h"
#include "DirscanBase.h"

#include <windows.h>
#include <process.h>   // _beginthreadex
#include <atomic>
#include <string>

// ---------------------------------------------------------------------------
// Shared state between worker thread and UI
// ---------------------------------------------------------------------------
struct ScanContext {
    std::atomic<bool> cancel;
    std::atomic<bool> running;
    std::atomic<unsigned long long> files;
    std::atomic<unsigned long long> dirs;
    std::atomic<unsigned long long> bytes;

    ScanContext() : cancel(false), running(false), files(0), dirs(0), bytes(0) {}
};

static dirgui::Window* g_win    = nullptr;
static ScanContext     g_ctx;
static HANDLE          g_thread = nullptr;

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------
static unsigned __stdcall ScanThread(void* /*param*/) {
    dirgui::Window* win = g_win;

    std::string root = win->directory();
    if (root.empty()) {
        win->post([win]() {
            win->error("No directory chosen.");
            win->setScanning(false);
            win->setProgress(0);
            win->setStatus("Ready.");
        });
        g_ctx.running = false;
        return 0;
    }

    dsb::Options opts;
    opts.recursive      = win->option(0);
    opts.followSymlinks = win->option(1);
    opts.hashFiles      = win->option(2);
    opts.showHidden     = win->option(3);

    win->post([win, root]() {
        win->clearResults();
        win->info("Start scan: " + root);
        win->setProgress(-1);
        win->setStatus("Scanning...");
        win->setStats(0, 0, 0);
    });

    // Callback runs on the worker thread → hop to UI thread via win->post()
    dsb::EntryFn onEntry = [win](const std::string& name,
                                 const std::string& path,
                                 dsb::u64 size,
                                 dsb::EntryKind kind,
                                 int /*depth*/) -> bool {

        if (g_ctx.cancel.load()) return false;

        if (kind == dsb::EntryKind::Directory) {
            g_ctx.dirs.fetch_add(1);
            g_ctx.bytes.fetch_add(size);
            win->post([win, name, path, size]() {
                win->addDirectory(name, size, path);
            });
        } else {
            g_ctx.files.fetch_add(1);
            g_ctx.bytes.fetch_add(size);
            win->post([win, name, path, size]() {
                win->addFile(name, size, path);
            });
        }

        // Throttled stats refresh (every ~64 entries)
        static std::atomic<int> tick(0);
        if ((tick.fetch_add(1) & 63) == 0) {
            unsigned long long f = g_ctx.files.load();
            unsigned long long d = g_ctx.dirs.load();
            unsigned long long b = g_ctx.bytes.load();
            win->post([win, f, d, b]() { win->setStats(f, d, b); });
        }

        return true;
    };

    bool finished = dsb::Scan(root, opts, onEntry);

    unsigned long long f = g_ctx.files.load();
    unsigned long long d = g_ctx.dirs.load();
    unsigned long long b = g_ctx.bytes.load();

    win->post([win, finished, f, d, b]() {
        win->setStats(f, d, b);
        win->setScanning(false);
        win->setProgress(100);
        if (finished) {
            win->ok("Scan complete.");
            win->setStatus("Done.");
        } else {
            win->warn("Scan cancelled.");
            win->setStatus("Cancelled.");
        }
    });

    g_ctx.running = false;
    return 0;
}

// ---------------------------------------------------------------------------
// Button handlers (UI thread)
// ---------------------------------------------------------------------------
static void OnBrowse() {
    std::string start  = g_win->directory();
    std::string picked = g_win->pickFolder(start);
    if (!picked.empty()) g_win->setDirectory(picked);
}

static void OnScan() {
    if (g_ctx.running.load()) return;

    g_ctx.cancel  = false;
    g_ctx.files   = 0;
    g_ctx.dirs    = 0;
    g_ctx.bytes   = 0;
    g_ctx.running = true;

    g_win->setScanning(true);

    unsigned tid = 0;
    g_thread = (HANDLE)_beginthreadex(nullptr, 0, &ScanThread, nullptr, 0, &tid);
    if (!g_thread) {
        g_win->error("Failed to start worker thread.");
        g_win->setScanning(false);
        g_ctx.running = false;
    }
}

static void OnCancel() {
    if (!g_ctx.running.load()) return;
    g_ctx.cancel = true;
    g_win->warn("Cancel requested...");
    g_win->setStatus("Cancelling...");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    dirgui::Window win;
    g_win = &win;

    dsg::Handle dirEdit = win.create("C:\\", OnBrowse, OnScan, OnCancel);
    if (!dirEdit) {
        dsg::messageBox(nullptr, "DIRSCAN",
                        "Failed to create window.", true);
        return 1;
    }

    win.info(std::string("DIRSCAN v1.0 — dsgui ") + dsg::versionString());
    win.info("Pick a folder and click Scan.");

    win.app().onClose([]() {
        if (g_ctx.running.load()) {
            g_ctx.cancel = true;
            Sleep(50);
        }
        if (g_thread) {
            WaitForSingleObject(g_thread, 2000);
            CloseHandle(g_thread);
            g_thread = nullptr;
        }
    });

    int rc = win.run();
    g_win = nullptr;
    return rc;
}