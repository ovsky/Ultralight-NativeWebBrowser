#include "DrmPlaybackSystem/DrmManager.h"
#include <iostream>

int main()
{
    try
    {
        std::cout << "Constructing DrmManager...\n";
        DrmManager mgr;
        std::cout << "DrmManager constructed. Checking config...\n";
        bool enabled = mgr.Config().IsGlobalDrmEnabled();
        std::cout << "Global DRM enabled: " << (enabled ? "true" : "false") << "\n";
        std::cout << "Starting dependency download (test)...\n";
        mgr.Window().StartDependencyDownload();
        std::cout << "Started download thread. Exiting diag.\n";
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 2;
    }
    catch (...)
    {
        std::cerr << "Unknown exception" << std::endl;
        return 3;
    }
    return 0;
}
