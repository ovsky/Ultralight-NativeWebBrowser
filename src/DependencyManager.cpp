#include "DependencyManager.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#include <winreg.h>
#pragma comment(lib, "advapi32.lib")
#endif

#ifdef ENABLE_DRM_SIDECAR
#include <curl/curl.h>
#endif

#include <atomic>
#include <mutex>

namespace fs = std::filesystem;

static size_t write_data_to_file(void *ptr, size_t size, size_t nmemb, void *userdata) {
  FILE *f = (FILE *)userdata;
  size_t written = fwrite(ptr, size, nmemb, f);
  return written;
}

struct CurlXferData {
  std::function<void(float)> progressCb;
  std::function<void(const std::string &)> statusCb;
  float lastPct = -1.0f;
};

static int dm_curl_progress(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
  auto data = reinterpret_cast<CurlXferData *>(p);
  if (data && data->progressCb && dltotal > 0) {
    float progress = static_cast<float>(dlnow) / static_cast<float>(dltotal);
    data->progressCb(progress);
    // Also emit textual short progress messages every ~5% so UI logs see activity even if numeric handlers are not installed
    int pct = static_cast<int>(progress * 100.0f);
    if (data->statusCb && (pct % 5 == 0)) {
      // throttle to every 5% to reduce log spam
      if (data->lastPct < 0 || pct - static_cast<int>(data->lastPct * 100.0f) >= 5) {
        data->statusCb(std::string("Download progress: ") + std::to_string(pct) + std::string("%"));
        data->lastPct = progress;
      }
    }
  }
  return 0;
}

// statusCallback: optional textual status messages for UI/logging
static bool DownloadFile(const std::string &url, const fs::path &outPath, std::function<void(float)> progressCallback,
                         std::function<void(const std::string &)> statusCallback = nullptr) {
  CURL *curl = curl_easy_init();
  if (!curl) return false;
  FILE *fp = fopen(outPath.string().c_str(), "wb");
  if (!fp) {
    curl_easy_cleanup(curl);
    return false;
  }
  // Notify start (0%) and emit a detailed status message
  if (progressCallback) progressCallback(0.0f);
  if (statusCallback) statusCallback(std::string("Starting download: ") + url + " -> " + outPath.string());
  if (statusCallback) statusCallback(std::string("Starting download: ") + url + " -> " + outPath.string());
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_to_file);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
#if LIBCURL_VERSION_NUM >= 0x072000
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, dm_curl_progress);
#else
  curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, dm_curl_progress);
#endif
#if LIBCURL_VERSION_NUM >= 0x072000
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, dm_curl_progress);
#else
  // older API
  curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, dm_curl_progress);
#endif

  // Start a small helper thread that simulates progress when the server doesn't provide a content-length.
  std::atomic<bool> download_active(true);
  std::thread simThread;
  // We'll only spawn simThread if the server doesn't report a content length (dltotal not reliable). We detect this later.

  CURLcode res = CURLE_OK;
  // We'll set up XFER callback data to allow dm_curl_progress to emit status; also capture the pointer lifetime
  // Use a small block so xferData lives for the duration of curl_easy_perform
  {
    CurlXferData xferData{progressCallback, statusCallback, -1.0f};
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &xferData);
#if LIBCURL_VERSION_NUM < 0x072000
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &xferData);
#endif
    // Start simulation thread to emit pseudo-progress for servers that don't provide content-length
    if (progressCallback) {
      simThread = std::thread([&]() {
        try {
          float simulated = 0.0f;
          while (download_active.load()) {
            // If dm_curl_progress set xferData.lastPct (>=0), prefer that and exit simulation
            if (xferData.lastPct >= 0.0f) break;
            // increment simulated progress faster initially then slow down
            simulated = std::min(0.95f, simulated + 0.01f);
            try { if (xferData.progressCb) xferData.progressCb(simulated); } catch(...) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
          }
        } catch(...) {}
      });
    }
    // Start perform
    res = curl_easy_perform(curl);
    // stop sim if running
    download_active.store(false);
    if (simThread.joinable()) { try { simThread.join(); } catch(...) {} }
  }
  fclose(fp);
  curl_easy_cleanup(curl);
  if (res == CURLE_OK) {
    if (progressCallback) progressCallback(1.0f);
    try {
      uintmax_t size = fs::file_size(outPath);
      if (statusCallback) statusCallback(std::string("Download complete: ") + outPath.string() + " (" + std::to_string(size) + " bytes)");
    } catch (...) {
      if (statusCallback) statusCallback(std::string("Download complete: ") + outPath.string());
    }
    return true;
  }
  // On failure, report 0.0 to indicate no progress/completion
  if (progressCallback) progressCallback(0.0f);
  if (statusCallback) {
    const char *err = curl_easy_strerror(res);
    if (err)
      statusCallback(std::string("Download failed: ") + url + " (curl: " + err + ")");
    else
      statusCallback(std::string("Download failed: ") + url);
  }
  return false;
}

