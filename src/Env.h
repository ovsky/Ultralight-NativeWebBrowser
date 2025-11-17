#pragma once

#include <string>
#include <cstdlib>

inline std::string GetEnvVar(const char *name)
{
#ifdef _MSC_VER
    char *buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, name) == 0 && buf)
    {
        std::string v(buf);
        free(buf);
        return v;
    }
    return std::string();
#else
    const char *v = std::getenv(name);
    return v ? std::string(v) : std::string();
#endif
}
