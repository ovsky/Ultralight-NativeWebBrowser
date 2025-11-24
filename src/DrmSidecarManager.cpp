// DrmSidecarManager.cpp
#include "DrmSidecarManager.h"

#include <iostream>
#include <thread>
#include <filesystem>
#include <fstream>
#include <vector>
#include <chrono>
#include <sstream>
#include <cstdio>
#include <cctype>
#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif
#endif

using namespace std::chrono_literals;
namespace fs = std::filesystem;

// Helper: run a command and capture its stdout (single string). Uses popen.
static std::string RunCommandCapture(const std::string &cmd) {
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

#ifdef HAVE_LIBCURL
// Helper: download text into a string using libcurl
static bool CurlDownloadToString(const std::string &url, std::string &out) {
    CURL *curl = curl_easy_init();
    if (!curl) return false;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
        std::string *s = (std::string*)userdata;
        s->append(ptr, size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK);
}
#endif

// Compute SHA256 of a file using platform tools (sha256sum or CertUtil)
static std::string ComputeFileSha256(const std::string &path) {
    std::string cmd;
#ifdef _WIN32
    // CertUtil prints hex groups with spaces; we'll parse it
    cmd = "certutil -hashfile \"" + path + "\" SHA256";
    std::string out = RunCommandCapture(cmd);
    // Find hex-like line
    std::istringstream iss(out);
    std::string line;
    while (std::getline(iss, line)) {
        // Remove spaces
        std::string t;
        for (char c : line) if (isxdigit((unsigned char)c)) t.push_back(c);
        if (t.size() == 64) return t;
    }
    return std::string();
#else
    cmd = "sha256sum '" + path + "' 2>/dev/null";
    std::string out = RunCommandCapture(cmd);
    if (out.empty()) return std::string();
    std::istringstream iss(out);
    std::string hash; iss >> hash;
    return hash;
#endif
}

DrmSidecarManager::DrmSidecarManager()
{
    // Determine installation path per-platform
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path)))
    {
        std::wstring base(path);
        std::wstring p = base + L"\\UltralightSidecar";
        std::string utf8Path;
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, p.c_str(), (int)p.size(), NULL, 0, NULL, NULL);
        utf8Path.resize(size_needed);
        WideCharToMultiByte(CP_UTF8, 0, p.c_str(), (int)p.size(), &utf8Path[0], size_needed, NULL, NULL);
        m_installationPath = utf8Path;
    }
    else
    {
        m_installationPath = "C:\\UltralightSidecar";
    }
#elif __APPLE__
    const char *home = getenv("HOME");
    if (!home)
        home = "/";
    m_installationPath = std::string(home) + "/Library/Application Support/UltralightSidecar";
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg)
        m_installationPath = std::string(xdg) + "/ultralight-sidecar";
    else if (home)
        m_installationPath = std::string(home) + "/.config/ultralight-sidecar";
    else
        m_installationPath = "/tmp/ultralight-sidecar";
#endif
}

DrmSidecarManager::~DrmSidecarManager()
{
    StopSidecar();
}

void DrmSidecarManager::Log(const std::string &msg)
{
    std::cout << "[DrmSidecar] " << msg << std::endl;
}

bool DrmSidecarManager::PathExists(const std::string &path) const
{
    try
    {
        return fs::exists(path);
    }
    catch (...)
    {
        return false;
    }
}

bool DrmSidecarManager::IsSidecarInstalled()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_installationPath.empty())
        return false;
#ifdef _WIN32
    std::string exe = m_installationPath + "\\sidecar.exe";
    return PathExists(exe);
#elif __APPLE__
    std::string exe = m_installationPath + "/sidecar";
    return PathExists(exe);
#else
    std::string exe = m_installationPath + "/sidecar";
    return PathExists(exe);
#endif
}

std::string DrmSidecarManager::DetermineOsArch() const
{
#ifdef _WIN32
#if defined(_M_X64) || defined(__x86_64__)
    return "windows-amd64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "windows-arm64";
#else
    return "windows-amd64";
#endif
#elif __APPLE__
#if defined(__aarch64__)
    return "mac-arm64";
#else
    return "mac-amd64";
#endif
#else
#if defined(__aarch64__)
    return "linux-arm64";
#else
    return "linux-amd64";
#endif
#endif
}

std::string DrmSidecarManager::MakeDownloadUrl(const std::string &osArch) const
{
    // Pseudo-URL generator. In production replace with actual GitHub Release URL template.
    // Example: https://github.com/owner/repo/releases/download/v1.0/sidecar-windows-amd64.zip
    std::ostringstream ss;
    ss << "https://github.com/yourorg/ultralight-sidecar/releases/latest/download/sidecar-" << osArch << ".zip";
    return ss.str();
}