// Minimal helper to extract 'data.tar.*' from a .deb/AR archive by scanning headers.
// If we find a data.tar.* we write it to destDataPath and return true.
static bool ExtractDataTarFromDeb(const fs::path &debPath, const fs::path &destDataPath) {
  std::ifstream in(debPath, std::ios::binary);
  if (!in) return false;
  // Quick fallback: try to find 'data.tar' bytes and then write remainder to file
  const std::string marker = "data.tar"; // will match data.tar.gz or data.tar.xz etc.
  std::string buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  size_t pos = buffer.find(marker);
  if (pos == std::string::npos) {
    return false;
  }
  // Try to get the compression suffix by checking the next few bytes
  size_t endPos = buffer.size();
  // We cannot parse real ar format fully here reliably, so we fallback to write the trailing bytes from 'data.tar' to the dest file.
  std::ofstream out(destDataPath, std::ios::binary);
  if (!out) return false;
  out.write(buffer.data() + pos, buffer.size() - pos);
  out.close();
  return true;
}

static bool ExtractTarUsingSystem(const fs::path &tarPath, const fs::path &destDir) {
  // destDir must exist
  if (!fs::exists(destDir)) fs::create_directories(destDir);
  std::string cmd = "tar -xf \"" + tarPath.string() + "\" -C \"" + destDir.string() + "\"";
  int code = std::system(cmd.c_str());
  return (code == 0);
}

static bool ensure_dir(const fs::path &p) {
  std::error_code ec;
  if (!fs::exists(p)) {
    return fs::create_directories(p, ec) && !ec;
  }
  return true;
}

#include <sstream>

bool DependencyManager::EnsureHeavyEngineReady(std::function<void(float)> progressCallback,
                                              std::function<void(const std::string &)> statusCallback) {
#ifdef ENABLE_DRM_SIDECAR
  // Use user-local bin cache
  fs::path localBin;
#if defined(_WIN32)
  PWSTR path = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path))) {
    localBin = fs::path(path) / "MyApp" / "bin";
    CoTaskMemFree(path);
  }
#elif defined(__APPLE__)
  const char *home = getenv("HOME");
  localBin = fs::path(home ? home : ".") / ".local" / "share" / "MyApp" / "bin";
#else
  const char *home = getenv("HOME");
  localBin = fs::path(home ? home : ".") / ".local" / "share" / "MyApp" / "bin";
