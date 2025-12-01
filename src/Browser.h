#include <AppCore/AppCore.h>
#include "UI.h"

#if defined(_WIN32)
#include <windows.h>
#endif

// Forward-declare AdBlocker
class AdBlocker;

using namespace ultralight;

class Browser
{
public:
  Browser();
  virtual ~Browser();

  virtual void Run();

protected:
  RefPtr<App> app_;
  RefPtr<Window> window_;
  std::unique_ptr<UI> ui_;
  std::unique_ptr<AdBlocker> adblock_;
#if defined(_WIN32)
  HBRUSH class_bg_brush_ = nullptr;
#endif
};
