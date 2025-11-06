#pragma once
#include <AppCore/AppCore.h>
#include <Ultralight/Listener.h>

class UI;
using namespace ultralight;

/**
 * Browser Tab UI implementation. Renders the actual page content in bottom pane.
 */
class Tab : public ViewListener,
            public LoadListener
{
public:
  Tab(UI *ui, uint64_t id, uint32_t width, uint32_t height, int x, int y);
  ~Tab();

  void set_ready_to_close(bool ready) { ready_to_close_ = ready; }
  bool ready_to_close() { return ready_to_close_; }

  RefPtr<View> view() { return overlay_->view(); }

  void Show();

  void Hide();

  void ToggleInspector();

  bool IsInspectorShowing() const;

  IntRect GetInspectorResizeDragHandle() const;

  int GetInspectorHeight() const;

  void SetInspectorHeight(int height);

  void Resize(uint32_t width, uint32_t height);

  // Move the tab's overlay to a new position (x, y)
  void MoveTo(uint32_t x, uint32_t y);

  // Inherited from Listener::View
  virtual void OnChangeTitle(View *caller, const String &title) override;
  virtual void OnChangeURL(View *caller, const String &url) override;
  virtual void OnChangeTooltip(View *caller, const String &tooltip) override;
  virtual void OnChangeCursor(View *caller, Cursor cursor) override;
  virtual void OnAddConsoleMessage(View *caller, const ConsoleMessage &msg) override;
  virtual RefPtr<View> OnCreateChildView(ultralight::View *caller,
                                         const String &opener_url, const String &target_url,
                                         bool is_popup, const IntRect &popup_rect) override;
  virtual RefPtr<View> OnCreateInspectorView(ultralight::View *caller, bool is_local,
                                             const String &inspected_url) override;

  // Inherited from Listener::Load
  virtual void OnBeginLoading(View *caller, uint64_t frame_id,
                              bool is_main_frame, const String &url) override;
  virtual void OnFinishLoading(View *caller, uint64_t frame_id,
                               bool is_main_frame, const String &url) override;
  virtual void OnFailLoading(View *caller, uint64_t frame_id,
                             bool is_main_frame, const String &url, const String &description,
                             const String &error_domain, int error_code) override;
  virtual void OnUpdateHistory(View *caller) override;

  // Inject page-side hooks when DOM is ready to capture right-click context
  virtual void OnDOMReady(View *caller, uint64_t frame_id,
                          bool is_main_frame, const String &url) override;

  // JS callback from page when right-click occurs
  void OnOpenContextMenu(const JSObject &obj, const JSArgs &args);

  // History page callbacks
  JSValue OnHistoryGetData(const JSObject &obj, const JSArgs &args);
  void OnHistoryClear(const JSObject &obj, const JSArgs &args);

  // General native JS bridge callbacks (exposed on window.__ul)
  void JS_Back(const JSObject &obj, const JSArgs &args);
  void JS_Forward(const JSObject &obj, const JSArgs &args);
  void JS_Reload(const JSObject &obj, const JSArgs &args);
  void JS_Stop(const JSObject &obj, const JSArgs &args);
  void JS_Navigate(const JSObject &obj, const JSArgs &args);
  void JS_NewTab(const JSObject &obj, const JSArgs &args);
  void JS_CloseTab(const JSObject &obj, const JSArgs &args);
  void JS_OpenHistory(const JSObject &obj, const JSArgs &args);
  JSValue JS_GetHistory(const JSObject &obj, const JSArgs &args);
  void JS_ClearHistory(const JSObject &obj, const JSArgs &args);
  void JS_ToggleDarkMode(const JSObject &obj, const JSArgs &args);
  JSValue JS_IsDarkModeEnabled(const JSObject &obj, const JSArgs &args);
  JSValue JS_GetAppInfo(const JSObject &obj, const JSArgs &args);

  // Downloads page callbacks
  JSValue OnDownloadsGetData(const JSObject &obj, const JSArgs &args);
  void OnDownloadsClear(const JSObject &obj, const JSArgs &args);
  void OnDownloadsOpen(const JSObject &obj, const JSArgs &args);
  void OnDownloadsReveal(const JSObject &obj, const JSArgs &args);

  // Lightweight Quick Inspector callbacks
  void OnQuickInspectorClose(const JSObject &obj, const JSArgs &args);
  void OnQuickInspectorOpenDevtools(const JSObject &obj, const JSArgs &args);
  JSValue OnQuickInspectorGetInfo(const JSObject &obj, const JSArgs &args);

  // Quick Inspector advanced APIs
  JSValue QI_Eval(const JSObject &obj, const JSArgs &args);
  JSValue QI_GetDOMTree(const JSObject &obj, const JSArgs &args);
  JSValue QI_GetNodeRect(const JSObject &obj, const JSArgs &args);
  void QI_StartPicker(const JSObject &obj, const JSArgs &args);
  void QI_StopPicker(const JSObject &obj, const JSArgs &args);
  void QI_HighlightSelector(const JSObject &obj, const JSArgs &args);
  JSValue QI_GetComputedStyle(const JSObject &obj, const JSArgs &args);
  JSValue QI_GetStorage(const JSObject &obj, const JSArgs &args);
  JSValue QI_GetPerformance(const JSObject &obj, const JSArgs &args);
  JSValue QI_GetOuterHTML(const JSObject &obj, const JSArgs &args);
  void QI_SetAttribute(const JSObject &obj, const JSArgs &args);
  void QI_RemoveAttribute(const JSObject &obj, const JSArgs &args);

  // Page-bound events forwarded to inspector
  void OnNetworkEvent(const JSObject &obj, const JSArgs &args);
  void OnPickerHover(const JSObject &obj, const JSArgs &args);
  void OnPickerSelect(const JSObject &obj, const JSArgs &args);

protected:
  UI *ui_;
  RefPtr<Overlay> overlay_;
  RefPtr<Overlay> inspector_overlay_;
  uint64_t id_;
  bool ready_to_close_ = false;
  uint32_t container_width_, container_height_;
};
