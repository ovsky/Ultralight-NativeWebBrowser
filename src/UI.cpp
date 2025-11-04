#include "UI.h"
#include <cstring>
#include <cmath>
#include <Ultralight/Renderer.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <unordered_set>
#include <cstdio>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <direct.h> // _mkdir, _getcwd
#else
#include <sys/stat.h> // mkdir
#include <unistd.h>   // getcwd
#endif

static UI *g_ui = 0;

#define UI_HEIGHT 80

UI::UI(RefPtr<Window> window) : window_(window), cur_cursor_(Cursor::kCursor_Pointer),
                                is_resizing_inspector_(false), is_over_inspector_resize_drag_handle_(false)
{
  uint32_t window_width = window_->width();
  ui_height_ = (uint32_t)std::round(UI_HEIGHT * window_->scale());
  base_ui_height_ = ui_height_;
  overlay_ = Overlay::Create(window_, window_width, ui_height_, 0, 0);
  g_ui = this;

  view()->set_load_listener(this);
  view()->set_view_listener(this);
  view()->LoadURL("file:///ui.html");

  // Load keyboard shortcuts mapping
  LoadShortcuts();

  // Load popular sites for suggestions
  LoadPopularSites();
  // Load suggestions favicons flag
  LoadSuggestionsFaviconsFlag();
  // Load favicon disk cache
  LoadFaviconDiskCache();

  // Load history from disk
  LoadHistoryFromDisk();
}

// Compatibility overload: accepts optional ad/tracker blockers (ignored if not used)
UI::UI(RefPtr<Window> window, AdBlocker *adblock, AdBlocker *tracker)
    : window_(window), cur_cursor_(Cursor::kCursor_Pointer),
      is_resizing_inspector_(false), is_over_inspector_resize_drag_handle_(false),
      adblock_(adblock), trackerblock_(tracker)
{
  uint32_t window_width = window_->width();
  ui_height_ = (uint32_t)std::round(UI_HEIGHT * window_->scale());
  base_ui_height_ = ui_height_;
  overlay_ = Overlay::Create(window_, window_width, ui_height_, 0, 0);
  g_ui = this;

  view()->set_load_listener(this);
  view()->set_view_listener(this);
  view()->LoadURL("file:///ui.html");

  // Load keyboard shortcuts mapping
  LoadShortcuts();

  // Load popular sites for suggestions
  LoadPopularSites();
  // Load suggestions favicons flag
  LoadSuggestionsFaviconsFlag();
  // Load favicon disk cache
  LoadFaviconDiskCache();

  // Load history from disk
  LoadHistoryFromDisk();
}

UI::~UI()
{
  // Save history to disk before closing
  SaveHistoryToDisk();

  view()->set_load_listener(nullptr);
  view()->set_view_listener(nullptr);
  g_ui = nullptr;
}

void UI::OnAddressBarBlur(const JSObject &obj, const JSArgs &args)
{
  address_bar_is_focused_ = false;
}

void UI::OnAddressBarFocus(const JSObject &obj, const JSArgs &args)
{
  address_bar_is_focused_ = true;
}

