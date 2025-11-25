#include <fstream>

struct __StartupProbe
{
    __StartupProbe()
    {
        std::ofstream f("startup_probe.log", std::ios::app);
        if (f.is_open())
        {
            f << "Startup probe: static initializer ran\n";
            f.flush();
        }
    }
};

static __StartupProbe __startup_probe_instance;
