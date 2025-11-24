#pragma once

#include <functional>
#include <string>

class DependencyManager {
public:
  // Ensures heavy engine components are ready for a DRM-protected page.
  // Returns true on success.
  // progressCallback: called with values in [0,1] to indicate progress.
  // statusCallback: optional textual status updates (human-readable) emitted at major steps.
  static bool EnsureHeavyEngineReady(std::function<void(float)> progressCallback,
                                     std::function<void(const std::string &)> statusCallback = nullptr);
};