bool UI::OnKeyEvent(const ultralight::KeyEvent &evt)
{
  // If menu overlay is active, route all key events to it and consume
  if (menu_overlay_ && menu_overlay_->view())
  {
    menu_overlay_->view()->FireKeyEvent(evt);
    return false;
  }
  // If context menu overlay is active, route keys (eg ESC) to it and consume
  if (context_menu_overlay_ && context_menu_overlay_->view())
  {
    context_menu_overlay_->view()->FireKeyEvent(evt);
    return false;
  }
  // If suggestions overlay is open, route navigation keys to it, others to UI overlay (address bar)
  if (suggestions_overlay_ && suggestions_overlay_->view())
  {
    // Virtual key codes: use evt.virtual_key_code values for arrows/enter/escape
    int vk = evt.virtual_key_code;
    bool isNavKey = (vk == 0x26 /*UP*/ || vk == 0x28 /*DOWN*/ || vk == 0x0D /*ENTER*/ || vk == 0x1B /*ESC*/ || vk == 0x09 /*TAB*/);
    if (isNavKey)
    {
      suggestions_overlay_->view()->FireKeyEvent(evt);
      return false;
    }
    // Otherwise send to UI overlay so address bar receives typing
    view()->FireKeyEvent(evt);
    return false;
  }

  if (evt.type == KeyEvent::kType_RawKeyDown && (evt.modifiers & KeyEvent::kMod_CtrlKey))
  {
    // Build key identifier like "Ctrl+T" for A-Z
    int vk = evt.virtual_key_code;
    char ch = static_cast<char>(vk);
    if (std::isalpha(static_cast<unsigned char>(ch)))
    {
      ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      std::string key = std::string("Ctrl+") + ch;
      auto it = shortcuts_.find(key);
      if (it != shortcuts_.end())
      {
        if (RunShortcutAction(it->second))
          return false;
      }
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
    return false; // Consume to avoid double-dispatch to focused view
  }

  return true;
}

// --- Shortcuts helpers ---
static void trim(std::string &s)
{
  size_t a = s.find_first_not_of(" \t\n\r");
  size_t b = s.find_last_not_of(" \t\n\r");
  if (a == std::string::npos)
  {
    s.clear();
    return;
  }
  s = s.substr(a, b - a + 1);
}

void UI::LoadShortcuts()
{
  // Defaults
  shortcuts_.clear();
  shortcuts_["Ctrl+T"] = "new-tab";
  shortcuts_["Ctrl+W"] = "close-tab";
  shortcuts_["Ctrl+H"] = "open-history";
  shortcuts_["Ctrl+L"] = "focus-address";

  // Try load from assets/shortcuts.json
  std::ifstream in("assets/shortcuts.json", std::ios::in | std::ios::binary);
  if (!in.is_open())
    return;
  std::ostringstream ss;
  ss << in.rdbuf();
  std::string txt = ss.str();
  in.close();

  // Very lenient JSON key-value parser for flat object { "Ctrl+T": "new-tab", ... }
  size_t i = 0, n = txt.size();
  auto next_quote = [&](size_t pos)
  {
    return txt.find('"', pos);
  };
  while (i < n)
  {
    size_t k1 = next_quote(i);
    if (k1 == std::string::npos)
      break;
    size_t k2 = next_quote(k1 + 1);
    if (k2 == std::string::npos)
      break;
    std::string key = txt.substr(k1 + 1, k2 - (k1 + 1));
    size_t colon = txt.find(':', k2 + 1);
    if (colon == std::string::npos)
      break;
    size_t v1 = next_quote(colon + 1);
    if (v1 == std::string::npos)
      break;
    size_t v2 = next_quote(v1 + 1);
    if (v2 == std::string::npos)
      break;
    std::string val = txt.substr(v1 + 1, v2 - (v1 + 1));
    trim(key);
    trim(val);
    if (!key.empty() && !val.empty())
      shortcuts_[key] = val;
    i = v2 + 1;
  }
}

bool UI::RunShortcutAction(const std::string &action)
{
  if (action == "new-tab")
  {
    CreateNewTab();
    return true;
  }
  if (action == "close-tab")
  {
    if (active_tab())
    {
      OnRequestTabClose({}, {active_tab_id_});
      return true;
    }
    return false;
  }
  if (action == "open-history")
  {
    // Open History in a NEW tab instead of replacing current
    RefPtr<View> child = CreateNewTabForChildView(String("file:///history.html"));
    if (child)
    {
      child->LoadURL("file:///history.html");
      return true;
    }
    return false;
  }
  if (action == "focus-address")
  {
    if (focusAddressBar)
    {
      RefPtr<JSContext> lock(view()->LockJSContext());
      focusAddressBar({});
      address_bar_is_focused_ = true;
      return true;
    }
    return false;
  }
  return false;
}

bool UI::OnMouseEvent(const ultralight::MouseEvent &evt)
{
  // If menu overlay is active, route mouse events to it and consume
  if (menu_overlay_ && menu_overlay_->view())
  {
    menu_overlay_->view()->FireMouseEvent(evt);
    return false;
  }
  // If context menu overlay is active, route mouse events to it and consume
  if (context_menu_overlay_ && context_menu_overlay_->view())
  {
    context_menu_overlay_->view()->FireMouseEvent(evt);
    return false;
  }
  // If suggestions overlay is active, route mouse to it and consume
  if (suggestions_overlay_ && suggestions_overlay_->view())
  {
    suggestions_overlay_->view()->FireMouseEvent(evt);
    return false;
  }

  // Handle right-clicks globally (both navbar/UI and page content)
  if (evt.type == MouseEvent::kType_MouseDown && evt.button == MouseEvent::kButton_Right)
  {
    bool on_ui = evt.y <= ui_height_;
    // Build a small script to collect context info at the click point
    char script_buf[512];
    std::snprintf(script_buf, sizeof(script_buf),
                  "(function(x,y){try{var t=document.elementFromPoint(x,y);var a=t&&t.closest?t.closest('a[href]'):null;var img=t&&t.closest?t.closest('img[src]'):null;var sel='';try{sel=String(window.getSelection?window.getSelection():'');}catch(_){ }var info={linkURL:a&&a.href?a.href:'',imageURL:img&&img.src?img.src:'',selectionText:sel||'',isEditable:!!(t&&(t.isContentEditable||(t.tagName==='INPUT'||t.tagName==='TEXTAREA')))};return JSON.stringify(info);}catch(e){return '{}';}})(%d,%d)",
                  on_ui ? evt.x : evt.x,
                  on_ui ? evt.y : (evt.y - ui_height_ < 0 ? 0 : evt.y - ui_height_));

    ultralight::String json;
    if (on_ui)
    {
      RefPtr<View> uiView = view();
      json = uiView->EvaluateScript(script_buf, nullptr);
      ctx_target_ = 1;
    }
    else if (active_tab())
    {
      json = active_tab()->view()->EvaluateScript(script_buf, nullptr);
      ctx_target_ = 2;
    }
    else
    {
      ctx_target_ = 0;
      json = "{}";
    }

    ShowContextMenuOverlay(evt.x, evt.y, json);
    return false; // consume right-click
  }

  // If mouse is interacting within the UI overlay region, forward to UI view and consume
  if (evt.y <= ui_height_)
  {
    // Ensure UI overlay has focus for typing in address bar
    if (evt.type == MouseEvent::kType_MouseDown)
    {
      address_bar_is_focused_ = true;
      view()->Focus();
    }
    view()->FireMouseEvent(evt);
    return false;
  }

  if (evt.type == MouseEvent::kType_MouseDown)
  {
    // Click occurred outside the UI overlay (handled above), switch focus to page
    address_bar_is_focused_ = false;
    if (active_tab())
    {
      active_tab()->view()->Focus();
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

  if (menu_overlay_)
    menu_overlay_->Resize(window->width(), window->height());
  if (context_menu_overlay_)
    context_menu_overlay_->Resize(window->width(), window->height());

  for (auto &tab : tabs_)
  {
    if (tab.second)
      tab.second->Resize(window->width(), (uint32_t)tab_height);
  }
}

void UI::OnDOMReady(View *caller, uint64_t frame_id, bool is_main_frame, const String &url)
{
  // Set the context for all subsequent JS* calls for THIS caller view
  RefPtr<JSContext> locked_context = caller->LockJSContext();
  SetJSContext(locked_context->ctx());

  JSObject global = JSGlobalObject();
  auto url_utf8 = url.utf8();
  bool is_menu_view = url_utf8.data() && std::strstr(url_utf8.data(), "menu.html") != nullptr;
  bool is_ctx_view = url_utf8.data() && std::strstr(url_utf8.data(), "contextmenu.html") != nullptr;
  bool is_sugg_view = url_utf8.data() && std::strstr(url_utf8.data(), "suggestions.html") != nullptr;

  if (!is_menu_view && !is_ctx_view && !is_sugg_view)
  {
    // Only main UI view has these functions
    updateBack = global["updateBack"];
    updateForward = global["updateForward"];
    updateLoading = global["updateLoading"];
    updateURL = global["updateURL"];
    addTab = global["addTab"];
    updateTab = global["updateTab"];
    closeTab = global["closeTab"];
    focusAddressBar = global["focusAddressBar"];
    isAddressBarFocused = global["isAddressBarFocused"];
    // (no adblock in this build)
    // Window controls callbacks
    global["OnWindowMinimize"] = BindJSCallback(&UI::OnWindowMinimize);
    global["OnWindowToggleMaximize"] = BindJSCallback(&UI::OnWindowToggleMaximize);
    global["OnWindowClose"] = BindJSCallback(&UI::OnWindowClose);
    global["OnBeginWindowDrag"] = BindJSCallback(&UI::OnBeginWindowDrag);
  }

  global["OnBack"] = BindJSCallback(&UI::OnBack);
  global["OnForward"] = BindJSCallback(&UI::OnForward);
  global["OnRefresh"] = BindJSCallback(&UI::OnRefresh);
  global["OnStop"] = BindJSCallback(&UI::OnStop);
  global["OnToggleTools"] = BindJSCallback(&UI::OnToggleTools);
  global["OnMenuOpen"] = BindJSCallback(&UI::OnMenuOpen);
  global["OnMenuClose"] = BindJSCallback(&UI::OnMenuClose);
  global["OnToggleDarkMode"] = BindJSCallback(&UI::OnToggleDarkMode);
  global["GetDarkModeEnabled"] = BindJSCallbackWithRetval(&UI::OnGetDarkModeEnabled);
  if (is_ctx_view)
  {
    // context menu overlay actions
    global["OnContextMenuAction"] = BindJSCallback(&UI::OnContextMenuAction);
    setupContextMenu = global["setupContextMenu"];
    // Initialize menu now (use '{}' if pending JSON is empty)
    if (setupContextMenu)
    {
      ultralight::String info = pending_ctx_info_json_.empty() ? ultralight::String("{}") : pending_ctx_info_json_;
      setupContextMenu({(double)pending_ctx_position_.first, (double)pending_ctx_position_.second, info});
    }
  }
  if (is_sugg_view)
  {
    // suggestions overlay actions
    global["OnSuggestionPick"] = BindJSCallback(&UI::OnSuggestionPick);
    global["OnSuggestionPaste"] = BindJSCallback(&UI::OnSuggestionPaste);
    global["OnFaviconReady"] = BindJSCallback(&UI::OnFaviconReady);
    setupSuggestions = global["setupSuggestions"];
    if (setupSuggestions)
    {
      ultralight::String items = pending_sugg_json_.empty() ? ultralight::String("[]") : pending_sugg_json_;
      setupSuggestions({(double)pending_sugg_position_.first, (double)pending_sugg_position_.second, (double)pending_sugg_width_, items});
    }
  }
  global["OnRequestNewTab"] = BindJSCallback(&UI::OnRequestNewTab);
  global["OnRequestTabClose"] = BindJSCallback(&UI::OnRequestTabClose);
  global["OnActiveTabChange"] = BindJSCallback(&UI::OnActiveTabChange);
  global["OnRequestChangeURL"] = BindJSCallback(&UI::OnRequestChangeURL);
  global["OnAddressBarNavigate"] = BindJSCallback(&UI::OnAddressBarNavigate);
  global["OnOpenHistoryNewTab"] = BindJSCallback(&UI::OnOpenHistoryNewTab);
  global["OnAddressBarBlur"] = BindJSCallback(&UI::OnAddressBarBlur);
  global["OnAddressBarFocus"] = BindJSCallback(&UI::OnAddressBarFocus);
  global["GetSuggestions"] = BindJSCallbackWithRetval(&UI::OnGetSuggestions);
  global["OpenSuggestionsOverlay"] = BindJSCallback(&UI::OnOpenSuggestionsOverlay);
  global["CloseSuggestionsOverlay"] = BindJSCallback(&UI::OnCloseSuggestionsOverlay);
  global["OnSuggestOpen"] = BindJSCallback(&UI::OnSuggestOpen);
  global["OnSuggestClose"] = BindJSCallback(&UI::OnSuggestClose);

  if (!is_menu_view && !is_ctx_view && !is_sugg_view)
  {
    CreateNewTab();
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

void UI::OnAddressBarNavigate(const JSObject &obj, const JSArgs &args)
{
  if (args.size() == 1)
  {
    ultralight::String url = args[0];
    // Record immediately so History UI updates quickly (dedup inside RecordHistory)
    RecordHistory(url, String(""));
    if (!tabs_.empty())
    {
      auto &tab = tabs_[active_tab_id_];
      tab->view()->LoadURL(url);
    }
  }
}

void UI::OnOpenHistoryNewTab(const JSObject &obj, const JSArgs &args)
{
  RefPtr<View> child = CreateNewTabForChildView(String("file:///history.html"));
  if (child)
    child->LoadURL("file:///history.html");
}

void UI::CreateNewTab()
{
  uint64_t id = tab_id_counter_++;
  RefPtr<Window> window = window_;
  int tab_height = window->height() - ui_height_;
  if (tab_height < 1)
    tab_height = 1;
  tabs_[id].reset(new Tab(this, id, window->width(), (uint32_t)tab_height, 0, ui_height_));
  // Load local static start page
  const char *kStartPage = "file:///static-sties/google-static.html";
  tabs_[id]->view()->LoadURL(kStartPage);

  RefPtr<JSContext> lock(view()->LockJSContext());
  addTab({id, "New Tab", GetFaviconURL(kStartPage), tabs_[id]->view()->is_loading()});
}

RefPtr<View> UI::CreateNewTabForChildView(const String &url)
{
  uint64_t id = tab_id_counter_++;
  RefPtr<Window> window = window_;
  int tab_height = window->height() - ui_height_;
  if (tab_height < 1)
    tab_height = 1;
  tabs_[id].reset(new Tab(this, id, window->width(), (uint32_t)tab_height, 0, ui_height_));

  RefPtr<JSContext> lock(view()->LockJSContext());
  addTab({id, "", GetFaviconURL(url), tabs_[id]->view()->is_loading()});

  return tabs_[id]->view();
}

void UI::UpdateTabTitle(uint64_t id, const ultralight::String &title)
{
  RefPtr<JSContext> lock(view()->LockJSContext());
  // Title changed; pass current page URL-derived favicon
  updateTab({id, title, GetFaviconURL(tabs_[id]->view()->url()), tabs_[id]->view()->is_loading()});

  // If active tab is a local file, reflect title in the address bar instead of file URL
  if (id == active_tab_id_)
  {
    auto url_u = tabs_[id]->view()->url().utf8();
    const char *cur = url_u.data();
    if (cur && strncmp(cur, "file://", 7) == 0)
    {
      updateURL({title});
    }
  }
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
  // For local static pages (file:///...), prefer showing the page title in the address bar
  ultralight::String display = url;
  auto u8 = url.utf8();
  const char *c_url = u8.data();
  bool is_file = (c_url && strncmp(c_url, "file://", 7) == 0);
  if (is_file && !tabs_.empty())
  {
    auto it = tabs_.find(active_tab_id_);
    if (it != tabs_.end() && it->second && it->second->view())
    {
      auto title = it->second->view()->title();
      auto t8 = title.utf8();
      if (t8.data() && *t8.data())
        display = title;
    }
  }

  RefPtr<JSContext> lock(view()->LockJSContext());
  updateURL({display});
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

// --- History helpers ---
void UI::RecordHistory(const String &url, const String &title)
{
  auto url_u = url.utf8();
  const char *c_url = url_u.data();
  if (!c_url)
    return;

  // Only record http(s)
  if (strncmp(c_url, "http://", 7) != 0 && strncmp(c_url, "https://", 8) != 0)
    return;

  // Basic cap to avoid unbounded growth later (we'll prune oldest when exceeding)
  auto title_u = title.utf8();
  std::string t = title_u.data() ? title_u.data() : "";
  std::string u = c_url;
  uint64_t now_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

  // Find existing entry by URL and update; otherwise push new
  bool found = false;
  for (auto &e : history_)
  {
    if (e.url == u)
    {
      if (!t.empty()) e.title = t;
      e.timestamp_ms = now_ms;
      e.visit_count = (e.visit_count + 1);
      found = true;
      break;
    }
  }
  if (!found)
  {
    history_.push_back({u, t, now_ms, 1});
  }

  // Cap to 500 by removing oldest (by timestamp)
  if (history_.size() > 500)
  {
    // Find oldest
    size_t idx = 0; uint64_t oldest = UINT64_MAX; size_t oldest_i = 0;
    for (idx = 0; idx < history_.size(); ++idx) {
      if (history_[idx].timestamp_ms < oldest) { oldest = history_[idx].timestamp_ms; oldest_i = idx; }
    }
    history_.erase(history_.begin() + oldest_i);
  }

  // Save history to disk after recording
  SaveHistoryToDisk();

  // If any tab is showing the History page, ask it to refresh now
  for (auto &it : tabs_)
  {
    auto &tabPtr = it.second;
    if (!tabPtr)
      continue;
    RefPtr<View> v = tabPtr->view();
    if (!v)
      continue;
    auto vurl_u = v->url().utf8();
    const char *vurl = vurl_u.data();
    if (vurl && std::strstr(vurl, "history.html"))
    {
      v->EvaluateScript("(function(){ if (window.refresh) window.refresh(); })();", nullptr);
    }
  }
}

static std::string jsonEscape(const std::string &s)
{
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s)
  {
    switch (c)
    {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

String UI::GetHistoryJSON()
{
  // Serialize as { items: [ {url,title,time}, ... ] }
  std::string json = std::string("{\"items\":[");
  // Newest first
  for (size_t i = 0; i < history_.size(); ++i)
  {
    const auto &e = history_[history_.size() - 1 - i];
    if (i)
      json += ",";
    json += "{\"url\":\"" + jsonEscape(e.url) + "\",\"title\":\"" + jsonEscape(e.title) + "\",\"time\":" + std::to_string(e.timestamp_ms) + "}";
  }
  json += "]}";
  return String(json.c_str());
}

void UI::ClearHistory()
{
  history_.clear();
}

void UI::OnMenuOpen(const JSObject &obj, const JSArgs &args)
{
  ShowMenuOverlay();
}

void UI::OnMenuClose(const JSObject &obj, const JSArgs &args)
{
  HideMenuOverlay();
}

void UI::OnToggleDarkMode(const JSObject &obj, const JSArgs &args)
{
  dark_mode_enabled_ = !dark_mode_enabled_;
  // Apply/remove to all existing tabs
  for (auto &p : tabs_)
  {
    if (!p.second)
      continue;
    auto v = p.second->view();
    if (!v)
      continue;
    if (dark_mode_enabled_)
      ApplyDarkModeToView(v);
    else
      RemoveDarkModeFromView(v);
  }
}

ultralight::JSValue UI::OnGetDarkModeEnabled(const JSObject &obj, const JSArgs &args)
{
  return ultralight::JSValue(dark_mode_enabled_);
}

void UI::AdjustUIHeight(uint32_t new_height)
{
  if (new_height == (uint32_t)ui_height_)
    return;

  ui_height_ = (int)new_height;

  // Resize top UI overlay
  overlay_->Resize(window_->width(), ui_height_);

  // Note: Do NOT move or resize tabs here; we only enlarge the UI overlay canvas.
}

void UI::OnSuggestOpen(const JSObject &obj, const JSArgs &args)
{
  // No-op: suggestions are shown in a top overlay; do not move layout.
}

void UI::OnSuggestClose(const JSObject &obj, const JSArgs &args)
{
  // No-op.
}

void UI::OnWindowMinimize(const JSObject &obj, const JSArgs &args)
{
#ifdef _WIN32
  HWND hwnd = (HWND)window_->native_handle();
  if (hwnd) ShowWindow(hwnd, SW_MINIMIZE);
#endif
}

void UI::OnWindowToggleMaximize(const JSObject &obj, const JSArgs &args)
{
#ifdef _WIN32
  HWND hwnd = (HWND)window_->native_handle();
  if (!hwnd) return;
  if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE); else ShowWindow(hwnd, SW_MAXIMIZE);
#endif
}

void UI::OnWindowClose(const JSObject &obj, const JSArgs &args)
{
  if (window_) window_->Close();
}

void UI::OnBeginWindowDrag(const JSObject &obj, const JSArgs &args)
{
#ifdef _WIN32
  HWND hwnd = (HWND)window_->native_handle();
  if (!hwnd) return;
  ReleaseCapture();
  SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
#endif
}

void UI::ShowMenuOverlay()
{
  if (menu_overlay_)
    return;

  // Create a transparent View so only the dropdown is visible over content
  ultralight::ViewConfig cfg;
  cfg.is_transparent = true;
  cfg.initial_device_scale = window_->scale();
  // Match acceleration/display with main UI view to avoid renderer mismatch
  if (overlay_ && overlay_->view())
  {
    cfg.is_accelerated = overlay_->view()->is_accelerated();
    cfg.display_id = overlay_->view()->display_id();
  }
  auto view = App::instance()->renderer()->CreateView(window_->width(), window_->height(), cfg, nullptr);

  // Wrap it in an overlay on top of everything
  menu_overlay_ = Overlay::Create(window_, view, 0, 0);
  menu_overlay_->Show();
  menu_overlay_->Focus();
  view->set_load_listener(this);
  view->set_view_listener(this);
  view->LoadURL("file:///menu.html");
}

void UI::HideMenuOverlay()
{
  if (!menu_overlay_)
    return;
  menu_overlay_->Hide();
  menu_overlay_->Unfocus();
  if (overlay_)
    overlay_->Focus();
  menu_overlay_->view()->set_load_listener(nullptr);
  menu_overlay_ = nullptr;
}

void UI::ShowContextMenuOverlay(int x, int y, const ultralight::String &json_info)
{
  // Recreate view each time for simplicity
  if (context_menu_overlay_)
  {
    HideContextMenuOverlay();
  }

  ultralight::ViewConfig cfg;
  cfg.is_transparent = true;
  cfg.initial_device_scale = window_->scale();
  if (overlay_ && overlay_->view())
  {
    cfg.is_accelerated = overlay_->view()->is_accelerated();
    cfg.display_id = overlay_->view()->display_id();
  }
  auto view = App::instance()->renderer()->CreateView(window_->width(), window_->height(), cfg, nullptr);
  context_menu_overlay_ = Overlay::Create(window_, view, 0, 0);
  context_menu_overlay_->Show();
  context_menu_overlay_->Focus();
  view->set_load_listener(this);
  view->set_view_listener(this);
  // Make data available before loading so OnDOMReady can initialize immediately
  pending_ctx_position_ = {x, y};
  pending_ctx_info_json_ = json_info;
  // Load overlay document; we'll invoke setupContextMenu in OnDOMReady
  view->LoadURL("file:///contextmenu.html");
}

void UI::HideContextMenuOverlay()
{
  if (!context_menu_overlay_)
    return;
  context_menu_overlay_->Hide();
  context_menu_overlay_->Unfocus();
  if (overlay_)
    overlay_->Focus();
  context_menu_overlay_->view()->set_load_listener(nullptr);
  context_menu_overlay_ = nullptr;
  pending_ctx_info_json_ = "";
}

void UI::OnContextMenuAction(const JSObject &obj, const JSArgs &args)
{
  // args: action, payload
  if (args.size() == 0)
  {
    HideContextMenuOverlay();
    return;
  }
  ultralight::String action = args[0];
  if (action == "close")
  {
    HideContextMenuOverlay();
    return;
  }
  if (action == "open_tab" && args.size() >= 2)
  {
    ultralight::String url = args[1];
    RefPtr<View> child = CreateNewTabForChildView(url);
    if (child)
      child->LoadURL(url);
    HideContextMenuOverlay();
    return;
  }
  if (action == "cut")
  {
    // Hide overlay then try to cut selection in page
    HideContextMenuOverlay();
    RefPtr<View> targetView = (ctx_target_ == 1) ? view() : (active_tab() ? active_tab()->view() : nullptr);
    if (targetView)
    {
      const char *script = R"JS((function(){
        try{
          if (document.execCommand && document.execCommand('cut')) return true;
          var selText = '';
          try { selText = String(window.getSelection ? window.getSelection() : ''); } catch(_){ }
          if (!selText) return false;
          try { if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(selText); } catch(_){ }
          // Delete selection
          if (document.execCommand) { document.execCommand('delete'); return true; }
          var el = document.activeElement;
          if (el && (el.tagName==='INPUT'||el.tagName==='TEXTAREA')){
            var s=el.selectionStart||0, e=el.selectionEnd||0, v=el.value||'';
            if (e>s){ el.setRangeText('', s, e, 'start'); el.dispatchEvent(new Event('input',{bubbles:true})); return true; }
          }
        }catch(e){}
        return false;
      })())JS";
      targetView->EvaluateScript(script, nullptr);
    }
    return;
  }
  if (action == "paste-text" && args.size() >= 2)
  {
    // Hide overlay then insert text into focused element/contentEditable
    ultralight::String text = args[1];
    HideContextMenuOverlay();
    RefPtr<View> targetView = (ctx_target_ == 1) ? view() : (active_tab() ? active_tab()->view() : nullptr);
    if (targetView)
    {
      auto utf8 = text.utf8();
      std::string t = utf8.data() ? utf8.data() : "";
      // Minimal JS string escape for quotes and backslashes and newlines
      std::string esc;
      esc.reserve(t.size() + 8);
      for (char c : t)
      {
        switch (c)
        {
        case '\\':
          esc += "\\\\";
          break;
        case '"':
          esc += "\\\"";
          break;
        case '\n':
          esc += "\\n";
          break;
        case '\r':
          esc += "\\r";
          break;
        case '\t':
          esc += "\\t";
          break;
        default:
          esc += c;
          break;
        }
      }
      std::string script = "(function(t){try{var el=document.activeElement; if(!el) return false;";
      script += "if(el.isContentEditable){ document.execCommand && document.execCommand('insertText', false, t); return true;}";
      script += "var tag=el.tagName; if(tag==='INPUT'||tag==='TEXTAREA'){ var s=el.selectionStart||0,e=el.selectionEnd||0; el.setRangeText(t, s, e, 'end'); el.dispatchEvent(new Event('input',{bubbles:true})); return true;} return false;}catch(e){return false;}})(\"" + esc + "\")";
      targetView->EvaluateScript(script.c_str(), nullptr);
    }
    return;
  }
  // Default: just close
  HideContextMenuOverlay();
}

void UI::ApplyDarkModeToView(RefPtr<View> v)
{
  if (!v)
    return;
  const char *js = R"JS((function(){
    try{
      var sid='__ul_auto_dark';
      // Remove any previous style to avoid duplicates/conflicts
      var prev=document.getElementById(sid); if(prev) prev.remove();
      var css = [
        'html,body{background:#0e0e0f !important;color:#e0e0e0 !important;color-scheme:dark !important}',
        // Make common large containers nearly black by default
        'body > header, body > main, body > footer, body > nav, body > section, body > article{background:#0e0e0f !important}',
        'div,section,main,article,aside,header,footer,nav{border-color:#333 !important}',
        // Form controls
        'input,textarea,select,button{background:#1a1b1c !important;color:#e6e6e6 !important;border-color:#333 !important}',
        'input::placeholder,textarea::placeholder{color:#9aa0a6 !important}',
        // Links
        'a{color:#8ab4f8 !important}',
        // Tables
        'table{background:#0f0f10 !important}',
        'thead,tbody,tr,th,td{background:transparent !important;border-color:#333 !important}',
        // Code blocks
        'pre,code,kbd,samp{background:#111215 !important;color:#e0e0e0 !important}',
        // Shadows
        '.card, .panel, .box{background:#111213 !important; border:1px solid #2a2b2d !important}'
      ].join('\n');
      var s=document.createElement('style'); s.id=sid; s.type='text/css'; s.appendChild(document.createTextNode(css));
      (document.head||document.documentElement).appendChild(s);

      // Heuristic: darken wide/large containers and backgrounds to near-black.
      function parseRGB(c){
        var m=(c||'').match(/rgba?\(([^)]+)\)/i); if(!m) return null; var p=m[1].split(',').map(function(x){return parseFloat(x)});
        return {r:~~p[0], g:~~p[1], b:~~p[2], a:(p.length>3? p[3]:1)};
      }
      function luminance(r,g,b){ // sRGB relative luminance
        function srgb(u){u/=255; return (u<=0.03928)? u/12.92: Math.pow((u+0.055)/1.055,2.4);}
        var R=srgb(r), G=srgb(g), B=srgb(b); return 0.2126*R+0.7152*G+0.0722*B;
      }
      function isLight(bg){ var c=parseRGB(bg); if(!c) return true; return luminance(c.r,c.g,c.b) > 0.35; }

      function darkenLarge(el){
        if(!el || !el.getBoundingClientRect) return;
        var r=el.getBoundingClientRect();
        var area=r.width*r.height;
        if (r.width>=600 || r.height>=300 || area>=120000) {
          var cs=getComputedStyle(el);
          var bg=cs.backgroundImage && cs.backgroundImage!=='none' ? null : cs.backgroundColor;
          if (!bg || bg==='transparent' || bg==='rgba(0, 0, 0, 0)' || isLight(bg)){
            try { el.style.setProperty('background-color', '#0e0e0f', 'important'); el.setAttribute('data-ul-dark','1'); } catch(e){}
          }
          // ensure text is readable
          try { el.style.setProperty('color', '#e0e0e0', 'important'); } catch(e){}
        }
      }

      function walk(root){
        var nodes=root.querySelectorAll('div,section,main,article,aside,header,footer,nav,body,html');
        for(var i=0;i<nodes.length;i++) darkenLarge(nodes[i]);
      }

      walk(document);
      var pending=null;
      var obs=new MutationObserver(function(){ if(pending) return; pending=requestAnimationFrame(function(){ pending=null; walk(document); }); });
      obs.observe(document.documentElement||document.body,{childList:true,subtree:true});
      window.__ul_dark_observer = obs;
      return true;
    }catch(e){return false;}
  })())JS";
  v->EvaluateScript(js, nullptr);
}

