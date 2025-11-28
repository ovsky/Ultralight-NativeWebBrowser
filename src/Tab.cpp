#include "Tab.h"
#include "UI.h"
#include "DownloadManager.h"
#include <iostream>
#include <string>
#include <cstdio>
#include <sstream>

#define INSPECTOR_DRAG_HANDLE_HEIGHT 10

Tab::Tab(UI *ui, uint64_t id, uint32_t width, uint32_t height, int x, int y)
    : ui_(ui), id_(id), container_width_(width), container_height_(height)
{
  overlay_ = Overlay::Create(ui->window_, width, height, x, y);
  view()->set_view_listener(this);
  view()->set_load_listener(this);
  view()->set_download_listener(ui->download_manager());
}

Tab::~Tab()
{
  view()->set_view_listener(nullptr);
  view()->set_load_listener(nullptr);
  view()->set_download_listener(nullptr);
}

void Tab::Show()
{
  overlay_->Show();
  overlay_->Focus();

  if (inspector_overlay_)
  {
    inspector_overlay_->Show();
    inspector_overlay_->Focus();
  }
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
    // Lightweight Quick Inspector: create our own overlay and load local UI
    inspector_overlay_ = Overlay::Create(ui_->window_, container_width_, container_height_ / 3, 0, 0);
    // Re-layout so the content overlay makes room and inspector is at the bottom
    Resize(container_width_, container_height_);
    inspector_overlay_->Show();
    auto iv = inspector_overlay_->view();
    if (iv)
    {
      iv->set_load_listener(this);
      iv->set_view_listener(this);
      iv->set_download_listener(ui_->download_manager());
      iv->LoadURL("file:///quick-inspector.html");
    }
  }
  else
  {
    if (inspector_overlay_->is_hidden())
    {
      inspector_overlay_->Show();
      inspector_overlay_->Focus();
      // Ensure layout updates so inspector sits below content
      Resize(container_width_, container_height_);
    }
    else
    {
      inspector_overlay_->Hide();
      // Restore full content height when inspector is hidden
      Resize(container_width_, container_height_);
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
  // Forward console messages to Quick Inspector if visible
  if (inspector_overlay_ && !inspector_overlay_->is_hidden())
  {
    auto iv = inspector_overlay_->view();
    if (iv)
    {
      String smsg = msg.message();
      auto u = smsg.utf8();
      std::string m = u.data() ? u.data() : "";
      // Minimal escaping for safe JS string literal
      auto escape = [](const std::string &in)
      { std::string out; out.reserve(in.size()+16); for(char c: in){ switch(c){ case '\\': out += "\\\\"; break; case '"': out += "\\\""; break; case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break; default: out += c; }} return out; };
      std::string js = std::string("(function(m){ if(window.__qi && __qi.onConsole){ __qi.onConsole({message:m}); } })(\"") + escape(m) + "\")";
      iv->EvaluateScript(String(js.c_str()), nullptr);
    }
  }
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
  // For compatibility: if devtools requests an inspector view, host it in our overlay.
  if (!inspector_overlay_)
  {
    inspector_overlay_ = Overlay::Create(ui_->window_, container_width_, container_height_ / 2, 0, 0);
    // Force resize to update layout
    Resize(container_width_, container_height_);
    inspector_overlay_->Show();
  }
  auto iv = inspector_overlay_->view();
  if (iv)
    iv->set_download_listener(ui_->download_manager());
  return iv;
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

    // Check if this is the settings page
    auto url_utf8 = url.utf8();
    bool is_settings_page = url_utf8.data() && std::strstr(url_utf8.data(), "settings.html") != nullptr;

    if (is_settings_page)
    {
      // Bind settings bridge functions when settings page loads in a tab
      global["GetSettingsSnapshot"] = BindJSCallbackWithRetval(&Tab::JS_GetSettingsSnapshot);
      global["OnUpdateSetting"] = BindJSCallback(&Tab::JS_UpdateSetting);
      global["OnSaveSettings"] = BindJSCallback(&Tab::JS_SaveSettings);
      global["OnRestoreSettingsDefaults"] = BindJSCallbackWithRetval(&Tab::JS_RestoreSettingsDefaults);
      std::cout << "[Tab::OnDOMReady] Settings page detected, bridge functions bound" << std::endl;
    }

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

  // Lightweight network monitor (fetch / XHR)
  if (is_main_frame)
  {
    RefPtr<JSContext> ctx = caller->LockJSContext();
    SetJSContext(ctx->ctx());
    JSObject global = JSGlobalObject();
    global["NativeNetworkEvent"] = BindJSCallback(&Tab::OnNetworkEvent);
    const char *netPatch = R"JS((function(){
      if (window.__ul_net_patched) return; window.__ul_net_patched = true;
      function now(){ return Date.now(); }
      function report(o){ try{ if(window.NativeNetworkEvent) window.NativeNetworkEvent(JSON.stringify(o)); }catch(e){} }
      var ofetch = window.fetch;
      if (ofetch) {
        window.fetch = function(input, init){
          var start = now();
          var url = (typeof input==='string') ? input : ((input && input.url) || '');
          return ofetch.apply(this, arguments).then(function(resp){
            try{ report({type:'fetch', url:url, status: resp.status, ok: !!resp.ok, time: now()-start}); }catch(e){}
            return resp; });
        };
      }
      var xo = XMLHttpRequest.prototype.open, xs = XMLHttpRequest.prototype.send;
      XMLHttpRequest.prototype.open = function(method, url){ this.__ul = {m:method,u:url,s:0}; return xo.apply(this, arguments); };
      XMLHttpRequest.prototype.send = function(){
        var self=this; self.__ul.s = now();
        self.addEventListener('loadend', function(){ try{ report({type:'xhr', url:(self.__ul && self.__ul.u)||'', status:self.status, time: now()-self.__ul.s}); }catch(e){} });
        return xs.apply(this, arguments);
      };
    })())JS";
    caller->EvaluateScript(netPatch, nullptr);
  }

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
    else if (c && std::strstr(c, "downloads.html"))
    {
      RefPtr<JSContext> ctx = caller->LockJSContext();
      SetJSContext(ctx->ctx());
      JSObject global = JSGlobalObject();
      global["NativeGetDownloads"] = BindJSCallbackWithRetval(&Tab::OnDownloadsGetData);
      global["NativeClearDownloads"] = BindJSCallback(&Tab::OnDownloadsClear);
      global["NativeOpenDownload"] = BindJSCallback(&Tab::OnDownloadsOpen);
      global["NativeRevealDownload"] = BindJSCallback(&Tab::OnDownloadsReveal);
      caller->EvaluateScript("(function(){ if (window.__ul_downloads_ready) window.__ul_downloads_ready(); })();", nullptr);
      if (ui_)
        ui_->NotifyDownloadsChanged();
    }
    else if (c && std::strstr(c, "quick-inspector.html"))
    {
      RefPtr<JSContext> ctx = caller->LockJSContext();
      SetJSContext(ctx->ctx());
      JSObject global = JSGlobalObject();
      // Bind Quick Inspector callbacks
      global["NativeQuickClose"] = BindJSCallback(&Tab::OnQuickInspectorClose);
      global["NativeQuickOpenDevtools"] = BindJSCallback(&Tab::OnQuickInspectorOpenDevtools);
      global["NativeQuickGetInfo"] = BindJSCallbackWithRetval(&Tab::OnQuickInspectorGetInfo);
      // Advanced APIs
      global["NativeQuickEval"] = BindJSCallbackWithRetval(&Tab::QI_Eval);
      global["NativeQuickGetDOMTree"] = BindJSCallbackWithRetval(&Tab::QI_GetDOMTree);
      global["NativeQuickGetNodeRect"] = BindJSCallbackWithRetval(&Tab::QI_GetNodeRect);
      global["NativeQuickStartPicker"] = BindJSCallback(&Tab::QI_StartPicker);
      global["NativeQuickStopPicker"] = BindJSCallback(&Tab::QI_StopPicker);
      global["NativeQuickHighlightSelector"] = BindJSCallback(&Tab::QI_HighlightSelector);
      global["NativeQuickGetComputedStyle"] = BindJSCallbackWithRetval(&Tab::QI_GetComputedStyle);
      global["NativeQuickGetStorage"] = BindJSCallbackWithRetval(&Tab::QI_GetStorage);
      global["NativeQuickGetPerformance"] = BindJSCallbackWithRetval(&Tab::QI_GetPerformance);
      global["NativeQuickGetOuterHTML"] = BindJSCallbackWithRetval(&Tab::QI_GetOuterHTML);
      global["NativeQuickSetAttribute"] = BindJSCallback(&Tab::QI_SetAttribute);
      global["NativeQuickRemoveAttribute"] = BindJSCallback(&Tab::QI_RemoveAttribute);
      // Initial refresh happens in page JS
    }
  }

  // Auto Dark Mode: if enabled, inject dark styling in this document
  if (ui_ && ui_->dark_mode_enabled_)
  {
    // Reuse UI helpers via the view
    ui_->ApplyDarkModeToView(caller);
  }
}

