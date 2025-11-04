#include "AdBlocker.h"

#include <Ultralight/Ultralight.h>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace ultralight;

namespace
{
    bool ends_with_dot_or_exact(const std::string &host, const std::string &rule)
    {
        // Match exact domain or subdomain ("example.com" matches example.com and a.example.com but not badexample.com)
        if (host == rule)
            return true;
        if (host.size() <= rule.size())
            return false;
        // Ensure boundary on the left is a dot
        size_t pos = host.size() - rule.size();
        if (host.compare(pos, std::string::npos, rule) == 0 && pos > 0 && host[pos - 1] == '.')
            return true;
        return false;
    }
}

bool AdBlocker::LoadDefaultBlocklist(const std::string &path)
{
    std::ifstream in(path);
    if (!in.is_open())
        return false;

    std::lock_guard<std::mutex> lock(mtx_);
    blocked_hosts_.clear();
    url_substrings_.clear();

    std::string line;
    while (std::getline(in, line))
    {
        line = Trim(line);
        if (line.empty())
            continue;
        if (line[0] == '#')
            continue;

        // Adblock-style domain pattern: ||example.com^
        if (line.rfind("||", 0) == 0)
        {
            std::string dom = line.substr(2);
            // Trim at first '^' if present
            auto hat = dom.find('^');
            if (hat != std::string::npos)
                dom = dom.substr(0, hat);
            dom = Trim(dom);
            if (!dom.empty())
                AddBlockedHost(dom);
            continue;
        }

        // Hosts-file style: IP then domain
        {
            std::istringstream iss(line);
            std::string tok1, tok2;
            if (iss >> tok1)
            {
                if (tok1.find('.') != std::string::npos || tok1 == "::1")
                {
                    if (iss >> tok2)
                    {
                        AddBlockedHost(tok2);
                        continue;
                    }
                }
            }
        }

        // Plain domain without IP
        if (line.find('.') != std::string::npos && line.find(' ') == std::string::npos)
        {
            AddBlockedHost(line);
            continue;
        }

        // Fallback: treat as URL substring rule
        AddURLSubstring(line);
    }

    return true;
}

void AdBlocker::Clear()
{
    std::lock_guard<std::mutex> lock(mtx_);
    blocked_hosts_.clear();
    url_substrings_.clear();
}

bool AdBlocker::OnNetworkRequest(View * /*caller*/, NetworkRequest &request)
{
    // Always allow file/data schemes and about:blank, etc.
    auto proto = request.urlProtocol().utf8();
    if (proto == "file" || proto == "data" || proto == "about")
        return true;

    auto host_ul = request.urlHost();
    auto url_ul = request.url();
    auto host = ToLower(std::string(host_ul.utf8().data()));
    auto url = ToLower(std::string(url_ul.utf8().data()));

    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!host.empty() && IsBlockedHost(host))
        {
            return false; // Block by domain
        }
        if (!url.empty() && IsBlockedURL(url))
        {
            return false; // Block by simple substring
        }
    }

    return true; // Allow
}

void AdBlocker::AddBlockedHost(const std::string &host_raw)
{
    std::string h = ToLower(Trim(host_raw));
    if (h.empty())
        return;
    // strip leading dots
    while (!h.empty() && h.front() == '.')
        h.erase(h.begin());
    blocked_hosts_.insert(h);
}

void AdBlocker::AddURLSubstring(const std::string &needle_raw)
{
    std::string n = ToLower(Trim(needle_raw));
    if (n.empty())
        return;
    url_substrings_.push_back(n);
}

bool AdBlocker::IsBlockedHost(const std::string &host) const
{
    for (const auto &rule : blocked_hosts_)
    {
        if (ends_with_dot_or_exact(host, rule))
            return true;
    }
    return false;
}

bool AdBlocker::IsBlockedURL(const std::string &url) const
{
    for (const auto &needle : url_substrings_)
    {
        if (url.find(needle) != std::string::npos)
            return true;
    }
    return false;
}

std::string AdBlocker::ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                   { return (char)std::tolower(c); });
    return s;
}

std::string AdBlocker::Trim(const std::string &s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}