void UI::RemoveDarkModeFromView(RefPtr<View> v)
{
  if (!v)
    return;
  const char *js = R"JS((function(){
    try{
      var s=document.getElementById('__ul_auto_dark'); if(s) s.remove();
      var obs=window.__ul_dark_observer; if (obs && obs.disconnect) obs.disconnect();
      // Remove inline marks we added
      var marked=document.querySelectorAll('[data-ul-dark="1"]');
      for (var i=0;i<marked.length;i++){
        try{ marked[i].style.removeProperty('background-color'); marked[i].style.removeProperty('color'); marked[i].removeAttribute('data-ul-dark'); }catch(e){}
      }
      return true;
    }catch(e){return false;}
  })())JS";
  v->EvaluateScript(js, nullptr);
}

// --- URL Suggestions Implementation ---

void UI::LoadPopularSites()
{
  popular_sites_.clear();
  std::ifstream in("assets/popular_sites.json", std::ios::in | std::ios::binary);
  if (!in.is_open())
    return;

  std::ostringstream ss;
  ss << in.rdbuf();
  std::string content = ss.str();
  in.close();

  // Simple JSON parser for array of strings
  size_t start = content.find('[');
  size_t end = content.rfind(']');
  if (start == std::string::npos || end == std::string::npos)
    return;

  std::string sites_array = content.substr(start + 1, end - start - 1);
  size_t pos = 0;
  while (pos < sites_array.size())
  {
    size_t quote1 = sites_array.find('"', pos);
    if (quote1 == std::string::npos)
      break;
    size_t quote2 = sites_array.find('"', quote1 + 1);
    if (quote2 == std::string::npos)
      break;

    std::string site = sites_array.substr(quote1 + 1, quote2 - quote1 - 1);
    if (!site.empty())
      popular_sites_.push_back(site);

    pos = quote2 + 1;
  }
}

