#include "UI.h"
#include <cstring>
#include <cmath>
#include <Ultralight/Renderer.h>

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

  if (evt.type == MouseEvent::kType_MouseDown)
  {
    // Right-click: open our custom context menu overlay above everything
    if (evt.button == MouseEvent::kButton_Right)
    {
      if (active_tab())
      {
        int client_x = evt.x;
        int client_y = evt.y - ui_height_;
        if (client_y < 0) client_y = 0;
        // Collect context info at the click point from the page
        char script_buf[512];
        // Build a small IIFE to inspect element under the cursor
        std::snprintf(script_buf, sizeof(script_buf),
          "(function(x,y){try{var t=document.elementFromPoint(x,y);var a=t&&t.closest?t.closest('a[href]'):null;var img=t&&t.closest?t.closest('img[src]'):null;var sel='';try{sel=String(window.getSelection?window.getSelection():'');}catch(_){}var info={linkURL:a&&a.href?a.href:'',imageURL:img&&img.src?img.src:'',selectionText:sel||'',isEditable:!!(t&&(t.isContentEditable||(t.tagName==='INPUT'||t.tagName==='TEXTAREA')))};return JSON.stringify(info);}catch(e){return '{}';}})(%d,%d)",
          client_x, client_y);
        ultralight::String json = active_tab()->view()->EvaluateScript(script_buf, nullptr);
        ShowContextMenuOverlay(evt.x, evt.y, json);
        return false; // consume
      }
    }
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

  if (!is_menu_view && !is_ctx_view)
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
  }

  global["OnBack"] = BindJSCallback(&UI::OnBack);
  global["OnForward"] = BindJSCallback(&UI::OnForward);
  global["OnRefresh"] = BindJSCallback(&UI::OnRefresh);
  global["OnStop"] = BindJSCallback(&UI::OnStop);
  global["OnToggleTools"] = BindJSCallback(&UI::OnToggleTools);
  global["OnMenuOpen"] = BindJSCallback(&UI::OnMenuOpen);
  global["OnMenuClose"] = BindJSCallback(&UI::OnMenuClose);
  if (is_ctx_view)
  {
    // context menu overlay actions
    global["OnContextMenuAction"] = BindJSCallback(&UI::OnContextMenuAction);
    setupContextMenu = global["setupContextMenu"];
    // If we have pending data, initialize menu now
    if (setupContextMenu && !pending_ctx_info_json_.empty())
    {
      setupContextMenu({(double)pending_ctx_position_.first, (double)pending_ctx_position_.second, pending_ctx_info_json_});
    }
  }
  global["OnRequestNewTab"] = BindJSCallback(&UI::OnRequestNewTab);
  global["OnRequestTabClose"] = BindJSCallback(&UI::OnRequestTabClose);
  global["OnActiveTabChange"] = BindJSCallback(&UI::OnActiveTabChange);
  global["OnRequestChangeURL"] = BindJSCallback(&UI::OnRequestChangeURL);
  global["OnAddressBarBlur"] = BindJSCallback(&UI::OnAddressBarBlur);

  if (!is_menu_view && !is_ctx_view)
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

void UI::CreateNewTab()
{
  uint64_t id = tab_id_counter_++;
  RefPtr<Window> window = window_;
  int tab_height = window->height() - ui_height_;
  if (tab_height < 1)
    tab_height = 1;
  tabs_[id].reset(new Tab(this, id, window->width(), (uint32_t)tab_height, 0, ui_height_));
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

void UI::OnMenuOpen(const JSObject &obj, const JSArgs &args)
{
  ShowMenuOverlay();
}

void UI::OnMenuClose(const JSObject &obj, const JSArgs &args)
{
  HideMenuOverlay();
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
  // Store desired initial state via URL hash, actual init via setupContextMenu after DOMReady
  view->LoadURL("file:///contextmenu.html");

  // After DOM ready we will call setupContextMenu; defer data by storing in a temporary string
  pending_ctx_position_ = {x, y};
  pending_ctx_info_json_ = json_info;
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
    if (active_tab())
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
      active_tab()->view()->EvaluateScript(script, nullptr);
    }
    return;
  }
  if (action == "paste-text" && args.size() >= 2)
  {
    // Hide overlay then insert text into focused element/contentEditable
    ultralight::String text = args[1];
    HideContextMenuOverlay();
    if (active_tab())
    {
      auto utf8 = text.utf8();
      std::string t = utf8.data() ? utf8.data() : "";
      // Minimal JS string escape for quotes and backslashes and newlines
      std::string esc; esc.reserve(t.size()+8);
      for (char c : t) {
        switch(c){
          case '\\': esc += "\\\\"; break;
          case '"': esc += "\\\""; break;
          case '\n': esc += "\\n"; break;
          case '\r': esc += "\\r"; break;
          case '\t': esc += "\\t"; break;
          default: esc += c; break;
        }
      }
      std::string script = "(function(t){try{var el=document.activeElement; if(!el) return false;";
      script += "if(el.isContentEditable){ document.execCommand && document.execCommand('insertText', false, t); return true;}";
      script += "var tag=el.tagName; if(tag==='INPUT'||tag==='TEXTAREA'){ var s=el.selectionStart||0,e=el.selectionEnd||0; el.setRangeText(t, s, e, 'end'); el.dispatchEvent(new Event('input',{bubbles:true})); return true;} return false;}catch(e){return false;}})(\"" + esc + "\")";
      active_tab()->view()->EvaluateScript(script.c_str(), nullptr);
    }
    return;
  }
  // Default: just close
  HideContextMenuOverlay();
}