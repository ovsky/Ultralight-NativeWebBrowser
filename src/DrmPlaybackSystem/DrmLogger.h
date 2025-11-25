#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <deque>

class DrmLogger
{
public:
    enum class Level
    {
        Info,
        Warning,
        Error,
        Debug
    };

    static DrmLogger &Instance();

    void Log(Level level, const std::string &component, const std::string &message);

    // convenience helpers
    void Info(const std::string &component, const std::string &message) { Log(Level::Info, component, message); }
    void Warn(const std::string &component, const std::string &message) { Log(Level::Warning, component, message); }
    void Error(const std::string &component, const std::string &message) { Log(Level::Error, component, message); }
    void Debug(const std::string &component, const std::string &message) { Log(Level::Debug, component, message); }

    // Return a concatenated recent in-memory log (thread-safe)
    std::string GetRecentLog(size_t max_chars = 8192);

private:
    DrmLogger();
    ~DrmLogger();

    std::string Timestamp();
    std::string LevelToString(Level l);

    std::mutex mutex_;
    std::ofstream ofs_;
    std::deque<std::string> recent_lines_;
    size_t recent_chars_ = 0;
};