void UI::LoadHistoryFromDisk()
{
  std::ifstream in("data/history.json", std::ios::in | std::ios::binary);
  if (!in.is_open())
    return;

  std::ostringstream ss;
  ss << in.rdbuf();
  std::string content = ss.str();
  in.close();

  // Parse JSON array of history entries
  history_.clear();
  size_t pos = 0;
  while (pos < content.size())
  {
    size_t obj_start = content.find('{', pos);
    if (obj_start == std::string::npos)
      break;
    size_t obj_end = content.find('}', obj_start);
    if (obj_end == std::string::npos)
      break;

    std::string obj = content.substr(obj_start, obj_end - obj_start + 1);

    // Extract url
    size_t url_key = obj.find("\"url\"");
    std::string url;
    if (url_key != std::string::npos)
    {
      size_t url_start = obj.find('"', url_key + 5);
      if (url_start != std::string::npos)
      {
        size_t url_end = obj.find('"', url_start + 1);
        if (url_end != std::string::npos)
          url = obj.substr(url_start + 1, url_end - url_start - 1);
      }
    }

    // Extract title
    size_t title_key = obj.find("\"title\"");
    std::string title;
    if (title_key != std::string::npos)
    {
      size_t title_start = obj.find('"', title_key + 7);
      if (title_start != std::string::npos)
      {
        size_t title_end = obj.find('"', title_start + 1);
        if (title_end != std::string::npos)
          title = obj.substr(title_start + 1, title_end - title_start - 1);
      }
    }

    // Extract timestamp
    size_t time_key = obj.find("\"time\"");
    uint64_t timestamp = 0;
    if (time_key != std::string::npos)
    {
      size_t colon = obj.find(':', time_key);
      if (colon != std::string::npos)
      {
        size_t num_start = colon + 1;
        while (num_start < obj.size() && (obj[num_start] == ' ' || obj[num_start] == '\t'))
          num_start++;
        size_t num_end = num_start;
        while (num_end < obj.size() && std::isdigit(obj[num_end]))
          num_end++;
        if (num_end > num_start)
          timestamp = std::stoull(obj.substr(num_start, num_end - num_start));
      }
    }

    // Extract count (optional)
    size_t count_key = obj.find("\"count\"");
    uint32_t count = 1;
    if (count_key != std::string::npos)
    {
      size_t colon2 = obj.find(':', count_key);
      if (colon2 != std::string::npos)
      {
        size_t num_start = colon2 + 1;
        while (num_start < obj.size() && (obj[num_start] == ' ' || obj[num_start] == '\t')) num_start++;
        size_t num_end = num_start;
        while (num_end < obj.size() && std::isdigit(obj[num_end])) num_end++;
        if (num_end > num_start)
          count = (uint32_t)std::stoul(obj.substr(num_start, num_end - num_start));
      }
    }

    if (!url.empty())
      history_.push_back({url, title, timestamp, count});

    pos = obj_end + 1;
  }
}

