#pragma once

#include <string>
#include <atomic>

#include <mutex>
class NativeDrmWindow
{
public:
    NativeDrmWindow();
    ~NativeDrmWindow();

    // Open a native OS webview window to play DRM content.
    void OpenWindow(const std::string &url, const std::string &title);

    // Start downloading platform-specific dependencies (e.g., webview header or runtimes).
    void StartDependencyDownload();

    // Return JSON string with last dependency info: {"url":"...","target":"...","in_progress":true}
    std::string GetDependencyInfoJson() const;
    bool IsDownloadInProgress() const { return downloadInProgress_.load(); }

private:
    mutable std::mutex dep_mutex_;
    std::string dep_url_;
    std::string dep_target_;
    std::atomic<int> dep_exit_code_{-1};
    std::atomic<uint64_t> dep_bytes_downloaded_{0};
    std::atomic<uint64_t> dep_bytes_total_{0};
    std::atomic<bool> downloadInProgress_{false};
};
