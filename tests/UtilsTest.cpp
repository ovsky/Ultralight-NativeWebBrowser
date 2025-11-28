#include "../src/Utils.h"
#include <iostream>
#include <cassert>

int main()
{
    using namespace util;

    // Test EscapeJsonString
    std::string s = "Line\nQuote\"\\Back";
    std::string e = EscapeJsonString(s);
    if (e.find("\\n") == std::string::npos || e.find("\\\"") == std::string::npos)
    {
        std::cerr << "EscapeJsonString failed: " << e << std::endl;
        return 2;
    }

    // Test ToIso8601UTC
    auto now = std::chrono::system_clock::now();
    std::string t = ToIso8601UTC(now);
    if (t.size() < 20)
    {
        std::cerr << "ToIso8601UTC output too short: " << t << std::endl;
        return 3;
    }

    // Test GetEnvVar platform safe path (set a known env var and check)
#if defined(_WIN32)
    _putenv_s("UITESTENV", "testval");
#else
    setenv("UITESTENV", "testval", 1);
#endif
    std::string v = GetEnvVar("UITESTENV");
    if (v != "testval")
    {
        std::cerr << "GetEnvVar failed: " << v << std::endl;
        return 4;
    }

    std::cout << "UtilsTest: OK\n";
    return 0;
}