void UI::SaveHistoryToDisk()
{
  std::ofstream out("data/history.json", std::ios::out | std::ios::binary | std::ios::trunc);
  if (!out.is_open())
    return;

  out << "[";
  for (size_t i = 0; i < history_.size(); ++i)
  {
    if (i > 0)
      out << ",";
    const auto &entry = history_[i];
  out << "{\"url\":\"" << jsonEscape(entry.url)
    << "\",\"title\":\"" << jsonEscape(entry.title)
    << "\",\"time\":" << entry.timestamp_ms
    << ",\"count\":" << entry.visit_count << "}";
  }
  out << "]";
  out.close();
}

std::vector<std::string> UI::GetSuggestions(const std::string &input, int maxResults)
{
  std::vector<std::string> suggestions;
  if (maxResults <= 0)
    maxResults = 10;

  // We'll build (url, score) pairs, then sort by score
  struct Scored { std::string url; double score; };
  std::vector<Scored> scored;
  auto push_scored = [&](const std::string &u, double s){ scored.push_back({u,s}); };

  auto now_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();

  auto score_history = [&](const std::string &u, const HistoryEntry &e, const std::string &q)->double{
    std::string ul = u, ql = q;
    std::transform(ul.begin(), ul.end(), ul.begin(), ::tolower);
    std::transform(ql.begin(), ql.end(), ql.begin(), ::tolower);
    double prefix = 0.0, contains = 0.0;
    // check domain part prefix
    size_t proto = ul.find("://"); size_t start = (proto==std::string::npos? 0: proto+3);
    if (ul.find(ql, start) == start) prefix = 1.0; // strong boost for prefix
    if (ul.find(ql) != std::string::npos) contains = 0.5;
    // recency: within ~30 days fades to 0
    double age_days = std::max<double>(0.0, (double)(now_ms - e.timestamp_ms) / (1000.0*60.0*60.0*24.0));
    double recency = std::max(0.0, 1.0 - (age_days / 30.0));
    // frequency: log scale
    double freq = std::log(1.0 + (double)std::max<uint32_t>(1, e.visit_count)) / std::log(10.0);
    return 2.0*prefix + 1.0*contains + 2.0*recency + 2.0*freq;
  };

  // If no input, suggest top by recency/frequency
  if (input.empty())
  {
    std::unordered_set<std::string> seen;
    // History candidates
    for (const auto &e : history_)
    {
      double s = score_history(e.url, e, std::string());
      if (seen.insert(e.url).second) push_scored(e.url, s);
    }
    // Sort by score desc
    std::sort(scored.begin(), scored.end(), [](const Scored&a,const Scored&b){return a.score>b.score;});
    for (const auto &p : scored)
    {
      if ((int)suggestions.size() >= maxResults) break;
      suggestions.push_back(p.url);
    }
    // Fill with popular sites
    for (const auto &site : popular_sites_)
    {
      if ((int)suggestions.size() >= maxResults) break;
      if (std::find(suggestions.begin(), suggestions.end(), site) == suggestions.end()) suggestions.push_back(site);
    }
    return suggestions;
  }

  std::string input_lower = input;
  std::transform(input_lower.begin(), input_lower.end(), input_lower.begin(), ::tolower);

  // Helper lambda to check if a URL matches the input
  auto matches = [&input_lower](const std::string &url) -> bool {
    std::string url_lower = url;
    std::transform(url_lower.begin(), url_lower.end(), url_lower.begin(), ::tolower);

    // Check if input matches the beginning of the URL (after protocol)
    size_t protocol_end = url_lower.find("://");
    if (protocol_end != std::string::npos)
    {
      std::string domain_part = url_lower.substr(protocol_end + 3);
      if (domain_part.find(input_lower) == 0)
        return true;
    }

    // Check if input appears anywhere in the URL
    return url_lower.find(input_lower) != std::string::npos;
  };

  // First, score matching history entries
  {
    std::unordered_set<std::string> seen;
    for (const auto &e : history_)
    {
      if (matches(e.url))
      {
        double s = score_history(e.url, e, input);
        if (seen.insert(e.url).second) push_scored(e.url, s);
      }
    }
  }

  // Then, add matching popular sites (lower score)
  for (const auto &site : popular_sites_)
  {
    if (matches(site))
    {
      // Low baseline score; small prefix boost
      double s = 0.6; std::string sl = site, il = input; std::transform(sl.begin(),sl.end(),sl.begin(),::tolower); std::transform(il.begin(),il.end(),il.begin(),::tolower);
      if (sl.find(il) == sl.find("://") + 3) s += 0.6;
      push_scored(site, s);
    }
  }

  // Sort by score and emit unique URLs
  std::sort(scored.begin(), scored.end(), [](const Scored&a,const Scored&b){return a.score>b.score;});
  std::unordered_set<std::string> emitted;
  for (const auto &p : scored)
  {
    if ((int)suggestions.size() >= maxResults) break;
    if (emitted.insert(p.url).second) suggestions.push_back(p.url);
  }
  return suggestions;
}

