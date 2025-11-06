#include "DownloadManager.h"

#include <Ultralight/platform/Platform.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#include <cstdlib>
#else
#include <cstdlib>
#endif

#include <system_error>

namespace
{
    constexpr const char *kDefaultFilename = "download";
    constexpr const char *kDownloadsFolderName = "downloads";

    std::string ToStdString(const ultralight::String &str)
    {
        auto utf8 = str.utf8();
        const char *data = utf8.data();
        return data ? std::string(data) : std::string();
    }

    std::wstring ToWide(const std::string &input)
    {
#ifdef _WIN32
        if (input.empty())
            return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), (int)input.size(), nullptr, 0);
        if (size_needed <= 0)
            return std::wstring();
        std::wstring result(static_cast<size_t>(size_needed), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, input.c_str(), (int)input.size(), result.data(), size_needed);
        return result;
#else
        return std::wstring(input.begin(), input.end());
#endif
    }

    std::string::size_type FindLastPathSeparator(const std::string &s)
    {
        auto pos = s.find_last_of("/\\");
        return pos == std::string::npos ? 0 : pos + 1;
    }

    std::string StripQueryAndFragment(const std::string &s)
    {
        auto qpos = s.find_first_of("?#");
        if (qpos == std::string::npos)
            return s;
        return s.substr(0, qpos);
    }
} // namespace

DownloadManager::DownloadManager()
    : DownloadManager(DetermineDefaultDirectory()) {}

DownloadManager::DownloadManager(std::filesystem::path download_dir)
    : download_dir_(std::move(download_dir))
{
    EnsureDirectoryExists();
}

DownloadManager::~DownloadManager()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &entry : active_)
    {
        if (entry.second.stream && entry.second.stream->is_open())
            entry.second.stream->close();
    }
    active_.clear();
}

void DownloadManager::SetOnChangeCallback(std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    on_change_ = std::move(callback);
}

DownloadManager::DownloadId DownloadManager::NextDownloadId(ultralight::View *caller)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return next_id_++;
}

bool DownloadManager::OnRequestDownload(ultralight::View *caller, DownloadId id, const ultralight::String &url)
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto &record = GetOrCreateRecordLocked(id);
    record.url = ToStdString(url);
    record.status = Status::Requested;
    record.error.clear();
    record.finished_at = {};
    record.path.clear();
    record.expected_bytes = -1;
    record.received_bytes = 0;
    if (record.display_name.empty())
    {
        record.display_name = SanitizeFilename(DeriveFilename(record.url, ""));
    }
    record.started_at = std::chrono::system_clock::now();
    NotifyChangeLocked(lock);
    return true;
}

void DownloadManager::OnBeginDownload(ultralight::View *caller, DownloadId id, const ultralight::String &url,
                                      const ultralight::String &filename, int64_t expected_content_length)
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto &record = GetOrCreateRecordLocked(id);
    if (record.url.empty())
        record.url = ToStdString(url);

    record.status = Status::InProgress;
    record.expected_bytes = expected_content_length;
    record.received_bytes = 0;
    record.started_at = std::chrono::system_clock::now();
    record.error.clear();
    record.finished_at = {};

    EnsureDirectoryExists();

    std::string suggested = ToStdString(filename);
    std::string derived = DeriveFilename(record.url, suggested);
    std::string sanitized = SanitizeFilename(derived);
    auto full_path = EnsureUniquePath(sanitized);

    record.display_name = full_path.filename().u8string();
    record.path = full_path;
    record.error.clear();

    auto stream = std::make_unique<std::ofstream>(full_path, std::ios::binary | std::ios::out);
    if (!stream->is_open())
    {
        record.status = Status::Failed;
        record.error = "Failed to open file for writing";
        record.path.clear();
        NotifyChangeLocked(lock);
        return;
    }

    ActiveDownload active;
    active.record = &record;
    active.stream = std::move(stream);
    active_[id] = std::move(active);

    NotifyChangeLocked(lock);
}

void DownloadManager::OnReceiveDataForDownload(ultralight::View *caller, DownloadId id, ultralight::RefPtr<ultralight::Buffer> data)
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = active_.find(id);
    if (it == active_.end())
        return;

    auto &active = it->second;
    if (active.record && active.record->status == Status::Requested)
        active.record->status = Status::InProgress;
    if (active.record && active.record->display_name.empty())
        active.record->display_name = SanitizeFilename(DeriveFilename(active.record->url, ""));
    if (active.stream && active.stream->is_open() && data && data->size())
    {
        active.stream->write(reinterpret_cast<const char *>(data->data()), static_cast<std::streamsize>(data->size()));
        active.record->received_bytes += static_cast<int64_t>(data->size());
    }

    NotifyChangeLocked(lock);
}

