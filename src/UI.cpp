#include "UI.h"
#include "AdBlocker.h"
#include <cstring>
#include <cmath>

static UI *g_ui = 0;

#define UI_HEIGHT 80

UI::UI(RefPtr<Window> window, ultralight::NetworkListener *net_listener, AdBlocker *adblocker) : window_(window), cur_cursor_(Cursor::kCursor_Pointer),
                                                                                                 is_resizing_inspector_(false), is_over_inspector_resize_drag_handle_(false),
                                                                                                 net_listener_(net_listener), adblocker_(adblocker)
{
  uint32_t window_width = window_->width();
  ui_height_ = (uint32_t)std::round(UI_HEIGHT * window_->scale());
  overlay_ = Overlay::Create(window_, window_width, ui_height_, 0, 0);
  g_ui = this;

  view()->set_load_listener(this);
  view()->set_view_listener(this);
  if (net_listener_)
    view()->set_network_listener(net_listener_);
  view()->LoadURL("file:///ui.html");
}

UI::~UI()
{
  view()->set_load_listener(nullptr);
  view()->set_view_listener(nullptr);
  g_ui = nullptr;
}

void UI::OnAddressBarBlur(const JSObject &obj, const JSArgs &args)
{
  address_bar_is_focused_ = false;
}

bool UI::OnKeyEvent(const ultralight::KeyEvent &evt)
{
  if (evt.type == KeyEvent::kType_RawKeyDown && (evt.modifiers & KeyEvent::kMod_CtrlKey))
  {
    switch (evt.virtual_key_code)
    {
    case 'T':
      CreateNewTab();
      return false;
    case 'W':
      if (active_tab())
      {
        OnRequestTabClose({}, {active_tab_id_});
      }
      return false;
    case 'L':
      if (focusAddressBar)
      {
        RefPtr<JSContext> lock(view()->LockJSContext());
        focusAddressBar({});
        address_bar_is_focused_ = true;
      }
      return false;
    }
  }

  if (address_bar_is_focused_)
  {
    view()->FireKeyEvent(evt);
    return false; // Consume the event
  }

  if (active_tab())
  {
    active_tab()->view()->FireKeyEvent(evt);
  }

  return true;
}

bool UI::OnMouseEvent(const ultralight::MouseEvent &evt)
{
  if (evt.type == MouseEvent::kType_MouseDown)
  {
    // Check if the click is outside the UI overlay
    if (evt.y > ui_height_)
    {
      address_bar_is_focused_ = false;
      if (active_tab())
      {
        active_tab()->view()->Focus();
      }
    }
  }
  if (active_tab() && active_tab()->IsInspectorShowing())
  {
    int x_px = static_cast<int>(std::lround(evt.x * window()->scale()));
    int y_px = static_cast<int>(std::lround(evt.y * window()->scale()));

    if (is_resizing_inspector_)
    {
      int resize_delta = inspector_resize_begin_mouse_y_ - y_px;
      int new_inspector_height = inspector_resize_begin_height_ + resize_delta;
      active_tab()->SetInspectorHeight(new_inspector_height);

      if (evt.type == MouseEvent::kType_MouseUp)
      {
        is_resizing_inspector_ = false;
      }

      return false;
    }

    IntRect drag_handle = active_tab()->GetInspectorResizeDragHandle();

    bool over_drag_handle = drag_handle.Contains(Point(static_cast<float>(x_px), static_cast<float>(y_px)));

    if (over_drag_handle && !is_over_inspector_resize_drag_handle_)
    {
      // We entered the drag area
      window()->SetCursor(Cursor::kCursor_NorthSouthResize);
      is_over_inspector_resize_drag_handle_ = true;
    }
    else if (!over_drag_handle && is_over_inspector_resize_drag_handle_)
    {
      // We left the drag area, restore previous cursor
      window()->SetCursor(cur_cursor_);
      is_over_inspector_resize_drag_handle_ = false;
    }

    if (over_drag_handle && evt.type == MouseEvent::kType_MouseDown && !is_resizing_inspector_)
    {
      is_resizing_inspector_ = true;
      inspector_resize_begin_mouse_y_ = y_px;
      inspector_resize_begin_height_ = active_tab()->GetInspectorHeight();
    }

    return !over_drag_handle;
  }

  return true;
}

void UI::OnClose(ultralight::Window *window)
{
  App::instance()->Quit();
}

void UI::OnResize(ultralight::Window *window, uint32_t width, uint32_t height)
{
  int tab_height = window->height() - ui_height_;

  if (tab_height < 1)
    tab_height = 1;

  overlay_->Resize(window->width(), ui_height_);

  for (auto &tab : tabs_)
  {
    if (tab.second)
      tab.second->Resize(window->width(), (uint32_t)tab_height);
  }
}

