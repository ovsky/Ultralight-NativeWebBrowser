#include "SidecarProcess.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

static fs::path FindHeavyRendererBinary() {
  // Simple heuristics: look in ~/.local/share/MyApp/bin or next to the current exe
  fs::path bin;
#if defined(_WIN32)
  char buffer[MAX_PATH];
  GetModuleFileNameA(NULL, buffer, MAX_PATH);
  fs::path exePath = fs::path(buffer).parent_path();
  fs::path candidate = exePath / "HeavyRenderer.exe";
  if (fs::exists(candidate)) return candidate;
  const char *localApp = getenv("LOCALAPPDATA");
  if (localApp) {
    fs::path p(localApp);
    fs::path localbin = p / "MyApp" / "bin" / "HeavyRenderer.exe";
    if (fs::exists(localbin)) return localbin;
  }
  return candidate; // fallback - caller should check exists
#else
  const char *home = getenv("HOME");
  fs::path p(home ? home : ".");
  fs::path candidate = p / ".local" / "share" / "MyApp" / "bin" / "HeavyRenderer";
  // Also check next to executable
  fs::path exePath = fs::path("./HeavyRenderer");
  if (fs::exists(exePath)) return exePath;
  if (fs::exists(candidate)) return candidate;
  return candidate;
#endif
}

uint64_t SidecarProcess::LaunchAndEmbed(void *parentWindowHandle, const std::string &url) {
#ifdef ENABLE_DRM_SIDECAR
  fs::path heavy = FindHeavyRendererBinary();
  if (!fs::exists(heavy)) {
    std::cerr << "HeavyRenderer binary not found at " << heavy << std::endl;
    return 0;
  }

  std::ostringstream ss;
#if defined(_WIN32)
  // Convert parentWindowHandle to numeric string value
  uintptr_t hwndVal = reinterpret_cast<uintptr_t>(parentWindowHandle);
  ss << "\"" << heavy.string() << "\"" << " --parent-window-handle=" << hwndVal << " --url=\"" << url << "\"";

  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));
  std::string cmd = ss.str();
  if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
    std::cerr << "Failed to create process for heavy renderer" << std::endl;
    return 0;
  }
  uint64_t pid = static_cast<uint64_t>(pi.dwProcessId);
  // We can safely close thread handle (we don't need it). Keep process handle closed as we'll monitor by PID.
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return pid;
#else
  pid_t pid = fork();
  if (pid == 0) {
    // child
    std::vector<char *> args;
    std::string parentArg = "--parent-window-handle=" + std::to_string(reinterpret_cast<uintptr_t>(parentWindowHandle));
    args.push_back(const_cast<char *>(heavy.c_str()));
    args.push_back(const_cast<char *>(parentArg.c_str()));
    std::string urlArg = "--url=" + url;
    args.push_back(const_cast<char *>(urlArg.c_str()));
    args.push_back(NULL);
    execv(heavy.c_str(), args.data());
    _exit(EXIT_FAILURE);
  } else if (pid < 0) {
    std::cerr << "Failed to fork heavy renderer process" << std::endl;
    return 0;
  } else {
    // parent continues
    // optionally wait or detach; we'll detach
    return static_cast<uint64_t>(pid);
  }
#endif
  return 0;
#else
  (void)parentWindowHandle; (void)url;
  // DRM sidecar disabled: nothing to do
  return 0;
#endif
}