void Tab::OnQuickInspectorClose(const JSObject &obj, const JSArgs &args)
{
  if (inspector_overlay_)
  {
    // Stop the picker if it's active
    QI_StopPicker(obj, args);

    inspector_overlay_->Hide();
    // Restore content area to full height and return focus to page
    Resize(container_width_, container_height_);
    overlay_->Focus();
  }
}

void Tab::OnQuickInspectorOpenDevtools(const JSObject &obj, const JSArgs &args)
{
  // Hide quick inspector and open full devtools into the same overlay
  if (inspector_overlay_)
  {
    inspector_overlay_->Show();
  }
  view()->CreateLocalInspectorView();
}

JSValue Tab::OnQuickInspectorGetInfo(const JSObject &obj, const JSArgs &args)
{
  std::string t, u;
  if (view())
  {
    auto tu = view()->title().utf8();
    auto uu = view()->url().utf8();
    if (tu.data())
      t = tu.data();
    if (uu.data())
      u = uu.data();
  }
  bool loading = view() ? view()->is_loading() : false;
  std::string json = std::string("{") +
                     "\"title\":\"" + t + "\"," +
                     "\"url\":\"" + u + "\"," +
                     "\"loading\":" + (loading ? "true" : "false") +
                     "}";
  return JSValue(String(json.c_str()));
}