void UI::OnDOMReady(View *caller, uint64_t frame_id, bool is_main_frame, const String &url)
{
  // Set the context for all subsequent JS* calls
  RefPtr<JSContext> locked_context = view()->LockJSContext();
  SetJSContext(locked_context->ctx());

  JSObject global = JSGlobalObject();
  updateBack = global["updateBack"];
  updateForward = global["updateForward"];
  updateLoading = global["updateLoading"];
  updateURL = global["updateURL"];
  addTab = global["addTab"];
  updateTab = global["updateTab"];
  closeTab = global["closeTab"];
  focusAddressBar = global["focusAddressBar"];
  isAddressBarFocused = global["isAddressBarFocused"];
  updateAdblockEnabled = global["updateAdblockEnabled"];

  global["OnBack"] = BindJSCallback(&UI::OnBack);
  global["OnForward"] = BindJSCallback(&UI::OnForward);
  global["OnRefresh"] = BindJSCallback(&UI::OnRefresh);
  global["OnStop"] = BindJSCallback(&UI::OnStop);
  global["OnToggleTools"] = BindJSCallback(&UI::OnToggleTools);
  global["OnToggleAdblock"] = BindJSCallback(&UI::OnToggleAdblock);
  global["OnRequestNewTab"] = BindJSCallback(&UI::OnRequestNewTab);
  global["OnRequestTabClose"] = BindJSCallback(&UI::OnRequestTabClose);
  global["OnActiveTabChange"] = BindJSCallback(&UI::OnActiveTabChange);
  global["OnRequestChangeURL"] = BindJSCallback(&UI::OnRequestChangeURL);
  global["OnAddressBarBlur"] = BindJSCallback(&UI::OnAddressBarBlur);

  CreateNewTab();

  // Initialize adblock state indicator, if available
  if (adblocker_ && updateAdblockEnabled)
  {
    updateAdblockEnabled({adblocker_->enabled()});
  }
}

void UI::OnBack(const JSObject &obj, const JSArgs &args)
{
  if (active_tab())
    active_tab()->view()->GoBack();
}

void UI::OnForward(const JSObject &obj, const JSArgs &args)
{
  if (active_tab())
    active_tab()->view()->GoForward();
}

void UI::OnRefresh(const JSObject &obj, const JSArgs &args)
{
  if (active_tab())
    active_tab()->view()->Reload();
}

void UI::OnStop(const JSObject &obj, const JSArgs &args)
{
  if (active_tab())
    active_tab()->view()->Stop();
}

void UI::OnToggleTools(const JSObject &obj, const JSArgs &args)
{
  if (active_tab())
    active_tab()->ToggleInspector();
}

void UI::OnToggleAdblock(const JSObject &obj, const JSArgs &args)
{
  if (!adblocker_)
    return;
  bool now = !adblocker_->enabled();
  adblocker_->set_enabled(now);
  if (updateAdblockEnabled)
  {
    RefPtr<JSContext> lock(view()->LockJSContext());
    updateAdblockEnabled({now});
  }
}

void UI::OnRequestNewTab(const JSObject &obj, const JSArgs &args)
{
  CreateNewTab();
}

void UI::OnRequestTabClose(const JSObject &obj, const JSArgs &args)
{
  if (args.size() == 1)
  {
    uint64_t id = args[0];

    auto &tab = tabs_[id];
    if (!tab)
      return;

    if (tabs_.size() == 1 && App::instance())
      App::instance()->Quit();

    if (id != active_tab_id_)
    {
      tabs_[id].reset();
      tabs_.erase(id);
    }
    else
    {
      tab->set_ready_to_close(true);
    }

    RefPtr<JSContext> lock(view()->LockJSContext());
    closeTab({id});
  }
}

void UI::OnActiveTabChange(const JSObject &obj, const JSArgs &args)
{
  if (args.size() == 1)
  {
    uint64_t id = args[0];

    if (id == active_tab_id_)
      return;

    auto &tab = tabs_[id];
    if (!tab)
      return;

    tabs_[active_tab_id_]->Hide();

    if (tabs_[active_tab_id_]->ready_to_close())
    {
      tabs_[active_tab_id_].reset();
      tabs_.erase(active_tab_id_);
    }

    active_tab_id_ = id;
    tabs_[active_tab_id_]->Show();

    auto tab_view = tabs_[active_tab_id_]->view();
    SetLoading(tab_view->is_loading());
    SetCanGoBack(tab_view->CanGoBack());
    SetCanGoForward(tab_view->CanGoBack());
    SetURL(tab_view->url());
  }
}

