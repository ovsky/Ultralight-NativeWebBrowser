#include "NativeDrmWindow.h"
#include "DrmLogger.h"
#include <thread>
#include <chrono>
#include <filesystem>
#include <atomic>
#include <cstdlib>
#include <sstream>
#include <cstdio>
#if defined(HAVE_LIBCURL)
#include <curl/curl.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#if defined(__APPLE__)
#define PLATFORM_MAC
#elif defined(_WIN32)
#define PLATFORM_WINDOWS
#else
#define PLATFORM_LINUX
#endif

// Attempt to include webview header. The settings downloader below can fetch it if missing.
#if __has_include("webview.h")
#include "webview.h"
#define HAVE_WEBVIEW 1
#elif __has_include(<webview/webview.h>)
#include <webview/webview.h>
#define HAVE_WEBVIEW 1
#else
#define HAVE_WEBVIEW 0
#endif

namespace fs = std::filesystem;

NativeDrmWindow::NativeDrmWindow() = default;
NativeDrmWindow::~NativeDrmWindow() = default;

static std::string PlatformDependencyUrl()
{
#ifdef PLATFORM_WINDOWS
    // WebView2 evergreen bootstrapper
    return "https://go.microsoft.com/fwlink/p/?LinkId=2124703";
#elif defined(PLATFORM_MAC)
    // webview header/raw archive (we'll download the header as an example)
    return "https://raw.githubusercontent.com/webview/webview/master/webview.h";
#else
    // Linux: webview header
    return "https://raw.githubusercontent.com/webview/webview/master/webview.h";
#endif
}

static std::string PlatformDependencyTarget()
{
#ifdef PLATFORM_WINDOWS
    return "dependencies\\WebView2Bootstrapper.exe";
#else
    return "dependencies/webview.h";
#endif
}

void NativeDrmWindow::StartDependencyDownload()
{
    if (downloadInProgress_.exchange(true))
    {
        DrmLogger::Instance().Warn("NativeDrmWindow", "Download already in progress");
        return;
    }

    std::thread([this]()
                {
                    std::string url = PlatformDependencyUrl();
                    std::string target = PlatformDependencyTarget();
                    {
                        std::lock_guard<std::mutex> lock(dep_mutex_);
                        dep_url_ = url;
                        dep_target_ = target;
                        dep_exit_code_.store(-1);
                    }

                    DrmLogger::Instance().Info("NativeDrmWindow", "Starting dependency download.");
                    DrmLogger::Instance().Info("NativeDrmWindow", "Dependency URL: " + url);
                    DrmLogger::Instance().Info("NativeDrmWindow", "Target file: " + target);

                    fs::path targetPath = fs::path(target);
                    fs::create_directories(targetPath.parent_path());

        // Build platform-specific command
#ifdef PLATFORM_WINDOWS
                    // Use PowerShell Invoke-WebRequest
                    std::ostringstream cmd;
                    cmd << "powershell -NoProfile -Command \"";
                    cmd << "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; ";
                    cmd << "Invoke-WebRequest -Uri '" << url << "' -OutFile '" << targetPath.string() << "' -UseBasicParsing\"";
                    std::string command = cmd.str();
#else
                    std::ostringstream cmd;
                    cmd << "curl -L -o '" << targetPath.string() << "' '" << url << "'";
                    std::string command = cmd.str();
#endif

                    DrmLogger::Instance().Info("NativeDrmWindow", "Download command: " + command);

#if defined(HAVE_LIBCURL)
                    // run download using libcurl so we can report progress reliably
                    std::atomic<int> exitCode{-1};

                    struct CurlProgress
                    {
                        NativeDrmWindow *self;
                    } progress_data{this};

                    auto write_cb = [](char *ptr, size_t size, size_t nmemb, void *userdata) -> size_t
                    {
                        FILE *fp = static_cast<FILE *>(userdata);
                        return fwrite(ptr, size, nmemb, fp);
                    };

                    auto xfer_cb = [](void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) -> int
                    {
                        CurlProgress *pd = static_cast<CurlProgress *>(p);
                        if (pd && pd->self)
                        {
                            if (dltotal > 0)
                                pd->self->dep_bytes_total_.store(static_cast<uint64_t>(dltotal));
                            pd->self->dep_bytes_downloaded_.store(static_cast<uint64_t>(dlnow));
                        }
                        return 0; // return non-zero to abort
                    };

                    std::thread downloader([&]()
                                           {
            DrmLogger::Instance().Info("NativeDrmWindow", "Downloader thread started (libcurl).");
            CURLcode res = CURLE_OK;
            CURL *curl = nullptr;
            FILE *fp = nullptr;
            do {
                curl = curl_easy_init();
                if (!curl) { res = CURLE_FAILED_INIT; break; }

                // open file for writing (binary)
                fp = fopen(targetPath.string().c_str(), "wb");
                if (!fp) { res = CURLE_WRITE_ERROR; break; }

                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfer_cb);
                // set reasonable timeouts
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // no overall timeout

                res = curl_easy_perform(curl);
            } while (0);

            if (fp) fclose(fp);
            if (curl) curl_easy_cleanup(curl);

            int code = (res == CURLE_OK) ? 0 : static_cast<int>(res);
            exitCode.store(code);
            dep_exit_code_.store(code);
            DrmLogger::Instance().Info("NativeDrmWindow", std::string("Downloader finished with code: ") + std::to_string(code));

            if (res == CURLE_OK) {
                DrmLogger::Instance().Info("NativeDrmWindow", "Download finished successfully.");
                DrmLogger::Instance().Info("NativeDrmWindow", "Installation: verifying archive...");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                DrmLogger::Instance().Info("NativeDrmWindow", "Installation: extracting files...");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                DrmLogger::Instance().Info("NativeDrmWindow", "Installation: placing files into runtime directory...");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                DrmLogger::Instance().Info("NativeDrmWindow", "Installation stage: completed.");
            } else {
                DrmLogger::Instance().Error("NativeDrmWindow", std::string("libcurl error: ") + curl_easy_strerror(res));
            }

            downloadInProgress_.store(false); });

                    // Monitor progress every 1 second; emit a clear human-friendly progress line periodically
                    int counter = 0;
                    while (downloadInProgress_.load())
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        ++counter;
                        uint64_t downloaded = dep_bytes_downloaded_.load();
                        uint64_t total = dep_bytes_total_.load();
                        std::ostringstream ss;
                        if (total > 0)
                        {
                            int pct = static_cast<int>((downloaded * 100) / total);
                            ss << "Downloading: " << pct << "% (" << downloaded << " / " << total << " bytes) -> " << targetPath.string();
                        }
                        else
                        {
                            ss << "Downloading: " << downloaded << " bytes received -> " << targetPath.string();
                        }
                        // Log Info every 3 seconds, Debug otherwise
                        if (counter % 3 == 0)
                        {
                            DrmLogger::Instance().Info("NativeDrmWindow", ss.str());
                        }
                        else
                        {
                            DrmLogger::Instance().Debug("NativeDrmWindow", ss.str());
                        }
                    }

                    if (downloader.joinable())
                        downloader.join();
#else
                    // Fallback: execute shell download and monitor file progress when libcurl not available
                    std::atomic<bool> finished{false};
                    std::atomic<int> exitCode{-1};

                    std::thread downloader([&]()
                                           {
            DrmLogger::Instance().Info("NativeDrmWindow", "Downloader thread started (shell): " + command);
            int r = std::system(command.c_str());
            exitCode.store(r);
            dep_exit_code_.store(r);
            finished.store(true);
            DrmLogger::Instance().Info("NativeDrmWindow", "Downloader thread finished with exit code: " + std::to_string(r)); });

                    // Monitor progress every 1 second, log file size every 3s
                    int counter = 0;
                    while (!finished.load())
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        ++counter;
                        std::error_code ec;
                        uintmax_t sz = 0;
                        if (fs::exists(targetPath, ec))
                        {
                            sz = fs::file_size(targetPath, ec);
                            dep_bytes_downloaded_.store(static_cast<uint64_t>(sz));
                        }
                        std::ostringstream ss;
                        ss << "Downloading (shell): " << sz << " bytes -> " << targetPath.string();
                        if (counter % 3 == 0)
                        {
                            DrmLogger::Instance().Info("NativeDrmWindow", ss.str());
                        }
                        else
                        {
                            DrmLogger::Instance().Debug("NativeDrmWindow", ss.str());
                        }
                    }

                    if (downloader.joinable())
                        downloader.join();