// ---- Events forwarded to Quick Inspector ----
void Tab::OnNetworkEvent(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 1)
    return;
  if (!(inspector_overlay_ && inspector_overlay_->view()))
    return;
  ultralight::String payload = args[0];
  auto u = payload.utf8();
  std::string s = u.data() ? u.data() : "{}";
  std::string js = std::string("(function(p){ if(window.__qi && __qi.onNetwork) __qi.onNetwork(p); })(") + s + ")";
  inspector_overlay_->view()->EvaluateScript(String(js.c_str()), nullptr);
}

void Tab::OnPickerHover(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 1)
    return;
  if (!(inspector_overlay_ && inspector_overlay_->view()))
    return;
  ultralight::String payload = args[0];
  auto u = payload.utf8();
  std::string s = u.data() ? u.data() : "{}";
  std::string js = std::string("(function(p){ if(window.__qi && __qi.onPickHover) __qi.onPickHover(p); })(") + s + ")";
  inspector_overlay_->view()->EvaluateScript(String(js.c_str()), nullptr);
}

void Tab::OnPickerSelect(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 1)
    return;
  if (!(inspector_overlay_ && inspector_overlay_->view()))
    return;
  ultralight::String payload = args[0];
  auto u = payload.utf8();
  std::string s = u.data() ? u.data() : "{}";
  std::string js = std::string("(function(p){ if(window.__qi && __qi.onPickSelect) __qi.onPickSelect(p); })(") + s + ")";
  inspector_overlay_->view()->EvaluateScript(String(js.c_str()), nullptr);
}

// ---- Quick Inspector advanced APIs ----
JSValue Tab::QI_Eval(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 1 || !view())
    return JSValue(String(""));
  String expr = args[0];
  String result;
  view()->EvaluateScript(expr, &result);
  return JSValue(result);
}