JSValue UI::OnGetSuggestions(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 1 || !args[0].IsString())
    return JSValue();

  ultralight::String input_ul = args[0];
  auto input_ul8 = input_ul.utf8();
  std::string input = input_ul8.data() ? input_ul8.data() : "";

  int maxResults = 10;
  if (args.size() >= 2 && args[1].IsNumber())
    maxResults = (int)args[1].ToInteger();

  auto suggestions = GetSuggestions(input, maxResults);

  // Build JSON array (strings or objects with favicon)
  std::string json = "[";
  for (size_t i = 0; i < suggestions.size(); ++i)
  {
    if (i > 0) json += ",";
    if (suggestion_favicons_enabled_)
    {
      std::string u = suggestions[i];
      // Prefer disk-cached favicon if available
      std::string origin = GetOriginStringFromURL(u);
      std::string f;
      auto itf = favicon_file_cache_.find(origin);
      if (itf != favicon_file_cache_.end())
      {
        f = itf->second; // file:///... path
      }
      if (f.empty())
      {
        auto fav = GetFaviconURL(String(u.c_str()));
        auto f8 = fav.utf8();
        f = f8.data() ? f8.data() : "";
      }
      json += "{\"url\":\"" + jsonEscape(u) + "\"";
      if (!f.empty()) json += ",\"favicon\":\"" + jsonEscape(f) + "\"";
      json += "}";
    }
    else
    {
      json += "\"" + jsonEscape(suggestions[i]) + "\"";
    }
  }
  json += "]";

  return JSValue(json.c_str());
}