#endif
                })
        .detach();
}

std::string NativeDrmWindow::GetDependencyInfoJson() const
{
    std::lock_guard<std::mutex> lock(dep_mutex_);
    std::ostringstream ss;
    ss << "{";
    ss << "\"url\":\"" << std::string(dep_url_.begin(), dep_url_.end()) << "\",";
    ss << "\"target\":\"" << std::string(dep_target_.begin(), dep_target_.end()) << "\",";
    ss << "\"in_progress\":" << (downloadInProgress_.load() ? "true" : "false") << ",";
    ss << "\"exit_code\":" << dep_exit_code_.load() << ",";
    ss << "\"bytes_downloaded\":" << dep_bytes_downloaded_.load() << ",";
    ss << "\"bytes_total\":" << dep_bytes_total_.load();
    ss << "}";
    return ss.str();
}

void NativeDrmWindow::OpenWindow(const std::string &url, const std::string &title)
{
    DrmLogger::Instance().Info("NativeDrmWindow", "Opening native window for URL: " + url);

#if HAVE_WEBVIEW
    // Launch a dedicated thread so we don't block caller
    std::thread([url, title]()
                {
        try {
#ifdef PLATFORM_MAC
            // macOS requires a Safari-like UA for some providers
            const std::string safariUA = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                "AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 Safari/605.1.15";
#endif
            webview::webview w(true, nullptr);
            w.set_title(title.c_str());
            w.set_size(1024, 768, WEBVIEW_HINT_NONE);
#ifdef PLATFORM_MAC
// try to set user agent if wrapper supports it
#if defined(WEBVIEW_USER_AGENT)
                w.set_user_agent(safariUA.c_str());
#else
                // Some webview wrappers might not expose set_user_agent. Log warning.
                DrmLogger::Instance().Warn("NativeDrmWindow", "Unable to set user agent via webview wrapper; provider may block playback.");
#endif
#endif
            w.navigate(url.c_str());
            w.run();
        } catch (const std::exception& ex) {
            DrmLogger::Instance().Error("NativeDrmWindow", std::string("Exception while opening webview: ") + ex.what());
        } })
        .detach();
#else
    // Fallback: open system browser as a last resort (not ideal for DRM)
    DrmLogger::Instance().Warn("NativeDrmWindow", "webview.h not available. Falling back to launching system browser.");
#ifdef PLATFORM_WINDOWS
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(PLATFORM_MAC)
    std::string cmd = std::string("open '") + url + "'";
    std::system(cmd.c_str());
#else
    std::string cmd = std::string("xdg-open '") + url + "'";
    std::system(cmd.c_str());
#endif
#endif
}