std::string DrmSidecarManager::GetDownloadUrl() const
{
    std::string osArch = DetermineOsArch();
    return MakeDownloadUrl(osArch);
}

void DrmSidecarManager::InstallSidecar(std::function<void(float)> progress)
{
    // Install runs async. We'll simulate a download + extract sequence.
    std::thread([this, progress]()
                {
                    Log("Starting sidecar installation...");
                    if (progress)
                        progress(0.0f);

                    std::string osArch = DetermineOsArch();
                    std::string url = MakeDownloadUrl(osArch);
                    Log("Resolved download URL: " + url);

                    // Persist last download URL for reference (UI may read it if needed)
                    try
                    {
                        std::ofstream f(m_installationPath + "/last_download_url.txt");
                        f << url << std::endl;
                    }
                    catch (...)
                    {
                    }

                    // Ensure install directory exists
                    try
                    {
                        fs::create_directories(m_installationPath);
                    }
                    catch (const std::exception &e)
                    {
                        Log(std::string("Failed to create install dir: ") + e.what());
                        if (progress)
                            progress(0.0f);
                        return;
                    }

                    // Destination paths
                    std::string archivePath = m_installationPath + "/sidecar.zip";

        // Use libcurl if available for robust download with progress reporting.
#ifdef HAVE_LIBCURL
                    Log("Downloading sidecar via libcurl: " + url);
                    if (progress)
                        progress(0.01f);

                    curl_global_init(CURL_GLOBAL_DEFAULT);
                    CURL *curl = curl_easy_init();
                    if (!curl)
                    {
                        Log("libcurl initialization failed. Falling back to system download.");
                    }
                    else
                    {
                        FILE *fp = nullptr;
#ifdef _WIN32
                        fopen_s(&fp, archivePath.c_str(), "wb");
#else
                        fp = fopen(archivePath.c_str(), "wb");
#endif
                        if (!fp)
                        {
                            Log("Failed to open target archive file for writing: " + archivePath);
                            curl_easy_cleanup(curl);
                            curl_global_cleanup();
                            if (progress)
                                progress(0.0f);
                            return;
                        }

                        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t
                                         {
                FILE* f = (FILE*)userdata;
                return fwrite(ptr, size, nmemb, f); });
                        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

                        // Progress callback
                        struct ProgressCtx
                        {
                            std::function<void(float)> cb;
                        } pctx{progress};
                        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, +[](void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) -> int
                                         {
                ProgressCtx* ctx = (ProgressCtx*)p;
                if (ctx && ctx->cb) {
                    if (dltotal > 0) {
                        double fraction = (double)dlnow / (double)dltotal;
                        ctx->cb(static_cast<float>(std::min(1.0, fraction)));
                    }
                }
                return 0; });
                        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &pctx);
                        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

                        CURLcode res = curl_easy_perform(curl);
                        fclose(fp);
                        if (res != CURLE_OK)
                        {
                            Log(std::string("libcurl download failed: ") + curl_easy_strerror(res));
                            curl_easy_cleanup(curl);
                            curl_global_cleanup();
                            if (progress)
                                progress(0.0f);
                            // fallback to system below
                        }
                        else
                        {
                            curl_easy_cleanup(curl);
                            curl_global_cleanup();
                            Log("Download complete (libcurl).");
                            // Attempt checksum verification if possible
                            try {
                                std::string checksumUrls[] = { url + ".sha256", url + ".sha256.txt", url + ".sha256sum" };
                                std::string expectedHash;
                                for (const auto &curlUrl : checksumUrls) {
                                    std::string txt;
                                    if (CurlDownloadToString(curlUrl, txt)) {
                                        std::istringstream iss(txt);
                                        std::string token;
                                        while (iss >> token) {
                                            std::string hex;
                                            for (char ch : token) if (isxdigit((unsigned char)ch)) hex.push_back(ch);
                                            if (hex.size() >= 64) { expectedHash = hex.substr(0,64); break; }
                                        }
                                    }
                                    if (!expectedHash.empty()) break;
                                }
                                if (!expectedHash.empty()) {
                                    Log("Found checksum; verifying...");
                                    std::string localHash = ComputeFileSha256(archivePath);
                                    if (localHash.empty()) {
                                        Log("Failed to compute local SHA256; aborting installation.");
                                        if (progress) progress(0.0f);
                                        return;
                                    }
                                    auto toLower = [](std::string s){ for (auto &c: s) c = (char)std::tolower((unsigned char)c); return s; };
                                    if (toLower(localHash) != toLower(expectedHash)) {
                                        Log("Checksum mismatch: expected=" + expectedHash + " local=" + localHash);
                                        if (progress) progress(0.0f);
                                        return;
                                    }
                                    Log("Checksum verified.");
                                } else {
                                    Log("No checksum available for download; skipping verification.");
                                }
                            } catch (...) {
                                Log("Checksum verification failed with exception; continuing.");
                            }
                        }
                    }