void DownloadManager::OnFinishDownload(ultralight::View *caller, DownloadId id)
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto rec = FindRecordLocked(id);
    if (rec)
    {
        if (rec->status != Status::Failed && rec->status != Status::Cancelled)
            rec->status = Status::Completed;
        if (rec->expected_bytes >= 0 && rec->received_bytes < rec->expected_bytes)
            rec->received_bytes = rec->expected_bytes;
        if (rec->display_name.empty())
            rec->display_name = SanitizeFilename(DeriveFilename(rec->url, ""));
        rec->finished_at = std::chrono::system_clock::now();
    }

    CloseStreamLocked(id, false);
    NotifyChangeLocked(lock);
}

void DownloadManager::OnFailDownload(ultralight::View *caller, DownloadId id)
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto rec = FindRecordLocked(id);
    if (rec)
    {
        rec->status = Status::Failed;
        rec->error = "Download failed";
        if (rec->display_name.empty())
            rec->display_name = SanitizeFilename(DeriveFilename(rec->url, ""));
        rec->finished_at = std::chrono::system_clock::now();
        rec->path.clear();
    }

    CloseStreamLocked(id, true);
    NotifyChangeLocked(lock);
}

std::string DownloadManager::GetDownloadsJSON() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::string json = "{\"items\":[";
    bool first = true;
    for (auto it = records_.rbegin(); it != records_.rend(); ++it)
    {
        const auto &rec = it->second;
        if (!first)
            json += ',';
        first = false;

        json += "{\"id\":" + std::to_string(rec.id);
        json += ",\"url\":\"" + JsonEscape(rec.url) + "\"";
        json += ",\"filename\":\"" + JsonEscape(rec.display_name) + "\"";
        json += ",\"path\":\"" + JsonEscape(rec.path.u8string()) + "\"";
        json += ",\"status\":\"" + JsonEscape(StatusToString(rec.status)) + "\"";
        json += ",\"received\":" + std::to_string(rec.received_bytes);
        json += ",\"total\":" + std::to_string(rec.expected_bytes);
        json += ",\"canOpen\":" + std::string((rec.status == Status::Completed && !rec.path.empty()) ? "true" : "false");
        json += ",\"canReveal\":" + std::string((rec.status == Status::Completed && !rec.path.empty()) ? "true" : "false");
        json += ",\"startedAt\":" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(rec.started_at.time_since_epoch()).count());
        json += ",\"finishedAt\":" + std::to_string(rec.finished_at.time_since_epoch().count() ? std::chrono::duration_cast<std::chrono::milliseconds>(rec.finished_at.time_since_epoch()).count() : 0);
        json += ",\"error\":\"" + JsonEscape(rec.error) + "\"";
        json += "}";
    }
    json += "]}";
    return json;
}

