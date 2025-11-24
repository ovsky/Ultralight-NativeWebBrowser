#pragma once

#include <string>

class SidecarProcess {
public:
  // Launch the external heavy renderer sidecar and embed it into the parent
  // parentWindowHandle is platform-specific native window handle (HWND on Win, X11/WL handle on Linux/macOS)
  // Returns the PID of the launched process, or 0 if failure.
  static uint64_t LaunchAndEmbed(void *parentWindowHandle, const std::string &url);
};
