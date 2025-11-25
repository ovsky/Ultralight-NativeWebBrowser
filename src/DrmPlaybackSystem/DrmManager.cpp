#include "DrmManager.h"
#include "DrmLogger.h"

DrmManager::DrmManager() : config_("drm_config.json"), window_()
{
}

bool DrmManager::TryHandleNavigation(const std::string &url)
{
    DrmLogger::Instance().Info("DrmManager", "Checking URL compatibility...");

    if (!config_.IsGlobalDrmEnabled())
    {
        DrmLogger::Instance().Info("DrmManager", "DRM disabled globally. Returning false.");
        return false;
    }

    bool isDrm = config_.IsDrmEnabledForUrl(url);
    if (!isDrm && !config_.GetForceNativeWebview())
    {
        DrmLogger::Instance().Info("DrmManager", "Standard content. Returning false.");
        return false;
    }

    DrmLogger::Instance().Info("DrmManager", "DRM content detected. Launching Native View.");
    window_.OpenWindow(url, "Native DRM Playback");
    return true;
}
