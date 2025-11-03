// Entry point for Windows; portable fallback for non-Windows.
#include "Browser.h"

#define ENABLE_PAUSE_FOR_DEBUGGER 0

#if defined(_WIN32)
  #include <Windows.h>
  #if ENABLE_PAUSE_FOR_DEBUGGER
    static void PauseForDebugger() { MessageBoxA(NULL, "Pause", "Caption", MB_OKCANCEL); }
  #else
    static void PauseForDebugger() {}
  #endif

  int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    PauseForDebugger();
    Browser browser;
    browser.Run();
    return 0;
  }
#else
  static void PauseForDebugger() {}
  int main(int argc, char** argv) {
    PauseForDebugger();
    Browser browser;
    browser.Run();
    return 0;
  }
#endif
