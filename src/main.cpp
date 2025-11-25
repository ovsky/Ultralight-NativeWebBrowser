// Entry point for Windows; portable fallback for non-Windows.
#include "Browser.h"

#define ENABLE_PAUSE_FOR_DEBUGGER 0

#if defined(_WIN32) && ENABLE_PAUSE_FOR_DEBUGGER
#include <Windows.h>
static void PauseForDebugger() { MessageBoxA(NULL, "Pause", "Caption", MB_OKCANCEL); }
#else
static void PauseForDebugger() {}
#endif

#include <fstream>
#include <exception>

#if defined(_WIN32)
#include <Windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  PauseForDebugger();
  try
  {
    // Early diagnostic logging to help identify startup crashes
    std::ofstream diag("startup_diag.log", std::ios::app);
    diag << "Starting WinMain..." << std::endl;
    diag.flush();
    Browser browser;
    diag << "Constructed Browser" << std::endl;
    diag.flush();
    browser.Run();
    diag << "Run returned" << std::endl;
    diag.close();
  }
  catch (const std::system_error &se)
  {
    std::ofstream diag("startup_diag.log", std::ios::app);
    diag << "system_error in WinMain: code=" << se.code().value() << " category=" << se.code().category().name() << " msg=" << se.what() << std::endl;
    diag.close();
  }
  catch (const std::exception &ex)
  {
    std::ofstream diag("startup_diag.log", std::ios::app);
    diag << "Exception in WinMain: " << ex.what() << std::endl;
    diag.close();
  }
  catch (...)
  {
    std::ofstream diag("startup_diag.log", std::ios::app);
    diag << "Unknown exception in WinMain" << std::endl;
    diag.close();
  }
  return 0;
}
#else
int main(int argc, char **argv)
{
  PauseForDebugger();
  Browser browser;
  browser.Run();
  return 0;
}
#endif