void UI::OnRequestChangeURL(const JSObject &obj, const JSArgs &args)
{
  if (args.size() == 1)
  {
    ultralight::String url = args[0];

    if (!tabs_.empty())
    {
      auto &tab = tabs_[active_tab_id_];
      tab->view()->LoadURL(url);
    }
  }
}

void UI::CreateNewTab()
{
  uint64_t id = tab_id_counter_++;
  RefPtr<Window> window = window_;
  int tab_height = window->height() - ui_height_;
  if (tab_height < 1)
    tab_height = 1;
  tabs_[id].reset(new Tab(this, id, window->width(), (uint32_t)tab_height, 0, ui_height_));
  if (net_listener_)
    tabs_[id]->view()->set_network_listener(net_listener_);
  tabs_[id]->view()->LoadURL("https://www.google.com");

  RefPtr<JSContext> lock(view()->LockJSContext());
  addTab({id, "New Tab", GetFaviconURL("https://www.google.com"), tabs_[id]->view()->is_loading()});
}

RefPtr<View> UI::CreateNewTabForChildView(const String &url)
{
  uint64_t id = tab_id_counter_++;
  RefPtr<Window> window = window_;
  int tab_height = window->height() - ui_height_;
  if (tab_height < 1)
    tab_height = 1;
  tabs_[id].reset(new Tab(this, id, window->width(), (uint32_t)tab_height, 0, ui_height_));
  if (net_listener_)
    tabs_[id]->view()->set_network_listener(net_listener_);

  RefPtr<JSContext> lock(view()->LockJSContext());
  addTab({id, "", GetFaviconURL(url), tabs_[id]->view()->is_loading()});

  return tabs_[id]->view();
}

void UI::UpdateTabTitle(uint64_t id, const ultralight::String &title)
{
  RefPtr<JSContext> lock(view()->LockJSContext());
  // Title changed; pass current page URL-derived favicon
  updateTab({id, title, GetFaviconURL(tabs_[id]->view()->url()), tabs_[id]->view()->is_loading()});
}

void UI::UpdateTabURL(uint64_t id, const ultralight::String &url)
{
  if (id == active_tab_id_ && !tabs_.empty())
    SetURL(url);
}

void UI::UpdateTabNavigation(uint64_t id, bool is_loading, bool can_go_back, bool can_go_forward)
{
  if (tabs_.empty())
    return;

  RefPtr<JSContext> lock(view()->LockJSContext());
  // Loading/nav state; update favicon based on current URL
  updateTab({id, tabs_[id]->view()->title(), GetFaviconURL(tabs_[id]->view()->url()), tabs_[id]->view()->is_loading()});

  if (id == active_tab_id_)
  {
    SetLoading(is_loading);
    SetCanGoBack(can_go_back);
    SetCanGoForward(can_go_forward);
  }
}

void UI::SetLoading(bool is_loading)
{
  RefPtr<JSContext> lock(view()->LockJSContext());
  updateLoading({is_loading});
}

void UI::SetCanGoBack(bool can_go_back)
{
  RefPtr<JSContext> lock(view()->LockJSContext());
  updateBack({can_go_back});
}

void UI::SetCanGoForward(bool can_go_forward)
{
  RefPtr<JSContext> lock(view()->LockJSContext());
  updateForward({can_go_forward});
}

void UI::SetURL(const ultralight::String &url)
{
  RefPtr<JSContext> lock(view()->LockJSContext());
  updateURL({url});
}

void UI::SetCursor(ultralight::Cursor cursor)
{
  if (App::instance())
    window_->SetCursor(cursor);
}

String UI::GetFaviconURL(const String &page_url)
{
  // Best-effort: use origin + "/favicon.ico" for http/https URLs.
  // Cache by origin so multiple tabs/pages from the same site reuse it.
  auto utf8 = page_url.utf8();
  const char *url = utf8.data();
  if (!url)
    return String("");

  if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
    return String("");

  const char *scheme_sep = strstr(url, "://");
  if (!scheme_sep)
    return String("");
  const char *host_start = scheme_sep + 3;
  const char *slash_after_host = strchr(host_start, '/');

  // Compute origin as a std::string for cache key
  std::string origin_str;
  if (!slash_after_host)
  {
    origin_str.assign(url);
  }
  else
  {
    origin_str.assign(url, (size_t)(slash_after_host - url));
  }

  auto it = favicon_cache_.find(origin_str);
  if (it != favicon_cache_.end())
  {
    return String(it->second.c_str());
  }

  std::string favicon = origin_str + "/favicon.ico";
  favicon_cache_[origin_str] = favicon;
  return String(favicon.c_str());
}