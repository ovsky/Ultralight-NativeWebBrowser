#include "drm/DRMDependencyManager.h"

#if defined(_WIN32)

#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <sstream>
#include <string>
#include <cstdlib>

namespace
{
  bool DirectoryExists(const std::wstring &path)
  {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
  }

  std::wstring GetProgramFilesPath()
  {
    wchar_t buffer[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr, SHGFP_TYPE_CURRENT, buffer) == S_OK)
      return std::wstring(buffer);
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, SHGFP_TYPE_CURRENT, buffer) == S_OK)
      return std::wstring(buffer);
    return L"C:/Program Files";
  }

  std::wstring BuildCommand(const std::wstring &command)
  {
    std::wstringstream ss;
    ss << L"powershell -NoProfile -ExecutionPolicy Bypass -Command \"" << command << L"\"";
    return ss.str();
  }
}

namespace drm
{
class DependencyManagerWindows : public DRMDependencyManager
{
public:
  std::string GetName() const override { return "Microsoft Edge WebView2 Runtime"; }

  bool IsInstalled() const override
  {
    std::wstring base = GetProgramFilesPath();
    std::filesystem::path runtime_dir = std::filesystem::path(base) / "Microsoft" / "EdgeWebView" / "Application";
    return std::filesystem::exists(runtime_dir);
  }

  bool Install(const LogSink &log) override
  {
    if (log)
      log("Downloading WebView2 bootstrapper...");

    wchar_t temp_path[MAX_PATH];
    GetTempPathW(MAX_PATH, temp_path);
    std::filesystem::path bootstrapper = std::filesystem::path(temp_path) / "MicrosoftEdgeWebview2Setup.exe";

    std::wstringstream download_cmd;
    download_cmd << L"Invoke-WebRequest -Uri 'https://go.microsoft.com/fwlink/?linkid=2124703' -OutFile '" << bootstrapper.wstring() << L"'";
    int download_code = _wsystem(BuildCommand(download_cmd.str()).c_str());
    if (download_code != 0)
    {
      if (log)
        log("Failed to download WebView2 runtime (Invoke-WebRequest exited with code " + std::to_string(download_code) + ")");
      return false;
    }

    if (log)
      log("Running WebView2 installer...");

    std::wstring install_cmd = L"\"" + bootstrapper.wstring() + L"\" /silent /install";
    int install_code = _wsystem(install_cmd.c_str());
    if (install_code != 0)
    {
      if (log)
        log("WebView2 installer returned exit code " + std::to_string(install_code));
      return false;
    }

    if (log)
      log("WebView2 runtime installation completed.");

    return true;
  }
};

std::unique_ptr<DRMDependencyManager> CreateWindowsDependencyManager()
{
  return std::make_unique<DependencyManagerWindows>();
}

} // namespace drm

#endif
