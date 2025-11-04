#pragma once
#include <Ultralight/Listener.h>
#include <Ultralight/NetworkRequest.h>
#include <unordered_set>
#include <vector>
#include <string>
#include <mutex>

// Basic ad/tracker blocker implementing Ultralight's NetworkListener.
//
// Features:
// - Domain-based blocking from a simple hosts/filter list
// - Optional URL-substring rules (very simple)
// - Allows file:// and data:// schemes unconditionally
//
// Blocklist format (any of the following per line):
// - "example.com"              (blocks example.com and all subdomains)
// - "0.0.0.0 example.com"     (hosts file style; ignores IP)
// - "127.0.0.1 example.com"   (hosts file style; ignores IP)
// - "||example.com^"          (Adblock-style domain rule; ^ terminator optional)
// - "*ads.js"                 (treated as a simple URL substring match)
// - Lines starting with '#' are comments; blank lines ignored.
class AdBlocker : public ultralight::NetworkListener
{
public:
    AdBlocker() = default;
    ~AdBlocker() override = default;

    // Load a blocklist from the given file path. Returns true if loaded.
    bool LoadDefaultBlocklist(const std::string &path);

    // Clear all rules
    void Clear();

    // NetworkListener override
    bool OnNetworkRequest(ultralight::View *caller, ultralight::NetworkRequest &request) override;

    // Add a blocked host (suffix-match, case-insensitive)
    void AddBlockedHost(const std::string &host);

    // Add a URL substring rule (case-insensitive)
    void AddURLSubstring(const std::string &needle);

    // Enable/disable blocking at runtime
    void set_enabled(bool enabled)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        enabled_ = enabled;
    }
    bool enabled() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return enabled_;
    }

private:
    bool IsBlockedHost(const std::string &host) const;
    bool IsBlockedURL(const std::string &url) const;

    static std::string ToLower(std::string s);
    static std::string Trim(const std::string &s);

    // Data structures
    std::unordered_set<std::string> blocked_hosts_; // host suffixes in lowercase (eg, "doubleclick.net")
    std::vector<std::string> url_substrings_;       // lowercase substrings

    mutable std::mutex mtx_;
    bool enabled_ = true;
};
