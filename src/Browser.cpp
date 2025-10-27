#include "Browser.h"
#include "resource.h"
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Config.h>
#include <Ultralight/Renderer.h>
#include <windows.h>

Browser::Browser()
{
  Settings settings;
  Config config;
  config.scroll_timer_delay = 1.0 / 90.0;
  app_ = App::Create(settings, config);

  window_ = Window::Create(app_->main_monitor(), 1024, 768, false,
                           kWindowFlags_Resizable | kWindowFlags_Titled | kWindowFlags_Maximizable);
  window_->SetTitle("Ultralight | Web Browser");

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
  } // Create the UI
  ui_.reset(new UI(window_));
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