// --- Favicon Disk Cache Helpers ---
std::string UI::GetOriginStringFromURL(const std::string &url)
{
  // Extract scheme://host[:port]
  size_t scheme = url.find("://");
  if (scheme == std::string::npos) return std::string();
  size_t host_start = scheme + 3;
  size_t slash = url.find('/', host_start);
  if (slash == std::string::npos) return url; // whole string is origin
  return url.substr(0, slash);
}

std::string UI::EnsureFaviconCacheDir()
{
  // Windows-style path acceptable; assets live under working dir.
  // Use data/favicons for persisted favicons
  std::string dir = "data/favicons";
  // Create directories if missing (best-effort)
  // We can't use std::filesystem here reliably; try to create via C runtime
#ifdef _WIN32
  _mkdir("data");
  _mkdir("data\\favicons");
#else
  mkdir("data", 0755);
  mkdir("data/favicons", 0755);
#endif
  return dir;
}

// simple base64 decoder (RFC 4648) for PNG payloads
std::string UI::Base64Decode(const std::string &in)
{
  static const int T[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    // rest -1
  };
  std::string out; out.reserve(in.size()*3/4);
  int val=0, valb=-8;
  for (unsigned char c : in) {
    int d = -1;
    if (c < 128) d = T[c];
    if (d == -1) { if (c=='=') break; else continue; }
    val = (val<<6) + d; valb += 6;
    if (valb>=0) { out.push_back(char((val>>valb)&0xFF)); valb-=8; }
  }
  return out;
}

void UI::LoadFaviconDiskCache()
{
  favicon_file_cache_.clear();
  // Ensure directory exists
  EnsureFaviconCacheDir();
  std::ifstream in("data/favicons/index.json", std::ios::in | std::ios::binary);
  if (!in.is_open()) return;
  std::ostringstream ss; ss << in.rdbuf(); std::string txt = ss.str(); in.close();
  // Parse simple object { "origin": "file:///...", ... }
  size_t pos = 0;
  while (true) {
    size_t k1 = txt.find('"', pos); if (k1==std::string::npos) break;
    size_t k2 = txt.find('"', k1+1); if (k2==std::string::npos) break;
    std::string key = txt.substr(k1+1, k2-k1-1);
    size_t c = txt.find(':', k2+1); if (c==std::string::npos) break;
    size_t v1 = txt.find('"', c+1); if (v1==std::string::npos) break;
    size_t v2 = txt.find('"', v1+1); if (v2==std::string::npos) break;
    std::string val = txt.substr(v1+1, v2-v1-1);
    if (!key.empty() && !val.empty()) favicon_file_cache_[key] = val;
    pos = v2+1;
    if (favicon_file_cache_.size() > favicon_cache_limit_) break;
  }
  if (favicon_file_cache_.size() > favicon_cache_limit_) {
    PruneFaviconDiskCacheToLimit();
    SaveFaviconDiskCache();
  }
}

void UI::SaveFaviconDiskCache()
{
  EnsureFaviconCacheDir();
  std::ofstream out("data/favicons/index.json", std::ios::out | std::ios::binary | std::ios::trunc);
  if (!out.is_open()) return;
  out << "{";
  bool first = true;
  for (auto &p : favicon_file_cache_) {
    if (!first) out << ","; first=false;
    out << "\"" << jsonEscape(p.first) << "\":\"" << jsonEscape(p.second) << "\"";
  }
  out << "}";
  out.close();
}

void UI::OnFaviconReady(const JSObject &obj, const JSArgs &args)
{
  // args: url, dataUrl (data:image/png;base64,....)
  if (args.size() < 2 || !args[0].IsString() || !args[1].IsString()) return;
  if (!suggestion_favicons_enabled_) return;
  ultralight::String url_ul = args[0].ToString();
  ultralight::String data_ul = args[1].ToString();
  auto u8 = url_ul.utf8(); std::string url = u8.data() ? u8.data() : "";
  auto d8 = data_ul.utf8(); std::string data = d8.data() ? d8.data() : "";
  if (url.empty() || data.empty()) return;
  std::string origin = GetOriginStringFromURL(url);
  if (origin.empty()) return;

  // Respect cache size limit with smarter eviction based on origin score
  double new_score = GetOriginScore(origin);
  if (favicon_file_cache_.find(origin) == favicon_file_cache_.end() && favicon_file_cache_.size() >= favicon_cache_limit_) {
    // Find the lowest-score existing origin
    std::string worst_origin; double worst_score = 1e18; bool found=false;
    for (const auto &p : favicon_file_cache_) {
      double s = GetOriginScore(p.first);
      if (!found || s < worst_score) { worst_score = s; worst_origin = p.first; found=true; }
    }
    if (!found || new_score <= worst_score) {
      // New origin is not better than the worst cached one; skip caching
      return;
    }
    // Evict worst
    auto itw = favicon_file_cache_.find(worst_origin);
    if (itw != favicon_file_cache_.end()) {
      // Try delete file on disk
      std::string furl = itw->second;
      std::string path;
      const std::string prefix = "file:///";
      if (furl.rfind(prefix, 0) == 0) {
        path = furl.substr(prefix.size());
#ifdef _WIN32
        for (auto &ch : path) { if (ch=='/') ch='\\'; }
#endif
        std::remove(path.c_str());
      }
      favicon_file_cache_.erase(itw);
    }
  }

  // Expect data URL like data:image/png;base64,....
  size_t comma = data.find(","); if (comma == std::string::npos) return;
  std::string b64 = data.substr(comma+1);
  std::string bytes = Base64Decode(b64);
  if (bytes.size() < 8) return;

  std::string dir = EnsureFaviconCacheDir();
  // Hash filename from origin
  std::hash<std::string> hasher; size_t h = hasher(origin);
  std::ostringstream path;
  path << dir << "/" << std::hex << h << ".png";
  std::string file = path.str();
  // Write file
  std::ofstream f(file, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!f.is_open()) return;
  f.write(bytes.data(), (std::streamsize)bytes.size());
  f.close();
  // Store as file:/// absolute URL
  char cwd_buf[1024] = {0};
#ifdef _WIN32
  _getcwd(cwd_buf, sizeof(cwd_buf));
  std::string abs = std::string("file:///") + std::string(cwd_buf) + "/" + file;
  // replace backslashes
  for (auto &ch : abs) { if (ch=='\\') ch='/'; }
#else
  getcwd(cwd_buf, sizeof(cwd_buf));
  std::string abs = std::string("file://") + std::string(cwd_buf) + "/" + file;
#endif
  favicon_file_cache_[origin] = abs;
  SaveFaviconDiskCache();
}