JSValue Tab::QI_GetDOMTree(const JSObject &obj, const JSArgs &args)
{
  int depth = 3;
  int maxChildren = 20;
  if (args.size() >= 1)
    depth = (int)(double)args[0];
  if (args.size() >= 2)
    maxChildren = (int)(double)args[1];
  if (!view())
    return JSValue(String("{}"));
  std::ostringstream ss;
  ss << "(function(){\n";
  ss << "function nodeInfo(n,d){ if(!n||d<0) return null; if(n.nodeType!==1) return null; var c=[]; var kids=n.children; var m=kids?Math.min(kids.length," << maxChildren << "):0; for(var i=0;i<m;i++){ var ci=nodeInfo(kids[i],d-1); if(ci) c.push(ci);} return { tag:n.tagName.toLowerCase(), id:n.id||'', cls:(n.className||'').toString(), children:c }; }\n";
  ss << "var r=nodeInfo(document.documentElement," << depth << "); return JSON.stringify(r||{});\n";
  ss << "})()";
  String res;
  view()->EvaluateScript(String(ss.str().c_str()), &res);
  return JSValue(res);
}

JSValue Tab::QI_GetNodeRect(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 1 || !view())
    return JSValue(String("{}"));
  String sel = args[0];
  auto u = sel.utf8();
  std::string jssel = u.data() ? u.data() : "";
  std::string script = std::string("(function(){ var el=document.querySelector(\"") + jssel + "\"); if(!el) return '{}'; var r=el.getBoundingClientRect(); return JSON.stringify({left:r.left,top:r.top,width:r.width,height:r.height}); })()";
  String res;
  view()->EvaluateScript(String(script.c_str()), &res);
  return JSValue(res);
}

void Tab::QI_StartPicker(const JSObject &obj, const JSArgs &args)
{
  if (!view())
    return;
  RefPtr<JSContext> ctx = view()->LockJSContext();
  SetJSContext(ctx->ctx());
  JSObject global = JSGlobalObject();
  global["NativeInspectorPickHover"] = BindJSCallback(&Tab::OnPickerHover);
  global["NativeInspectorPickSelect"] = BindJSCallback(&Tab::OnPickerSelect);
  const char *picker = R"JS((function(){
    if (window.__ul_picker_active) return; window.__ul_picker_active = true;
    if (!window.__ul_pick_overlay){ var o=document.createElement('div'); o.style.cssText='position:fixed;pointer-events:none;z-index:2147483647;border:2px solid #4af;background:rgba(68,170,255,.15)'; document.documentElement.appendChild(o); window.__ul_pick_overlay=o; }
    function cssPath(el){ try{ var p=[]; for(;el&&el.nodeType===1; el=el.parentElement){ var s=el.nodeName.toLowerCase(); if(el.id){ s += '#'+el.id; p.unshift(s); break; } else { var cls = (el.className||'').toString().trim().split(/\s+/).filter(Boolean); if(cls.length) s += '.'+cls.join('.'); var idx=1, sib=el; while((sib=sib.previousElementSibling)!=null){ if(sib.nodeName===el.nodeName) idx++; } p.unshift(s+':nth-of-type('+idx+')'); } } return p.join('>');} catch(e){ return ''; } }
    function update(el){ if(!el) return; var r=el.getBoundingClientRect(); var o=window.__ul_pick_overlay; o.style.left=r.left+'px'; o.style.top=r.top+'px'; o.style.width=r.width+'px'; o.style.height=r.height+'px'; }
    window.__ul_pick_move = function(e){ var t=e.target; update(t); try{ if(window.NativeInspectorPickHover) NativeInspectorPickHover(JSON.stringify({selector:cssPath(t)})); }catch(_){} };
    window.__ul_pick_click = function(e){ e.preventDefault(); e.stopPropagation(); var t=e.target; try{ if(window.NativeInspectorPickSelect) NativeInspectorPickSelect(JSON.stringify({selector:cssPath(t)})); }catch(_){} };
    document.addEventListener('mousemove', window.__ul_pick_move, true);
    document.addEventListener('click', window.__ul_pick_click, true);
  })())JS";
  view()->EvaluateScript(String(picker), nullptr);
}