void DownloadManager::ClearFinishedDownloads()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = records_.begin(); it != records_.end();)
    {
        if (it->second.status == Status::Completed || it->second.status == Status::Failed || it->second.status == Status::Cancelled)
        {
            if (active_.find(it->first) == active_.end())
            {
                it = records_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

bool DownloadManager::OpenDownload(DownloadId id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto rec = records_.find(id);
    if (rec == records_.end())
        return false;
    if (rec->second.status != Status::Completed || rec->second.path.empty())
        return false;

    const auto path_str = rec->second.path.u8string();
#ifdef _WIN32
    auto wpath = ToWide(path_str);
    if (wpath.empty())
        return false;
    auto result = ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
#elif defined(__APPLE__)
    std::string cmd = "open \"" + path_str + "\"";
    return std::system(cmd.c_str()) == 0;
#else
    std::string cmd = "xdg-open \"" + path_str + "\"";
    return std::system(cmd.c_str()) == 0;
#endif
}

bool DownloadManager::RevealDownload(DownloadId id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto rec = records_.find(id);
    if (rec == records_.end())
        return false;
    if (rec->second.path.empty())
        return false;

    auto folder = rec->second.path.parent_path();
    const auto folder_str = folder.u8string();
#ifdef _WIN32
    std::wstring params = L"/select,\"" + rec->second.path.wstring() + L"\"";
    auto result = ShellExecuteW(nullptr, L"open", L"explorer.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
#elif defined(__APPLE__)
    std::string cmd = "open -R \"" + rec->second.path.u8string() + "\"";
    return std::system(cmd.c_str()) == 0;
#else
    std::string cmd = "xdg-open \"" + folder_str + "\"";
    return std::system(cmd.c_str()) == 0;
#endif
}

bool DownloadManager::CancelDownload(DownloadId id)
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto active = active_.find(id);
    if (active == active_.end())
        return false;
    if (auto rec = FindRecordLocked(id))
        rec->status = Status::Cancelled;
    CloseStreamLocked(id, true);
    NotifyChangeLocked(lock);
    return true;
}

std::filesystem::path DownloadManager::DetermineDefaultDirectory()
{
    std::filesystem::path base;
#ifdef _WIN32
    base = std::filesystem::current_path();
#else
    const char *home = std::getenv("HOME");
    if (home && *home)
        base = std::filesystem::path(home);
    else
        base = std::filesystem::current_path();
#endif
    return base / kDownloadsFolderName;
}

std::string DownloadManager::DeriveFilename(const std::string &url, const std::string &suggested)
{
    if (!suggested.empty())
        return suggested;
    std::string trimmed = StripQueryAndFragment(url);
    auto start = FindLastPathSeparator(trimmed);
    std::string name = trimmed.substr(start);
    if (name.empty())
        return kDefaultFilename;
    return name;
}

std::string DownloadManager::SanitizeFilename(const std::string &filename)
{
    std::string result;
    result.reserve(filename.size());
    const std::string invalid = "\\/:*?\"<>|";
    for (char c : filename)
    {
        if (static_cast<unsigned char>(c) < 32)
            continue;
        if (invalid.find(c) != std::string::npos)
            continue;
        result.push_back(c);
    }
    if (result.empty())
        result = kDefaultFilename;
    return result;
}

std::filesystem::path DownloadManager::EnsureUniquePath(const std::string &base_name) const
{
    auto path = download_dir_ / base_name;
    if (!std::filesystem::exists(path))
        return path;

    auto stem = path.stem().u8string();
    auto extension = path.extension().u8string();
    for (int i = 1; i < 1000; ++i)
    {
        std::string candidate = stem + " (" + std::to_string(i) + ")" + extension;
        auto candidate_path = download_dir_ / candidate;
        if (!std::filesystem::exists(candidate_path))
            return candidate_path;
    }
    return path;
}

std::string DownloadManager::StatusToString(Status status) const
{
    switch (status)
    {
    case Status::Requested:
        return "requested";
    case Status::InProgress:
        return "in-progress";
    case Status::Completed:
        return "completed";
    case Status::Failed:
        return "failed";
    case Status::Cancelled:
        return "cancelled";
    }
    return "unknown";
}

std::string DownloadManager::JsonEscape(const std::string &input)
{
    std::string out;
    out.reserve(input.size() + 16);
    for (char c : input)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

void DownloadManager::EnsureDirectoryExists()
{
    std::error_code ec;
    if (!std::filesystem::exists(download_dir_, ec))
    {
        std::filesystem::create_directories(download_dir_, ec);
    }
}

void DownloadManager::NotifyChangeLocked(std::unique_lock<std::mutex> &lock)
{
    auto callback = on_change_;
    lock.unlock();
    if (callback)
        callback();
    lock.lock();
}

DownloadManager::DownloadRecord &DownloadManager::GetOrCreateRecordLocked(DownloadId id)
{
    auto it = records_.find(id);
    if (it == records_.end())
    {
        DownloadRecord rec;
        rec.id = id;
        rec.started_at = std::chrono::system_clock::now();
        it = records_.emplace(id, std::move(rec)).first;
    }
    return it->second;
}

DownloadManager::DownloadRecord *DownloadManager::FindRecordLocked(DownloadId id)
{
    auto it = records_.find(id);
    if (it == records_.end())
        return nullptr;
    return &it->second;
}

void DownloadManager::CloseStreamLocked(DownloadId id, bool remove_file)
{
    auto it = active_.find(id);
    if (it != active_.end())
    {
        if (it->second.stream && it->second.stream->is_open())
            it->second.stream->close();
        active_.erase(it);
    }
    auto rec = records_.find(id);
    if (remove_file && rec != records_.end() && !rec->second.path.empty())
    {
        std::error_code ec;
        std::filesystem::remove(rec->second.path, ec);
    }
}
