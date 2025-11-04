#include "Tab.h"
#include "UI.h"
#include <iostream>
#include <string>
#include <cstdio>

#define INSPECTOR_DRAG_HANDLE_HEIGHT 10

Tab::Tab(UI *ui, uint64_t id, uint32_t width, uint32_t height, int x, int y)
    : ui_(ui), id_(id), container_width_(width), container_height_(height)
{
  overlay_ = Overlay::Create(ui->window_, width, height, x, y);
  view()->set_view_listener(this);
  view()->set_load_listener(this);
}

Tab::~Tab()
{
  view()->set_view_listener(nullptr);
  view()->set_load_listener(nullptr);
}

void Tab::Show()
{
  overlay_->Show();
  overlay_->Focus();

  if (inspector_overlay_)
    inspector_overlay_->Show();
}

void Tab::Hide()
{
  overlay_->Hide();
  overlay_->Unfocus();

  if (inspector_overlay_)
    inspector_overlay_->Hide();
}

void Tab::ToggleInspector()
{
  if (!inspector_overlay_)
  {
    view()->CreateLocalInspectorView();
  }
  else
  {
    if (inspector_overlay_->is_hidden())
    {
      inspector_overlay_->Show();
    }
    else
    {
      inspector_overlay_->Hide();
    }
  }

  // Force resize to update layout
}

bool Tab::IsInspectorShowing() const
{
  if (!inspector_overlay_)
    return false;

  return !inspector_overlay_->is_hidden();
}

IntRect Tab::GetInspectorResizeDragHandle() const
{
  if (!IsInspectorShowing())
    return IntRect::MakeEmpty();

  int drag_handle_height_px = (uint32_t)std::round(INSPECTOR_DRAG_HANDLE_HEIGHT * ui_->window()->scale());

  // This drag handle should span the width of the UI and be centered vertically at the boundary between
  // the page overlay and inspector overlay.

  int drag_handle_x = (int)inspector_overlay_->x();
  int drag_handle_y = (int)inspector_overlay_->y() - drag_handle_height_px / 2;

  return {drag_handle_x, drag_handle_y, drag_handle_x + (int)inspector_overlay_->width(),
          drag_handle_y + drag_handle_height_px};
}

int Tab::GetInspectorHeight() const
{
  if (inspector_overlay_)
    return inspector_overlay_->height();

  return 0;
}

void Tab::SetInspectorHeight(int height)
{
  if (height > 2)
  {
    inspector_overlay_->Resize(inspector_overlay_->width(), height);

    // Trigger a resize to perform re-layout / re-size of content overlay
    Resize(container_width_, container_height_);
  }
}

void Tab::Resize(uint32_t width, uint32_t height)
{
  container_width_ = width;
  container_height_ = height;

  uint32_t content_height = container_height_;
  if (inspector_overlay_ && !inspector_overlay_->is_hidden())
  {
    content_height -= inspector_overlay_->height();
  }

  if (content_height < 1)
    content_height = 1;

  overlay_->Resize(container_width_, content_height);

  if (inspector_overlay_ && !inspector_overlay_->is_hidden())
  {
    inspector_overlay_->MoveTo(0, overlay_->y() + overlay_->height());
    inspector_overlay_->Resize(container_width_, inspector_overlay_->height());
  }
}

void Tab::MoveTo(uint32_t x, uint32_t y)
{
  overlay_->MoveTo(x, y);
  // Re-layout any inspector overlay relative to new content position
  Resize(container_width_, container_height_);
}

void Tab::OnChangeTitle(View *caller, const String &title)
{
  ui_->UpdateTabTitle(id_, title);
}

void Tab::OnChangeURL(View *caller, const String &url)
{
  ui_->UpdateTabURL(id_, url);
  // Record history when the page URL changes (navigation start/change), not on load finish
  if (ui_)
  {
    ui_->RecordHistory(url, caller->title());
  }
}

void Tab::OnChangeTooltip(View *caller, const String &tooltip) {}

void Tab::OnChangeCursor(View *caller, Cursor cursor)
{
  if (id_ == ui_->active_tab_id_)
    ui_->SetCursor(cursor);
}

void Tab::OnAddConsoleMessage(View *caller, const ConsoleMessage &msg)
{
}

RefPtr<View> Tab::OnCreateChildView(ultralight::View *caller,
                                    const String &opener_url, const String &target_url,
                                    bool is_popup, const IntRect &popup_rect)
{
  return ui_->CreateNewTabForChildView(target_url);
}

RefPtr<View> Tab::OnCreateInspectorView(ultralight::View *caller, bool is_local,
                                        const String &inspected_url)
{
  if (inspector_overlay_)
    return nullptr;

  inspector_overlay_ = Overlay::Create(ui_->window_, container_width_, container_height_ / 2, 0, 0);

  // Force resize to update layout
  Resize(container_width_, container_height_);
  inspector_overlay_->Show();

  return inspector_overlay_->view();
}

void Tab::OnBeginLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url)
{
  ui_->UpdateTabNavigation(id_, caller->is_loading(), caller->CanGoBack(), caller->CanGoForward());
}