void Tab::QI_StopPicker(const JSObject &obj, const JSArgs &args)
{
  if (!view())
    return;
  const char *stop = R"JS((function(){
    if (!window.__ul_picker_active) return; window.__ul_picker_active=false;
    document.removeEventListener('mousemove', window.__ul_pick_move, true);
    document.removeEventListener('click', window.__ul_pick_click, true);
    if (window.__ul_pick_overlay && window.__ul_pick_overlay.parentNode) window.__ul_pick_overlay.parentNode.removeChild(window.__ul_pick_overlay);
    window.__ul_pick_overlay=null;
  })())JS";
  view()->EvaluateScript(String(stop), nullptr);
}

void Tab::QI_HighlightSelector(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 1 || !view())
    return;
  String sel = args[0];
  auto u = sel.utf8();
  std::string jssel = u.data() ? u.data() : "";
  std::string script = std::string("(function(){ var el=document.querySelector(\"") + jssel + "\"); if(!el) return; if(!window.__ul_pick_overlay){ var o=document.createElement('div'); o.style.cssText='position:fixed;pointer-events:none;z-index:2147483647;border:2px solid #4af;background:rgba(68,170,255,.15)'; document.documentElement.appendChild(o); window.__ul_pick_overlay=o; } var r=el.getBoundingClientRect(); var o=window.__ul_pick_overlay; o.style.left=r.left+'px'; o.style.top=r.top+'px'; o.style.width=r.width+'px'; o.style.height=r.height+'px'; })()";
  view()->EvaluateScript(String(script.c_str()), nullptr);
}

JSValue Tab::QI_GetComputedStyle(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 1 || !view())
    return JSValue(String("{}"));
  String sel = args[0];
  auto u = sel.utf8();
  std::string raw = u.data() ? u.data() : "";
  auto esc = [](const std::string &in)
  {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in)
    {
      if (c == '\\')
        out += "\\\\";
      else if (c == '"')
        out += "\\\"";
      else if (c == '\n')
        out += "\\n";
      else if (c == '\r')
        out += "\\r";
      else
        out += c;
    }
    return out;
  };
  std::ostringstream ss;
  ss << "(function(){ var el=document.querySelector(\"" << esc(raw) << "\"); if(!el) return '{}'; var cs=getComputedStyle(el); var out={}; try{ for(var i=0;i<cs.length;i++){ var k=cs.item(i); out[k]=cs.getPropertyValue(k); } }catch(e){} return JSON.stringify(out); })()";
  String res;
  std::string js = ss.str();
  view()->EvaluateScript(String(js.c_str()), &res);
  return JSValue(res);
}

JSValue Tab::QI_GetStorage(const JSObject &obj, const JSArgs &args)
{
  if (!view())
    return JSValue(String("{}"));
  const char *script = R"JS((function(){
    function dumpLS(ls){ var o={}; try{ for(var i=0;i<ls.length;i++){ var k=ls.key(i); o[k]=ls.getItem(k); } }catch(e){} return o; }
    var out={ localStorage: dumpLS(window.localStorage||{}), sessionStorage: dumpLS(window.sessionStorage||{}), cookies: document.cookie||'' };
    return JSON.stringify(out);
  })())JS";
  String res;
  view()->EvaluateScript(String(script), &res);
  return JSValue(res);
}

JSValue Tab::QI_GetPerformance(const JSObject &obj, const JSArgs &args)
{
  if (!view())
    return JSValue(String("{}"));
  const char *script = R"JS((function(){
    var nav = (performance.getEntriesByType && performance.getEntriesByType('navigation'));
    var resources = (performance.getEntriesByType && performance.getEntriesByType('resource'))||[];
    var t = performance.timing || {};
    return JSON.stringify({ navigation: nav && nav[0] || {}, resourceCount: resources.length, timing: t });
  })())JS";
  String res;
  view()->EvaluateScript(String(script), &res);
  return JSValue(res);
}

JSValue Tab::QI_GetOuterHTML(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 1 || !view())
    return JSValue(String(""));
  String sSel = args[0];
  auto us = sSel.utf8();
  std::string sel = us.data() ? us.data() : "";
  auto esc = [](const std::string &in)
  {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in)
    {
      if (c == '\\')
        out += "\\\\";
      else if (c == '"')
        out += "\\\"";
      else if (c == '\n')
        out += "\\n";
      else if (c == '\r')
        out += "\\r";
      else
        out += c;
    }
    return out;
  };
  std::string js = std::string("(function(){ var el=document.querySelector(\"") + esc(sel) + "\"); if(!el) return ''; return el.outerHTML; })()";
  String res;
  view()->EvaluateScript(String(js.c_str()), &res);
  return JSValue(res);
}