#else
                    // Fallback to the system-based approach if libcurl isn't available
                    {
                        Log("Downloading sidecar (attempting system curl). Will log progress.");
                        std::ostringstream cmd;
#ifdef _WIN32
                        // Use powershell Invoke-WebRequest as fallback if curl isn't present
                        cmd << "powershell -Command \"try { Invoke-WebRequest -Uri '" << url << "' -OutFile '" << archivePath << "' -UseBasicParsing } catch { exit 1 }\"";
#else
                        cmd << "curl -L -s -o '" << archivePath << "' '" << url << "'";
#endif
                        int rc = system(cmd.str().c_str());
                        if (rc != 0)
                        {
                            Log("Download command returned non-zero. Installation aborted.");
                            if (progress)
                                progress(0.0f);
                            return;
                        }
                    }
#endif

                    if (progress)
                        progress(0.6f);

                    // Extract archive -> Here we use system unzip or PowerShell Expand-Archive as a pragmatic approach.
                    Log("Extracting sidecar archive...");
                    {
#ifdef _WIN32
                        std::ostringstream cmd;
                        cmd << "powershell -Command \"Expand-Archive -Path '" << archivePath << "' -DestinationPath '" << m_installationPath << "' -Force\"";
                        int rc = system(cmd.str().c_str());
                        if (rc != 0)
                        {
                            Log("Failed to extract archive on Windows.");
                            if (progress)
                                progress(0.0f);
                            return;
                        }
#else
                        std::ostringstream cmd;
                        cmd << "unzip -o '" << archivePath << "' -d '" << m_installationPath << "'";
                        int rc = system(cmd.str().c_str());
                        if (rc != 0)
                        {
                            Log("Failed to extract archive on Linux/macOS. Ensure 'unzip' is installed.");
                            if (progress)
                                progress(0.0f);
                            return;
                        }
#endif
                    }

        // Optionally set executable bits on unix
#ifndef _WIN32
                    try
                    {
                        std::string exe = m_installationPath + "/sidecar";
                        fs::permissions(exe, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add);
                    }
                    catch (...)
                    {
                    }
#endif

                    if (progress)
                        progress(1.0f);
                    Log("Sidecar installed into: " + m_installationPath); })
        .detach();
}

void DrmSidecarManager::LaunchSidecar(void *parentNativeHandle, const std::string &url,
                                      int x, int y, int width, int height)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled)
    {
        Log("Sidecar feature is disabled; not launching.");
        return;
    }

    m_parentNativeHandle = parentNativeHandle;

#ifdef _WIN32
    std::string exePath = m_installationPath + "\\sidecar.exe";
    if (!PathExists(exePath))
    {
        Log("Sidecar executable not found: " + exePath);
        return;
    }

    // Build command line
    std::ostringstream cmdline;
    cmdline << '"' << exePath << '"'
            << " --url=\"" << url << "\""
            << " --parent-window-id=" << (uint64_t)parentNativeHandle
            << " --width=" << width
            << " --height=" << height;

    std::string cmd = cmdline.str();
    // CreateProcess wants writable buffer
    std::vector<wchar_t> wcmd;
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, NULL, 0);
    wcmd.resize(size_needed);
    MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, &wcmd[0], size_needed);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessW(NULL, &wcmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!ok)
    {
        DWORD err = GetLastError();
        Log(std::string("CreateProcess failed: ") + std::to_string(err));
        return;
    }

    m_childPid = pi.dwProcessId;
    m_childProcessHandle = (void *)pi.hProcess;
    // Close thread handle; keep process handle to allow termination
    CloseHandle(pi.hThread);

    Log(std::string("Launched sidecar process pid=") + std::to_string((uint64_t)pi.dwProcessId));