double UI::GetOriginScore(const std::string &origin)
{
  // Combine recency and frequency across history entries that match this origin
  if (origin.empty()) return 0.0;
  auto now_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
  double total = 0.0;
  for (const auto &e : history_) {
    if (GetOriginStringFromURL(e.url) != origin) continue;
    double age_days = std::max<double>(0.0, (double)(now_ms - e.timestamp_ms) / (1000.0*60.0*60.0*24.0));
    double recency = std::max(0.0, 1.0 - (age_days / 30.0));
    double freq = std::log(1.0 + (double)std::max<uint32_t>(1, e.visit_count)) / std::log(10.0);
    total += (2.0*recency + 2.0*freq);
  }
  return total;
}

void UI::PruneFaviconDiskCacheToLimit()
{
  if (favicon_file_cache_.size() <= favicon_cache_limit_) return;
  // Collect and sort by score desc
  std::vector<std::pair<std::string,double>> arr; arr.reserve(favicon_file_cache_.size());
  for (const auto &p : favicon_file_cache_) {
    arr.emplace_back(p.first, GetOriginScore(p.first));
  }
  std::sort(arr.begin(), arr.end(), [](const auto &a, const auto &b){ return a.second > b.second; });
  // Determine origins to keep
  std::unordered_set<std::string> keep;
  for (size_t i=0; i<arr.size() && i<favicon_cache_limit_; ++i) keep.insert(arr[i].first);
  // Remove the rest (and try delete files)
  for (auto it = favicon_file_cache_.begin(); it != favicon_file_cache_.end(); ) {
    if (keep.find(it->first) != keep.end()) { ++it; continue; }
    std::string furl = it->second;
    const std::string prefix = "file:///";
    if (furl.rfind(prefix, 0) == 0) {
      std::string path = furl.substr(prefix.size());
#ifdef _WIN32
      for (auto &ch : path) { if (ch=='/') ch='\\'; }
#endif
      std::remove(path.c_str());
    }
    it = favicon_file_cache_.erase(it);
  }
}

void UI::LoadSuggestionsFaviconsFlag()
{
  suggestion_favicons_enabled_ = true; // default
  std::ifstream in("assets/suggestions_favicons.txt", std::ios::in | std::ios::binary);
  if (!in.is_open()) return;
  std::ostringstream ss; ss << in.rdbuf(); std::string txt = ss.str(); in.close();
  // Normalize
  std::string s; s.reserve(txt.size());
  for (char c: txt){ if (!std::isspace((unsigned char)c)) s += (char)std::tolower((unsigned char)c); }
  if (s == "off" || s == "0" || s == "false") suggestion_favicons_enabled_ = false;
}

void UI::ShowSuggestionsOverlay(int x, int y, int width, const ultralight::String &json_items)
{
  // Recreate each time for simplicity
  if (suggestions_overlay_)
    HideSuggestionsOverlay();

  ultralight::ViewConfig cfg;
  cfg.is_transparent = true;
  cfg.initial_device_scale = window_->scale();
  if (overlay_ && overlay_->view())
  {
    cfg.is_accelerated = overlay_->view()->is_accelerated();
    cfg.display_id = overlay_->view()->display_id();
  }
  auto view = App::instance()->renderer()->CreateView(window_->width(), window_->height(), cfg, nullptr);
  suggestions_overlay_ = Overlay::Create(window_, view, 0, 0);
  suggestions_overlay_->Show();
  // Do not steal focus from address bar; leave unfocused
  view->set_load_listener(this);
  view->set_view_listener(this);
  pending_sugg_position_ = {x, y};
  pending_sugg_width_ = width;
  pending_sugg_json_ = json_items;
  view->LoadURL("file:///suggestions.html");
}

void UI::HideSuggestionsOverlay()
{
  if (!suggestions_overlay_)
    return;
  suggestions_overlay_->Hide();
  suggestions_overlay_->Unfocus();
  suggestions_overlay_->view()->set_load_listener(nullptr);
  suggestions_overlay_ = nullptr;
  pending_sugg_json_ = "";
}

void UI::OnOpenSuggestionsOverlay(const JSObject &obj, const JSArgs &args)
{
  // args: x, y, width, items_json
  if (args.size() < 4)
    return;
  int x = (int)args[0].ToInteger();
  int y = (int)args[1].ToInteger();
  int w = (int)args[2].ToInteger();
  ultralight::String items = args[3].ToString();
  ShowSuggestionsOverlay(x, y, w, items);
}

void UI::OnCloseSuggestionsOverlay(const JSObject &obj, const JSArgs &args)
{
  HideSuggestionsOverlay();
}

void UI::OnSuggestionPick(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 1 || !args[0].IsString())
  {
    HideSuggestionsOverlay();
    return;
  }
  ultralight::String s = args[0].ToString();
  HideSuggestionsOverlay();
  // Navigate like pressing Enter on the address bar
  std::string url = s.utf8().data() ? s.utf8().data() : "";
  if (url.empty()) return;
  // If second arg is "newtab", open in a new tab
  bool open_new_tab = false;
  if (args.size() >= 2 && args[1].IsString())
  {
    ultralight::String mode = args[1].ToString();
    auto m8 = mode.utf8();
    std::string m = m8.data() ? m8.data() : "";
    open_new_tab = (m == "newtab");
  }
  if (open_new_tab)
  {
    RefPtr<View> child = CreateNewTabForChildView(s);
    if (child) child->LoadURL(s);
    return;
  }
  if (!tabs_.empty())
  {
    auto &tab = tabs_[active_tab_id_];
    tab->view()->LoadURL(s);
  }
}

void UI::OnSuggestionPaste(const JSObject &obj, const JSArgs &args)
{
  // args: url string to paste into address bar; no navigation
  if (args.size() < 1 || !args[0].IsString())
    return;
  ultralight::String s = args[0].ToString();
  // Update address bar text in main UI without navigating
  if (updateURL)
  {
    RefPtr<JSContext> lock(view()->LockJSContext());
    updateURL({s});
  }
  // keep focus on address bar for continued typing
  address_bar_is_focused_ = true;
}