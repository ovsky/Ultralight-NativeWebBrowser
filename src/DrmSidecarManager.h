// DrmSidecarManager.h
// Cross-platform manager for launching and managing a DRM "sidecar" process
// that hosts a CEF/Chromium renderer for Widevine-protected content.
//
// Note: This implementation focuses on process creation, handle passing,
// and high-level install/download scaffolding. Network download and archive
// extraction are implemented as stubs/pseudocode where platform-specific
// libraries would normally be used (curl, libarchive, WinHTTP, etc.).

#pragma once

#include <functional>
#include <string>
#include <atomic>
#include <mutex>
#include <optional>

class DrmSidecarManager
{
public:
    DrmSidecarManager();
    ~DrmSidecarManager();

    // Returns true when the sidecar binary is present in the app data/config folder
    bool IsSidecarInstalled();

    // Install the sidecar. `progress` should be called with values in [0,1].
    // This function does its work on a background thread and returns immediately.
    void InstallSidecar(std::function<void(float)> progress);

    // Return the download URL that would be used for the current platform/arch
    std::string GetDownloadUrl() const;

    // Launch the sidecar process and pass the native parent handle so the
    // child can embed its renderer into the UI container.
    // `parentNativeHandle` is an opaque native window handle (HWND on Windows, XID on X11, NSView* on macOS)
    void LaunchSidecar(void *parentNativeHandle, const std::string &url,
                       int x, int y, int width, int height);

    // Resize/relay a geometry update to the sidecar.
    void ResizeSidecar(int x, int y, int width, int height);

    // Stop / terminate the sidecar if running.
    void StopSidecar();

    // Toggle enabling the sidecar feature (persisted in a simple settings file).
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    // Get the installation path where the sidecar binary would be placed.
    std::string GetInstallationPath() const;

private:
    void Log(const std::string &msg);

    // Platform-specific helpers
    bool PathExists(const std::string &path) const;
    std::string DetermineOsArch() const;
    std::string MakeDownloadUrl(const std::string &osArch) const;

    // Process management
    std::mutex m_mutex;
    std::atomic<bool> m_enabled{true};
    std::optional<uint64_t> m_childPid; // pid on Unix, process id on Windows
    std::string m_installationPath;
    void *m_parentNativeHandle = nullptr;
    void *m_childProcessHandle = nullptr; // HANDLE on Windows, unused on unix (we keep pid)
};