#else
    // Unix-like: fork + exec
    std::string exePath = m_installationPath + "/sidecar";
    if (!PathExists(exePath))
    {
        Log("Sidecar executable not found: " + exePath);
        return;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        Log("fork() failed");
        return;
    }

    if (pid == 0)
    {
        // Child
        // Ensure DISPLAY is set when launching GUI child (propagate from parent)
        const char *display = getenv("DISPLAY");
        if (display)
        {
            setenv("DISPLAY", display, 1);
        }

        std::vector<char *> argv;
        argv.push_back(const_cast<char *>(exePath.c_str()));

        std::string argUrl = std::string("--url=") + url;
        std::string argParent = std::string("--parent-window-id=") + std::to_string((uint64_t)parentNativeHandle);
        std::string argW = std::string("--width=") + std::to_string(width);
        std::string argH = std::string("--height=") + std::to_string(height);

        argv.push_back(const_cast<char *>(argUrl.c_str()));
        argv.push_back(const_cast<char *>(argParent.c_str()));
        argv.push_back(const_cast<char *>(argW.c_str()));
        argv.push_back(const_cast<char *>(argH.c_str()));
        argv.push_back(nullptr);

        // exec
        execv(exePath.c_str(), argv.data());
        // If execv returns, it's an error
        _exit(127);
    }

    // Parent
    m_childPid = pid;
    Log(std::string("Launched sidecar process pid=") + std::to_string((uint64_t)pid));
#endif
}

void DrmSidecarManager::ResizeSidecar(int x, int y, int width, int height)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_childPid)
        return;

    Log("Resizing sidecar to: " + std::to_string(x) + "," + std::to_string(y) + " " + std::to_string(width) + "x" + std::to_string(height));

#ifdef _WIN32
    if (m_parentNativeHandle)
    {
        HWND hwnd = (HWND)m_parentNativeHandle;
        // Send WM_USER message with packed params. Consumer (child) should handle it.
        // We pack x,y in wParam and width,height in lParam (two 16-bit values each) - limited to 16-bit fields; use COPYDATA for larger values if needed.
        struct ResizeData
        {
            int x, y, w, h;
        } rd{x, y, width, height};

        COPYDATASTRUCT cds;
        cds.dwData = 0x5244524D; // 'RDRM' marker
        cds.cbData = sizeof(rd);
        cds.lpData = &rd;

        // PostMessage so we don't block if child isn't responding
        PostMessage(hwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
    }
#elif __linux__
#ifdef __linux__
    // Use X11 ClientMessage to notify the child. Child should listen for the atom we use.
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy)
        return;
    Window parent = (Window)(uintptr_t)m_parentNativeHandle;

    Atom atom = XInternAtom(dpy, "ULTRALIGHT_SIDE_CAR_RESIZE", False);
    XClientMessageEvent ev;
    ev.type = ClientMessage;
    ev.window = parent;
    ev.message_type = atom;
    ev.format = 32;
    ev.data.l[0] = x;
    ev.data.l[1] = y;
    ev.data.l[2] = width;
    ev.data.l[3] = height;
    ev.data.l[4] = 0;

    XSendEvent(dpy, parent, False, NoEventMask, (XEvent *)&ev);
    XFlush(dpy);
    XCloseDisplay(dpy);
#endif
#endif
}

void DrmSidecarManager::StopSidecar()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_childPid)
        return;

#ifdef _WIN32
    if (m_childProcessHandle)
    {
        HANDLE h = (HANDLE)m_childProcessHandle;
        Log("Terminating sidecar process via TerminateProcess.");
        TerminateProcess(h, 0);
        WaitForSingleObject(h, 5000);
        CloseHandle(h);
        m_childProcessHandle = nullptr;
    }
    m_childPid.reset();
#else
    pid_t pid = (pid_t)(*m_childPid);
    Log(std::string("Killing sidecar pid=") + std::to_string(pid));
    kill(pid, SIGTERM);
    // Wait a short time then SIGKILL if still alive
    int status = 0;
    for (int i = 0; i < 10; ++i)
    {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid)
            break;
        std::this_thread::sleep_for(100ms);
    }
    m_childPid.reset();
#endif
}

void DrmSidecarManager::SetEnabled(bool enabled)
{
    m_enabled = enabled;
    // Persisting setting: write a simple settings file
    try
    {
        fs::create_directories(m_installationPath);
        std::ofstream f(m_installationPath + "/sidecar_settings.txt");
        f << "enabled=" << (enabled ? "1" : "0") << "\n";
    }
    catch (...)
    {
    }
}

bool DrmSidecarManager::IsEnabled() const { return m_enabled.load(); }

std::string DrmSidecarManager::GetInstallationPath() const { return m_installationPath; }