#endif
  ensure_dir(localBin);

  // Shared state for heartbeat/status polling
  std::atomic<bool> heartbeat_done(false);
  std::string last_status;
  std::mutex status_mutex;

  // Helper to emit status both to statusCallback and to a local install log for diagnostics
  auto emitStatus = [&](const std::string &msg) {
    if (statusCallback) statusCallback(msg);
    try {
      fs::path logPath = localBin / "install.log";
      std::ofstream out(logPath.string(), std::ios::app);
      if (out) {
        // simple timestamp
        std::time_t t = std::time(nullptr);
        out << std::asctime(std::localtime(&t)) << ": " << msg << "\n";
        out.close();
      }
      // Also append to a plain ./install.log in the current working directory for convenience
      try {
        fs::path cwd_log = fs::current_path() / "install.log";
        std::ofstream out2(cwd_log.string(), std::ios::app);
        if (out2) {
          std::time_t t2 = std::time(nullptr);
          out2 << std::asctime(std::localtime(&t2)) << ": " << msg << "\n";
          out2.close();
        }
      } catch(...) {}
    } catch (...) {}
    // update last_status for heartbeat thread
    try {
      std::lock_guard<std::mutex> lk(status_mutex);
      last_status = msg;
    } catch(...) {}
  };

  // Start a background heartbeat thread that will emit a short status every 2 seconds
  // This ensures the UI sees periodic updates even during long-running blocking operations.
  std::thread heartbeat_thread([&]() {
    try {
      while (!heartbeat_done.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (heartbeat_done.load()) break;
        std::string snapshot;
        {
          std::lock_guard<std::mutex> lk(status_mutex);
          snapshot = last_status;
        }
        if (statusCallback) {
          if (!snapshot.empty()) statusCallback(std::string("Heartbeat: ") + snapshot);
          else statusCallback(std::string("Heartbeat: working..."));
        }
      }
    } catch(...) {}
  });

  // Ensure heartbeat thread is stopped/joined when this function exits (RAII)
  struct HeartbeatGuard {
    std::atomic<bool> &done; std::thread &thr;
    HeartbeatGuard(std::atomic<bool> &d, std::thread &t) : done(d), thr(t) {}
    ~HeartbeatGuard() {
      try { done.store(true); } catch(...) {}
      try { if (thr.joinable()) thr.join(); } catch(...) {}
    }
  } heartbeat_guard(heartbeat_done, heartbeat_thread);

  // Announce entry
  emitStatus("EnsureHeavyEngineReady: started");
  if (progressCallback) progressCallback(0.0f);

  // Helper to map a per-step [0..1] progress into a global progress range
  auto makeProgressMapper = [](std::function<void(float)> globalCb, float start, float weight) {
    return [globalCb, start, weight](float p) {
      if (globalCb) globalCb(start + p * weight);
    };
  };

#if defined(_WIN32)
  // Quick check: look for WebView2Loader.dll in local bin
  fs::path loader = localBin / "WebView2Loader.dll";
  bool loaderPresent = fs::exists(loader);

  // Basic registry check for WebView2 runtime (HKLM) - best-effort
  HKEY key = NULL;
  std::string subkey = "SOFTWARE\\Microsoft\\EdgeUpdate\\Clients";
  bool runtimeInstalled = false;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, KEY_READ, &key) == ERROR_SUCCESS) {
    // If this key exists, assume runtime present. (Simplified check)
    runtimeInstalled = true;
    RegCloseKey(key);
  }

  if (loaderPresent && runtimeInstalled) {
    emitStatus("WebView2 already present");
    if (progressCallback) progressCallback(1.0f);
    return true;
  }

  // Download the WebView2 Evergreen bootstrapper and run silently
  std::string url = "https://go.microsoft.com/fwlink/p/?LinkId=2124703"; // evergreen bootstrapper
  fs::path tmpDeb = localBin / "WebView2Bootstrapper.exe";
  emitStatus(std::string("Downloading WebView2 bootstrapper: ") + url + " -> " + tmpDeb.string());
  if (!DownloadFile(url, tmpDeb, makeProgressMapper(progressCallback, 0.0f, 0.85f), statusCallback)) {
    emitStatus("Failed to download WebView2 bootstrapper");
    return false;
  }

  SHELLEXECUTEINFOA ShExecInfo = {0};
  ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFOA);
  ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
  ShExecInfo.lpVerb = "open";
  ShExecInfo.lpFile = tmpDeb.string().c_str();
  ShExecInfo.lpParameters = "/silent /install";
  ShExecInfo.nShow = SW_HIDE;
  if (!ShellExecuteExA(&ShExecInfo)) {
    emitStatus(std::string("Failed to launch WebView2 installer: ") + tmpDeb.string());
    return false;
  }
  WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
  CloseHandle(ShExecInfo.hProcess);
  emitStatus(std::string("WebView2 installation finished (installer: ") + tmpDeb.string() + ")");
  if (progressCallback) progressCallback(1.0f);
  return true;

