#pragma once

#include <string>
#include <mutex>
#include <unordered_map>
#include <nlohmann/json.hpp>

class DrmConfig
{
public:
    DrmConfig(const std::string &path = "drm_config.json");
    void Load();
    void Save();

    bool IsDrmEnabledForUrl(const std::string &url);
    void SetGlobalDrm(bool enabled);
    bool IsGlobalDrmEnabled() const;

    // Accessors for UI or other systems
    void SetForceNativeWebview(bool v);
    bool GetForceNativeWebview() const;

private:
    std::string ExtractDomain(const std::string &url) const;

    mutable std::mutex mutex_;
    nlohmann::json json_;
    std::string path_;
};
