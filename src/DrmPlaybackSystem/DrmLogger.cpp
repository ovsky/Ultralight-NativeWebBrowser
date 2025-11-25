#include "DrmLogger.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

DrmLogger &DrmLogger::Instance()
{
    static DrmLogger instance;
    return instance;
}

DrmLogger::DrmLogger()
{
    ofs_.open("drm_system.log", std::ios::app);
}

DrmLogger::~DrmLogger()
{
    if (ofs_.is_open())
        ofs_.close();
}

std::string DrmLogger::LevelToString(Level l)
{
    switch (l)
    {
    case Level::Info:
        return "INFO";
    case Level::Warning:
        return "WARN";
    case Level::Error:
        return "ERROR";
    case Level::Debug:
        return "DEBUG";
    }
    return "UNKNOWN";
}

std::string DrmLogger::Timestamp()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void DrmLogger::Log(Level level, const std::string &component, const std::string &message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream out;
    out << "[" << Timestamp() << "] [" << LevelToString(level) << "] [" << component << "]: " << message;
    std::string s = out.str();
    std::cout << s << std::endl;
    if (ofs_.is_open())
    {
        ofs_ << s << std::endl;
        ofs_.flush();
    }
    // Maintain an in-memory recent-lines buffer for UI queries.
    // Keep total chars roughly bounded to avoid unbounded memory growth.
    recent_lines_.push_back(s);
    recent_chars_ += s.size() + 1;
    const size_t kMaxChars = 16384; // keep up to ~16KB in memory
    while (recent_chars_ > kMaxChars && !recent_lines_.empty())
    {
        recent_chars_ -= (recent_lines_.front().size() + 1);
        recent_lines_.pop_front();
    }
}

std::string DrmLogger::GetRecentLog(size_t max_chars)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream ss;
    size_t chars = 0;
    for (const auto &line : recent_lines_)
    {
        if (chars + line.size() + 1 > max_chars)
            break;
        ss << line << "\n";
        chars += line.size() + 1;
    }
    return ss.str();
}
