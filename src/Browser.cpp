#include "Browser.h"
#include "resource.h"
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Config.h>
#include <Ultralight/Renderer.h>
#include <memory>
#include <fstream>

#include "AdBlocker.h"

#if defined(_WIN32)
#include <windows.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

Browser::Browser()
{
  // Startup trace (append-only) to help isolate crashes before debugger attach
  {
    std::ofstream trace("startup_trace.log", std::ios::app);
    if (trace.good())
      trace << "Browser::Browser() - start\n";
  }
  Settings settings;
  Config config;
  config.scroll_timer_delay = 1.0 / 90.0;
  app_ = App::Create(settings, config);
  {
    std::ofstream trace("startup_trace.log", std::ios::app);
    if (trace.good())
      trace << "Browser::Browser() - app created\n";
  }

  window_ = Window::Create(app_->main_monitor(), 1024, 768, false,
                           kWindowFlags_Resizable | kWindowFlags_Titled | kWindowFlags_Maximizable);
  window_->SetTitle("Ultralight | Web Browser");

#if defined(_WIN32)
  // Set the native window background to a dark gray so the app shows a dark
  // background before any web content paints. This uses a per-class brush.
  HWND hwnd_bg = (HWND)window_->native_handle();
  if (hwnd_bg) {
    HBRUSH hBrush = CreateSolidBrush(RGB(22, 21, 29)); // #16151d dark gray
    // Replace the class background brush for this window's class.
    SetClassLongPtr(hwnd_bg, GCLP_HBRBACKGROUND, (LONG_PTR)hBrush);
    // Force a repaint so the background is visible immediately.
    InvalidateRect(hwnd_bg, NULL, TRUE);
    UpdateWindow(hwnd_bg);
  }
#endif

  {
    std::ofstream trace("startup_trace.log", std::ios::app);
    if (trace.good())
      trace << "Browser::Browser() - window created\n";
  }

#if defined(_WIN32)
  HWND hwnd = (HWND)window_->native_handle();
  if (hwnd)
  {
    BOOL use_dark_mode = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &use_dark_mode, sizeof(use_dark_mode)); // DWMWA_USE_IMMERSIVE_DARK_MODE = 20

    COLORREF dark_purple = RGB(42, 33, 60);
    DwmSetWindowAttribute(hwnd, 35, &dark_purple, sizeof(dark_purple)); // DWMWA_CAPTION_COLOR = 35
  }
#endif

#if defined(_WIN32)
  // Set window icons using Win32 API
  HINSTANCE hInstance = GetModuleHandle(NULL);

  // Load the icon in two different sizes
  HICON hIconBig = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MYICON),
                                    IMAGE_ICON, 32, 32, LR_SHARED);
  HICON hIconSmall = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_MYICON),
                                      IMAGE_ICON, 16, 16, LR_SHARED);

  if (hIconBig && hIconSmall)
  {
    // Set both icons
    SendMessage((HWND)window_->native_handle(), WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    SendMessage((HWND)window_->native_handle(), WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
  }

  {
    std::ofstream trace("startup_trace.log", std::ios::app);
    if (trace.good())
      trace << "Browser::Browser() - window icons set\n";
  }
#endif

  // Create the UI
  // NOTE: Temporarily disable AdBlocker initialization/loads and pass
  // null pointers into UI. This is a reversible mitigation to determine
  // whether AdBlocker startup code is triggering the crash.
  adblock_ = std::make_unique<AdBlocker>();
  {
    std::ofstream trace("startup_trace.log", std::ios::app);
    if (trace.good())
      trace << "Browser::Browser() - adblock constructed\n";
  }
  //adblock_->Clear();
  //adblock_->LoadBlocklist("assets/blocklist.txt", true);
  //adblock_->LoadBlocklistsInDirectory("assets/filters");

  // Pass nullptrs for adblock/tracker to avoid early adblock interactions.
  {
    std::ofstream trace("startup_trace.log", std::ios::app);
    if (trace.good())
      trace << "Browser::Browser() - creating UI (null adblock)\n";
  }
  ui_.reset(new UI(window_, nullptr, nullptr));
  {
    std::ofstream trace("startup_trace.log", std::ios::app);
    if (trace.good())
      trace << "Browser::Browser() - UI created\n";
  }
  window_->set_listener(ui_.get());
}

Browser::~Browser()
{
  window_->set_listener(nullptr);

  ui_.reset();

  window_ = nullptr;
  app_ = nullptr;
}

void Browser::Run()
{
  app_->Run();
}
