#include "DrmConfig.h"
#include "DrmLogger.h"
#include <fstream>
#include <regex>

using json = nlohmann::json;

DrmConfig::DrmConfig(const std::string &path) : path_(path)
{
    Load();
}

void DrmConfig::Load()
{
    std::ifstream ifs(path_);
    if (!ifs.good())
    {
        // create defaults (do not hold mutex while calling Save to avoid deadlock)
        json j;
        j["is_drm_globally_enabled"] = true;
        j["force_native_webview"] = false;
        j["known_drm_domains"] = json::object();
        j["known_drm_domains"]["netflix.com"] = true;
        j["known_drm_domains"]["spotify.com"] = true;
        j["known_drm_domains"]["youtube.com"] = false;
        j["known_drm_domains"]["disneyplus.com"] = true;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            json_ = j;
        }
        Save();
        DrmLogger::Instance().Info("DrmConfig", "No config found. Created default drm_config.json");
        return;
    }
    try
    {
        ifs >> json_;
    }
    catch (const std::exception &ex)
    {
        DrmLogger::Instance().Error("DrmConfig", std::string("Failed to parse config: ") + ex.what());
        // fallback to defaults
        json_ = json::object();
        json_["is_drm_globally_enabled"] = true;
        json_["force_native_webview"] = false;
        json_["known_drm_domains"] = json::object();
    }
}

void DrmConfig::Save()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream ofs(path_);
    if (!ofs.good())
    {
        DrmLogger::Instance().Error("DrmConfig", "Failed to open config file for writing");
        return;
    }
    ofs << json_.dump(4);
    ofs.flush();
}

std::string DrmConfig::ExtractDomain(const std::string &url) const
{
    // simple regex to extract host
    std::regex re(R"((?:https?:\/\/)?(?:[^@\n]+@)?(?:www\.)?([^:\/?#]+))", std::regex::icase);
    std::smatch m;
    if (std::regex_search(url, m, re) && m.size() > 1)
    {
        return m.str(1);
    }
    return std::string();
}

bool DrmConfig::IsDrmEnabledForUrl(const std::string &url)
{
    std::lock_guard<std::mutex> lock(mutex_);
    bool global = json_.value("is_drm_globally_enabled", true);
    if (!global)
    {
        DrmLogger::Instance().Info("DrmConfig", "User disabled DRM globally, ignoring request.");
        return false;
    }
    std::string domain = ExtractDomain(url);
    if (domain.empty())
    {
        DrmLogger::Instance().Debug("DrmConfig", "Could not extract domain from URL: " + url);
        return false;
    }
    // check exact domain match or subdomain
    auto known = json_.value("known_drm_domains", json::object());
    // direct match
    if (known.contains(domain))
    {
        return known[domain].get<bool>();
    }
    // check higher-level domain
    // e.g., www.netflix.com -> netflix.com already handled; for subsub domains like secure.netflix.com
    // find last two labels
    size_t pos = domain.find('.');
    if (pos != std::string::npos)
    {
        std::string tld = domain.substr(pos + 1);
        if (known.contains(tld))
            return known[tld].get<bool>();
    }
    return false;
}

void DrmConfig::SetGlobalDrm(bool enabled)
{
    std::lock_guard<std::mutex> lock(mutex_);
    json_["is_drm_globally_enabled"] = enabled;
    Save();
    DrmLogger::Instance().Info("DrmConfig", std::string("Set global DRM to ") + (enabled ? "enabled" : "disabled"));
}

bool DrmConfig::IsGlobalDrmEnabled() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return json_.value("is_drm_globally_enabled", true);
}

void DrmConfig::SetForceNativeWebview(bool v)
{
    std::lock_guard<std::mutex> lock(mutex_);
    json_["force_native_webview"] = v;
    Save();
}

bool DrmConfig::GetForceNativeWebview() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return json_.value("force_native_webview", false);
}