void Tab::QI_SetAttribute(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 3 || !view())
    return;
  String sSel = args[0];
  String sName = args[1];
  String sVal = args[2];
  auto us = sSel.utf8();
  auto un = sName.utf8();
  auto uv = sVal.utf8();
  std::string sel = us.data() ? us.data() : "";
  std::string name = un.data() ? un.data() : "";
  std::string val = uv.data() ? uv.data() : "";
  auto esc = [](const std::string &in)
  {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in)
    {
      if (c == '\\')
        out += "\\\\";
      else if (c == '"')
        out += "\\\"";
      else if (c == '\n')
        out += "\\n";
      else if (c == '\r')
        out += "\\r";
      else
        out += c;
    }
    return out;
  };
  std::ostringstream ss;
  ss << "(function(){ var el=document.querySelector(\"" << esc(sel) << "\"); if(!el) return; el.setAttribute(\"" << esc(name) << "\",\"" << esc(val) << "\"); })()";
  std::string js = ss.str();
  view()->EvaluateScript(String(js.c_str()), nullptr);
}

void Tab::QI_RemoveAttribute(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 2 || !view())
    return;
  String sSel = args[0];
  String sName = args[1];
  auto us = sSel.utf8();
  auto un = sName.utf8();
  std::string sel = us.data() ? us.data() : "";
  std::string name = un.data() ? un.data() : "";
  auto esc = [](const std::string &in)
  {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in)
    {
      if (c == '\\')
        out += "\\\\";
      else if (c == '"')
        out += "\\\"";
      else if (c == '\n')
        out += "\\n";
      else if (c == '\r')
        out += "\\r";
      else
        out += c;
    }
    return out;
  };
  std::ostringstream ss;
  ss << "(function(){ var el=document.querySelector(\"" << esc(sel) << "\"); if(!el) return; el.removeAttribute(\"" << esc(name) << "\"); })()";
  std::string js = ss.str();
  view()->EvaluateScript(String(js.c_str()), nullptr);
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

JSValue Tab::OnDownloadsGetData(const JSObject &obj, const JSArgs &args)
{
  if (!ui_)
    return JSValue(String("{\"items\":[]}"));
  return JSValue(ui_->GetDownloadsJSON());
}

void Tab::OnDownloadsClear(const JSObject &obj, const JSArgs &args)
{
  if (ui_)
    ui_->ClearCompletedDownloads();
}

void Tab::OnDownloadsOpen(const JSObject &obj, const JSArgs &args)
{
  if (!ui_ || args.empty())
    return;
  uint64_t id = static_cast<uint64_t>((double)args[0]);
  ui_->OpenDownloadItem(id);
}

void Tab::OnDownloadsReveal(const JSObject &obj, const JSArgs &args)
{
  if (!ui_ || args.empty())
    return;
  uint64_t id = static_cast<uint64_t>((double)args[0]);
  ui_->RevealDownloadItem(id);
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

// --- Settings page JS bridge (forward to UI) ---
JSValue Tab::JS_GetSettingsSnapshot(const JSObject &obj, const JSArgs &args)
{
  if (!ui_)
    return JSValue();
  return ui_->OnGetSettings(obj, args);
}

void Tab::JS_UpdateSetting(const JSObject &obj, const JSArgs &args)
{
  if (ui_)
    ui_->OnUpdateSetting(obj, args);
}

void Tab::JS_SaveSettings(const JSObject &obj, const JSArgs &args)
{
  if (ui_)
    ui_->OnSaveSettings(obj, args);
}

JSValue Tab::JS_RestoreSettingsDefaults(const JSObject &obj, const JSArgs &args)
{
  if (!ui_)
    return JSValue();
  return ui_->OnRestoreSettingsDefaults(obj, args);
}

// (Disable-history removed)
