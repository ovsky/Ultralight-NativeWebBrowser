#include <AppCore/AppCore.h>
#include "UI.h"

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
};