#elif defined(__linux__)
  // On Linux, check for libcef.so and libwidevinecdm.so under local bin
  fs::path libcef = localBin / "libcef.so";
  fs::path widevine = localBin / "libwidevinecdm.so";

  bool cefOk = fs::exists(libcef);
  bool widevineOk = fs::exists(widevine);
  bool needCef = !cefOk;

  // If widevine is missing, download official Chrome package and extract widevine
  if (!widevineOk) {
    // Determine arch
    std::string arch = "amd64";
    std::string unamecmd = "uname -m";
    FILE *p = popen(unamecmd.c_str(), "r");
    if (p) {
      char buf[128] = {0};
      if (fgets(buf, sizeof(buf), p)) {
        std::string m = buf;
        if (m.find("aarch64") != std::string::npos || m.find("arm64") != std::string::npos) {
          arch = "arm64";
        }
      }
      pclose(p);
    }

    std::string chromeUrl;
    if (arch == "amd64")
      chromeUrl = "https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb";
    else
      chromeUrl = "https://dl.google.com/linux/direct/google-chrome-stable_current_arm64.deb";

    fs::path tmpDir = fs::temp_directory_path() / "ultrawidevine";
    ensure_dir(tmpDir);
    fs::path tmpDeb = tmpDir / "chrome.deb";
    if (statusCallback) statusCallback(std::string("Downloading Chrome package for extraction: ") + chromeUrl);
    // If we also need CEF, split weight between widevine and CEF; otherwise widevine gets full range.
    float widevineWeight = 1.0f;
    bool needCefLocal = needCef; // captured flag for local logic
    if (needCefLocal) widevineWeight = 0.6f;

    // Download portion of widevine uses 75% of widevineWeight
    float dlPortion = 0.75f;
    float extractPortion = 1.0f - dlPortion;
    if (!DownloadFile(chromeUrl, tmpDeb, makeProgressMapper(progressCallback, 0.0f, widevineWeight * dlPortion), statusCallback)) {
      if (progressCallback) progressCallback(0.0f);
      if (statusCallback) statusCallback("Failed to download Chrome package");
      return false;
    }
    fs::path dataTar = tmpDir / "data.tar.xz";
    // Prefer dpkg-deb extraction if available
    bool extracted = false;
    if (statusCallback) statusCallback("Checking for dpkg-deb to extract Chrome package...");
    if (std::system("command -v dpkg-deb >/dev/null 2>&1") == 0) {
      // Attempt dpkg-deb -x to extract package contents directly
      fs::path dpkgExtractDir = tmpDir / "dpkg-data";
      ensure_dir(dpkgExtractDir);
      std::string cmd = "dpkg-deb -x \"" + tmpDeb.string() + "\" \"" + dpkgExtractDir.string() + "\"";
      if (statusCallback) statusCallback("Running dpkg-deb -x...");
      if (std::system(cmd.c_str()) == 0) {
        if (statusCallback) statusCallback("dpkg-deb extraction succeeded");
        // Attempt to find libwidevinecdm inside the extract directory
        dataTar = dpkgExtractDir; // treat as extracted dir
        extracted = true;
      } else {
        if (statusCallback) statusCallback("dpkg-deb extraction failed, will try fallbacks");
      }
    }
    if (!extracted) {
      if (!ExtractDataTarFromDeb(tmpDeb, dataTar)) {
    emitStatus("ExtractDataTarFromDeb failed, trying ar/tar fallback");
        // As fallback, try using system ar and tar commands if available
        std::string cmd = "ar x " + tmpDeb.string() + " -C " + tmpDir.string();
        std::system(cmd.c_str());
        // Find data.tar.* in tmpDir
        for (auto &e : fs::directory_iterator(tmpDir)) {
          std::string name = e.path().filename().string();
          if (name.rfind("data.tar", 0) == 0) {
            dataTar = e.path();
            break;
          }
        }
  emitStatus(std::string("Looking for data.tar.* in extracted ar contents"));
        // If still not found, try rpm2cpio fallback (if rpm present on system and the package downloaded is an RPM)
        if (!fs::exists(dataTar) && std::system("command -v rpm2cpio >/dev/null 2>&1") == 0) {
          emitStatus("Trying rpm2cpio fallback (if applicable)");
          fs::path candidateRpm = tmpDir / "chrome.rpm";
          try { fs::copy_file(tmpDeb, candidateRpm, fs::copy_options::overwrite_existing); } catch (...) {}
          std::string cmd = "rpm2cpio \"" + candidateRpm.string() + "\" | cpio -idmv -D \"" + tmpDir.string() + "\" 2>/dev/null";
          if (std::system(cmd.c_str()) == 0) {
            // Search extracted tree for data files
            for (auto &e : fs::recursive_directory_iterator(tmpDir)) {
              if (e.is_regular_file()) {
                std::string nm = e.path().filename().string();
                if (nm == "libwidevinecdm.so") {
                  // Found widevine; copy and mark found below
                  fs::path dest = localBin / "libwidevinecdm.so";
                  try { fs::copy_file(e.path(), dest, fs::copy_options::overwrite_existing); widevineOk = true; emitStatus("Copied libwidevinecdm.so from rpm2cpio extraction"); } catch(...) { emitStatus("Failed to copy libwidevinecdm.so from rpm2cpio extraction"); }
                  break;
                }
              }
            }
            dataTar = tmpDir;
          }
        }
      }
    }
    if (!fs::exists(dataTar) && !fs::exists(tmpDir / "dpkg-data")) {
      if (progressCallback) progressCallback(0.0f);
      emitStatus("Failed to locate data.tar in downloaded package and no dpkg-data available");
      return false;
    }

    // Extract dataTar to temp dir
  fs::path extractDir = tmpDir / "data";
    ensure_dir(extractDir);
    if (fs::is_directory(dataTar)) {
      // dpkg-deb extraction resulted in a folder; move extracted files into extractDir
      if (!fs::exists(extractDir)) ensure_dir(extractDir);
      for (auto &entry : fs::recursive_directory_iterator(dataTar)) {
        if (entry.is_regular_file()) {
          fs::path dest = extractDir / entry.path().filename();
          try { fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing); } catch (...) {}
        }
      }
      if (statusCallback) statusCallback(std::string("dpkg-deb extraction placed data into: ") + extractDir.string());
    } else {
  emitStatus(std::string("Extracting data tar: ") + dataTar.string());
        // Emit extraction progress mapped into the widevine remaining weight
        if (progressCallback) progressCallback(widevineWeight * dlPortion); // starting point for extraction
        if (!ExtractTarUsingSystem(dataTar, extractDir)) {
        emitStatus("Failed to extract data tar: " + dataTar.string());
          return false;
        }
        // After extraction, update to mid-extract progress
        if (progressCallback) progressCallback(widevineWeight * (dlPortion + extractPortion * 0.6f));

    // search for libwidevinecdm.so
    for (auto &entry : fs::recursive_directory_iterator(extractDir)) {
      if (entry.is_regular_file() && entry.path().filename() == "libwidevinecdm.so") {
        fs::path dest = localBin / "libwidevinecdm.so";
  try { fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing); emitStatus("Installed libwidevinecdm.so to local bin"); } catch (...) { emitStatus("Failed to copy libwidevinecdm.so to local bin"); }
        widevineOk = true;
        // Mark near-complete for widevine (copy step)
        if (progressCallback) progressCallback(widevineWeight * 0.95f);
        if (statusCallback) statusCallback(std::string("Located libwidevinecdm in extracted package: ") + entry.path().string());
        break;
      }
    }
    // After widevine work, set progress to widevineWeight (if not followed by CEF). If CEF is required we'll continue from widevineWeight.
    if (progressCallback && !needCef) progressCallback(1.0f);
  }

  // If CEF missing, download a prebuilt tarball placeholder and extract
  if (!cefOk) {
    std::string arch = "amd64";
    FILE *p = popen("uname -m", "r");
    if (p) {
      char buf[128] = {0};
      if (fgets(buf, sizeof(buf), p)) {
        std::string m = buf;
        if (m.find("aarch64") != std::string::npos || m.find("arm64") != std::string::npos) {
          arch = "arm64";
        }
      }
      pclose(p);
    }
    std::string cefUrl = "https://github.com/myrepo/releases/download/v1/cef_linux_" + arch + ".tar.gz";
    fs::path tmpDir = fs::temp_directory_path() / "ultracef";
    ensure_dir(tmpDir);
    fs::path tmpTar = tmpDir / "cef.tar.gz";
    emitStatus(std::string("Downloading CEF tarball: ") + cefUrl);
    // Compute CEF overall progress mapping: if widevine was also needed, allocate 40% to CEF starting at 60%.
    float cefOverallStart = 0.0f;
    float cefOverallWeight = 1.0f;
    if (!widevineOk && needCef) {
      cefOverallStart = 0.6f;
      cefOverallWeight = 0.4f;
    }
    // Give download 80% of cef weight, extraction 20%
    if (!DownloadFile(cefUrl, tmpTar, makeProgressMapper(progressCallback, cefOverallStart, cefOverallWeight * 0.8f), statusCallback)) {
      emitStatus("Failed to download CEF tarball");
      return false;
    }
    emitStatus(std::string("Extracting CEF to local bin: ") + localBin.string());
    if (!ExtractTarUsingSystem(tmpTar, localBin)) {
      emitStatus("Failed to extract CEF tarball to local bin");
      return false;
    }
    // Inform which (if any) libcef.so files are present now
    for (auto &e : fs::directory_iterator(localBin)) {
      if (e.is_regular_file() && e.path().filename() == "libcef.so") {
        if (statusCallback) statusCallback(std::string("libcef.so present at: ") + e.path().string());
        break;
      }
    }
    if (progressCallback) progressCallback(cefOverallStart + cefOverallWeight);
    cefOk = fs::exists(localBin / "libcef.so");
  }

  if (progressCallback) progressCallback(1.0f);
  return (cefOk && widevineOk);

#elif defined(__APPLE__)
  // On macOS treat similarly to Linux: check for libwidevinecdm.dylib and libcef.dylib
  fs::path libcef_dylib = localBin / "libcef.dylib";
  fs::path widevine_dylib = localBin / "libwidevinecdm.dylib";
  bool cefOk = fs::exists(libcef_dylib);
  bool widevineOk = fs::exists(widevine_dylib);
  if (!widevineOk) {
    // macOS extraction of widevine from Chrome PKG is more complex; we leave it as a no-op fallback
    // In actual implementation, we would download chrome package and extract the plug-in.
    // For now, return false to signal not available.
    return false;
  }
  if (!cefOk) {
    // Similar: download prebuilt macOS CEF and extract
    return false;
  }
  if (progressCallback) progressCallback(1.0f);
  return (cefOk && widevineOk);
#endif // end platform-specific (#if defined(_WIN32) / __linux__ / __APPLE__)

#else // ENABLE_DRM_SIDECAR not defined
  // If DRM sidecar support is disabled at build time, just return false so
  // NavigateMaybeHeavy will fall back to the builtin renderer.
  (void)progressCallback;
  (void)statusCallback;
  return false;
#endif // ENABLE_DRM_SIDECAR
}