void Tab::OnFinishLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url)
{
  ui_->UpdateTabNavigation(id_, caller->is_loading(), caller->CanGoBack(), caller->CanGoForward());
}

void Tab::OnFailLoading(View *caller, uint64_t frame_id, bool is_main_frame, const String &url,
                        const String &description, const String &error_domain, int error_code)
{
  if (is_main_frame)
  {
    char error_code_str[16];
    std::snprintf(error_code_str, sizeof(error_code_str), "%d", error_code);

    String html_string = "<html><head><style>";
    html_string += "* { font-family: sans-serif; }";
    html_string += "body { background-color: #CCC; color: #555; padding: 4em; }";
    html_string += "dt { font-weight: bold; padding: 1em; }";
    html_string += "</style></head><body>";
    html_string += "<h2>A Network Error was Encountered</h2>";
    html_string += "<dl>";
    html_string += "<dt>URL</dt><dd>" + url + "</dd>";
    html_string += "<dt>Description</dt><dd>" + description + "</dd>";
    html_string += "<dt>Error Domain</dt><dd>" + error_domain + "</dd>";
    html_string += "<dt>Error Code</dt><dd>" + String(error_code_str) + "</dd>";
    html_string += "</dl></body></html>";

    view()->LoadHTML(html_string);
  }
}

void Tab::OnUpdateHistory(View *caller)
{
  ui_->UpdateTabNavigation(id_, caller->is_loading(), caller->CanGoBack(), caller->CanGoForward());
}

void Tab::OnDOMReady(View *caller, uint64_t frame_id, bool is_main_frame, const String &url)
{
  // Install hooks for all frames (main and subframes)

  // Bind a native JS callback that the page can call when right-click occurs (main frame only)
  if (is_main_frame)
  {
    RefPtr<JSContext> ctx = caller->LockJSContext();
    SetJSContext(ctx->ctx());
    JSObject global = JSGlobalObject();
    global["NativeOpenContextMenu"] = BindJSCallback(&Tab::OnOpenContextMenu);

    // Expose a unified native bridge on window.__ul using global function proxies
    global["__ul_back"] = BindJSCallback(&Tab::JS_Back);
    global["__ul_forward"] = BindJSCallback(&Tab::JS_Forward);
    global["__ul_reload"] = BindJSCallback(&Tab::JS_Reload);
    global["__ul_stop"] = BindJSCallback(&Tab::JS_Stop);
    global["__ul_navigate"] = BindJSCallback(&Tab::JS_Navigate);
    global["__ul_newTab"] = BindJSCallback(&Tab::JS_NewTab);
    global["__ul_closeTab"] = BindJSCallback(&Tab::JS_CloseTab);
    global["__ul_openHistory"] = BindJSCallback(&Tab::JS_OpenHistory);
    global["__ul_getHistory"] = BindJSCallbackWithRetval(&Tab::JS_GetHistory);
    global["__ul_clearHistory"] = BindJSCallback(&Tab::JS_ClearHistory);
    global["__ul_toggleDarkMode"] = BindJSCallback(&Tab::JS_ToggleDarkMode);
    global["__ul_isDarkModeEnabled"] = BindJSCallbackWithRetval(&Tab::JS_IsDarkModeEnabled);
    global["__ul_getAppInfo"] = BindJSCallbackWithRetval(&Tab::JS_GetAppInfo);

    const char *attachScript = R"JS((function(){
      try{
        var n = (window.__ul = window.__ul || {});
        n.back = window.__ul_back;
        n.forward = window.__ul_forward;
        n.reload = window.__ul_reload;
        n.stop = window.__ul_stop;
        n.navigate = window.__ul_navigate;
        n.newTab = window.__ul_newTab;
        n.closeTab = window.__ul_closeTab;
        n.openHistory = window.__ul_openHistory;
        n.getHistory = window.__ul_getHistory;
        n.clearHistory = window.__ul_clearHistory;
        n.toggleDarkMode = window.__ul_toggleDarkMode;
        n.isDarkModeEnabled = window.__ul_isDarkModeEnabled;
        n.getAppInfo = window.__ul_getAppInfo;
      }catch(e){}
    })())JS";
    caller->EvaluateScript(attachScript, nullptr);
  }

  // Inject a contextmenu handler into the page to capture link/image/selection info
  const char *script = R"JS(
    (function(){
      try {
        if (window.__ul_ctxmenu_installed) return;
        window.__ul_ctxmenu_installed = true;
        document.addEventListener('contextmenu', function(e){
          try {
            e.preventDefault();
            var t = e.target;
            var a = t && t.closest ? t.closest('a[href]') : null;
            var img = t && t.closest ? t.closest('img[src]') : null;
            var sel = '';
            try { sel = String(window.getSelection ? window.getSelection() : ''); } catch(_) {}
            var info = {
              linkURL: a && a.href ? a.href : '',
              imageURL: img && img.src ? img.src : '',
              selectionText: sel || '',
              isEditable: !!(t && (t.isContentEditable || (t.tagName==='INPUT' || t.tagName==='TEXTAREA')))
            };
            if (window.NativeOpenContextMenu) {
              window.NativeOpenContextMenu(e.clientX, e.clientY, JSON.stringify(info));
            }
          } catch (err) {
          }
        }, true);
      } catch (err) {
      }
    })();
  )JS";
  if (is_main_frame)
    caller->EvaluateScript(script, nullptr);

  // If this is our History page, expose native methods
  {
    auto url_u = url.utf8();
    const char *c = url_u.data();
    if (c && std::strstr(c, "history.html"))
    {
      RefPtr<JSContext> ctx = caller->LockJSContext();
      SetJSContext(ctx->ctx());
      JSObject global = JSGlobalObject();
      global["NativeGetHistory"] = BindJSCallbackWithRetval(&Tab::OnHistoryGetData);
      global["NativeClearHistory"] = BindJSCallback(&Tab::OnHistoryClear);
      // Notify the page JS that native bridge is ready so it can refresh now
      caller->EvaluateScript("(function(){ if (window.__ul_history_ready) window.__ul_history_ready(); })();", nullptr);
    }
  }

  // Auto Dark Mode: if enabled, inject dark styling in this document
  if (ui_ && ui_->dark_mode_enabled_)
  {
    // Reuse UI helpers via the view
    ui_->ApplyDarkModeToView(caller);
  }
}

