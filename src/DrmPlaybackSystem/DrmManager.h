#pragma once

#include <string>
#include "DrmConfig.h"
#include "NativeDrmWindow.h"

class DrmManager
{
public:
    DrmManager();

    // Returns true if navigation was handled (native window opened) and Ultralight should stop.
    bool TryHandleNavigation(const std::string &url);

    DrmConfig &Config() { return config_; }
    NativeDrmWindow &Window() { return window_; }

private:
    DrmConfig config_;
    NativeDrmWindow window_;
};