// --- General JS bridge implementations ---
void Tab::JS_Back(const JSObject &obj, const JSArgs &args)
{
  if (view())
    view()->GoBack();
}

void Tab::JS_Forward(const JSObject &obj, const JSArgs &args)
{
  if (view())
    view()->GoForward();
}

void Tab::JS_Reload(const JSObject &obj, const JSArgs &args)
{
  if (view())
    view()->Reload();
}

void Tab::JS_Stop(const JSObject &obj, const JSArgs &args)
{
  if (view())
    view()->Stop();
}

void Tab::JS_Navigate(const JSObject &obj, const JSArgs &args)
{
  if (args.size() >= 1 && view())
  {
    ultralight::String url = args[0];
    view()->LoadURL(url);
  }
}

void Tab::JS_NewTab(const JSObject &obj, const JSArgs &args)
{
  if (!ui_)
    return;
  if (args.size() >= 1)
  {
    ultralight::String url = args[0];
    RefPtr<View> child = ui_->CreateNewTabForChildView(url);
    if (child)
      child->LoadURL(url);
  }
  else
  {
    ui_->CreateNewTab();
  }
}

void Tab::JS_CloseTab(const JSObject &obj, const JSArgs &args)
{
  if (!ui_)
    return;
  // If an id is passed, close it, else close current tab
  uint64_t target = id_;
  if (args.size() >= 1)
  {
    // Clamp to uint64 from JS number
    target = static_cast<uint64_t>((double)args[0]);
  }
  ui_->OnRequestTabClose({}, {(double)target});
}

void Tab::JS_OpenHistory(const JSObject &obj, const JSArgs &args)
{
  if (!ui_)
    return;
  RefPtr<View> child = ui_->CreateNewTabForChildView(String("file:///history.html"));
  if (child)
    child->LoadURL("file:///history.html");
}

JSValue Tab::JS_GetHistory(const JSObject &obj, const JSArgs &args)
{
  if (!ui_)
    return JSValue();
  return JSValue(ui_->GetHistoryJSON());
}

void Tab::JS_ClearHistory(const JSObject &obj, const JSArgs &args)
{
  if (ui_)
    ui_->ClearHistory();
}

void Tab::JS_ToggleDarkMode(const JSObject &obj, const JSArgs &args)
{
  if (!ui_)
    return;
  ui_->OnToggleDarkMode({}, {});
}

JSValue Tab::JS_IsDarkModeEnabled(const JSObject &obj, const JSArgs &args)
{
  if (!ui_)
    return JSValue(false);
  return ui_->OnGetDarkModeEnabled({}, {});
}

JSValue Tab::JS_GetAppInfo(const JSObject &obj, const JSArgs &args)
{
  // Minimal app info
  std::string json = std::string("{\"name\":\"Ultralight-WebBrowser\",\"version\":\"1.0\"}");
  return JSValue(String(json.c_str()));
}

void Tab::OnOpenContextMenu(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 3)
    return;

  int view_x = (int)args[0];
  int view_y = (int)args[1];
  ultralight::String json = args[2];

  // Convert to window coords by offsetting with our overlay position
  // Convert overlay pixel offsets to CSS pixels (DIP) before adding client coords
  double scale = ui_->window()->scale();
  int win_x = (int)std::lround(((double)overlay_->x() / scale)) + view_x;
  int win_y = (int)std::lround(((double)overlay_->y() / scale)) + view_y;

  if (ui_)
  {
    ui_->ShowContextMenuOverlay(win_x, win_y, json);
  }
}

// --- History page JS bridge ---
JSValue Tab::OnHistoryGetData(const JSObject &obj, const JSArgs &args)
{
  if (!ui_)
    return JSValue();
  return JSValue(ui_->GetHistoryJSON());
}

void Tab::OnHistoryClear(const JSObject &obj, const JSArgs &args)
{
  if (ui_)
    ui_->ClearHistory();
}

// (Disable-history removed)
