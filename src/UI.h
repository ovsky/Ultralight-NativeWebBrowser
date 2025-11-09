#pragma once
#include <AppCore/AppCore.h>
#include "Tab.h"
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

using ultralight::JSArgs;
using ultralight::JSFunction;
using ultralight::JSObject;
using namespace ultralight;

class Console;
class AdBlocker; // forward declaration, optional dependency
class DownloadManager;

/**
 * Browser UI implementation. Renders the toolbar/addressbar/tabs in top pane.
 */
class UI : public WindowListener,
           public LoadListener,
           public ViewListener
{
public:
  UI(RefPtr<Window> window);
  // Overload retained for compatibility; adblockers are optional and may be unused.
  UI(RefPtr<Window> window, AdBlocker *adblock, AdBlocker *tracker);
  ~UI();

  struct BrowserSettings
  {
    // Appearance
    bool launch_dark_theme = false;
    bool vibrant_window_theme = false;
    bool experimental_transparent_toolbar = false;
    bool experimental_compact_tabs = false;

    // Privacy & Security
    bool enable_adblock = true;
    bool log_blocked_requests = false;
    bool clear_history_on_exit = true;
    bool enable_javascript = true;
    bool enable_web_security = true;
    bool block_third_party_cookies = false;
    bool do_not_track = true;

    // Address Bar & Suggestions
    bool enable_suggestions = true;
    bool enable_suggestion_favicons = true;

    // Downloads
    bool show_download_badge = true;
    bool auto_open_download_panel = true;
    bool ask_download_location = false;

    // Performance
    bool smooth_scrolling = true;
    bool hardware_acceleration = true;
    bool enable_local_storage = true;
    bool enable_database = true;

    // Accessibility
    bool reduce_motion = false;
    bool high_contrast_ui = false;
    bool enable_caret_browsing = false;

    // Developer
    bool enable_remote_inspector = false;
    bool show_performance_overlay = false;

    bool operator==(const BrowserSettings &other) const;
    bool operator!=(const BrowserSettings &other) const { return !(*this == other); }
  };

  // Inherited from WindowListener
  virtual bool OnKeyEvent(const ultralight::KeyEvent &evt) override;
  virtual bool OnMouseEvent(const ultralight::MouseEvent &evt) override;
  virtual void OnClose(ultralight::Window *window) override;
  virtual void OnResize(ultralight::Window *window, uint32_t width, uint32_t height) override;

  // Inherited from LoadListener
  virtual void OnDOMReady(View *caller, uint64_t frame_id,
                          bool is_main_frame, const String &url) override;

  // Inherited from ViewListener
  virtual void OnChangeCursor(ultralight::View *caller, Cursor cursor) override { SetCursor(cursor); }

  // Called by UI JavaScript
  void OnBack(const JSObject &obj, const JSArgs &args);
  void OnForward(const JSObject &obj, const JSArgs &args);
  void OnRefresh(const JSObject &obj, const JSArgs &args);
  void OnStop(const JSObject &obj, const JSArgs &args);
  void OnToggleTools(const JSObject &obj, const JSArgs &args);
  void OnRequestNewTab(const JSObject &obj, const JSArgs &args);
  void OnRequestTabClose(const JSObject &obj, const JSArgs &args);
  void OnActiveTabChange(const JSObject &obj, const JSArgs &args);
  void OnRequestChangeURL(const JSObject &obj, const JSArgs &args);
  // Explicitly called when user pressed Enter in the address bar
  void OnAddressBarNavigate(const JSObject &obj, const JSArgs &args);
  // Open History page in a new tab (used by menu button and shortcuts)
  void OnOpenHistoryNewTab(const JSObject &obj, const JSArgs &args);
  void OnOpenDownloadsNewTab(const JSObject &obj, const JSArgs &args);
  void OnAddressBarBlur(const JSObject &obj, const JSArgs &args);
  void OnAddressBarFocus(const JSObject &obj, const JSArgs &args);
  void OnMenuOpen(const JSObject &obj, const JSArgs &args);
  void OnMenuClose(const JSObject &obj, const JSArgs &args);
  void OnDownloadsOverlayToggle(const JSObject &obj, const JSArgs &args);
  void OnDownloadsOverlayClose(const JSObject &obj, const JSArgs &args);
  void OnContextMenuAction(const JSObject &obj, const JSArgs &args);
  void OnToggleDarkMode(const JSObject &obj, const JSArgs &args);
  ultralight::JSValue OnDownloadsOverlayGet(const JSObject &obj, const JSArgs &args);
  void OnDownloadsOverlayClear(const JSObject &obj, const JSArgs &args);
  void OnDownloadsOverlayOpenItem(const JSObject &obj, const JSArgs &args);
  void OnDownloadsOverlayRevealItem(const JSObject &obj, const JSArgs &args);
  void OnDownloadsOverlayPauseItem(const JSObject &obj, const JSArgs &args);
  void OnDownloadsOverlayRemoveItem(const JSObject &obj, const JSArgs &args);
  ultralight::JSValue OnGetDarkModeEnabled(const JSObject &obj, const JSArgs &args);
  void OnToggleAdblock(const JSObject &obj, const JSArgs &args);
  ultralight::JSValue OnGetAdblockEnabled(const JSObject &obj, const JSArgs &args);
  void OnOpenSettingsPanel(const JSObject &obj, const JSArgs &args);
  void OnCloseSettingsPanel(const JSObject &obj, const JSArgs &args);
  ultralight::JSValue OnGetSettings(const JSObject &obj, const JSArgs &args);
  void OnUpdateSetting(const JSObject &obj, const JSArgs &args);
  ultralight::JSValue OnRestoreSettingsDefaults(const JSObject &obj, const JSArgs &args);
  void OnSaveSettings(const JSObject &obj, const JSArgs &args);
  // Suggestions callback (address bar autocomplete)
  ultralight::JSValue OnGetSuggestions(const JSObject &obj, const JSArgs &args);
  // Adjust UI overlay height for suggestions dropdown
  void OnSuggestOpen(const JSObject &obj, const JSArgs &args);
  void OnSuggestClose(const JSObject &obj, const JSArgs &args);

  RefPtr<Window> window() { return window_; }
  DownloadManager *download_manager() { return download_manager_.get(); }

protected:
  static std::filesystem::path SettingsDirectory();
  static std::filesystem::path SettingsFilePath();
  static std::filesystem::path LegacySettingsFilePath();
  void CreateNewTab();
  RefPtr<View> CreateNewTabForChildView(const String &url);
  void UpdateTabTitle(uint64_t id, const String &title);
  void UpdateTabURL(uint64_t id, const String &url);
  void UpdateTabNavigation(uint64_t id, bool is_loading, bool can_go_back, bool can_go_forward);

  void SetLoading(bool is_loading);
  void SetCanGoBack(bool can_go_back);
  void SetCanGoForward(bool can_go_forward);
  void SetURL(const String &url);
  void SetCursor(Cursor cursor);
  void AdjustUIHeight(uint32_t new_height);
  void ShowMenuOverlay();
  void HideMenuOverlay();
  void ShowDownloadsOverlay();
  void HideDownloadsOverlay();
  void LayoutDownloadsOverlay();
  void ShowContextMenuOverlay(int x, int y, const ultralight::String &json_info);
  void HideContextMenuOverlay();
  // Suggestions overlay (above all UI)
  void ShowSuggestionsOverlay(int x, int y, int width, const ultralight::String &json_items);
  void HideSuggestionsOverlay();

  // History management
  void RecordHistory(const String &url, const String &title);
  String GetHistoryJSON();
  void ClearHistory();

  // Downloads management helpers
  String GetDownloadsJSON();
  void ClearCompletedDownloads();
  bool OpenDownloadItem(uint64_t id);
  bool RevealDownloadItem(uint64_t id);
  bool PauseDownloadItem(uint64_t id);
  bool RemoveDownloadItem(uint64_t id);
  void NotifyDownloadsChanged();
  void OnNewDownloadStarted();
  void SyncAdblockStateToUI();
  void SyncSettingsStateToUI(bool snapshot_is_baseline = false);
  void ApplySettings(bool initial, bool snapshot_is_baseline);
  void SetDarkModeEnabled(bool enabled);
  void LoadSettingsFromDisk();
  bool SaveSettingsToDisk();
  void EnsureDataDirectoryExists();
  void RestoreSettingsToDefaults();
  std::string BuildSettingsJSON() const;
  std::string BuildSettingsPayload(bool snapshot_is_baseline) const;
  bool ParseSettingsBool(const std::string &buffer, const char *key, bool fallback) const;
  void HandleSettingMutation(const std::string &key, bool value);
  void UpdateSettingsDirtyFlag();

  // Suggestions / persistence helpers
  void LoadPopularSites();
  void LoadHistoryFromDisk();
  void SaveHistoryToDisk();
  std::vector<std::string> GetSuggestions(const std::string &input, int maxResults);
  // JS bridge to open/close suggestions overlay and pick
  void OnOpenSuggestionsOverlay(const JSObject &obj, const JSArgs &args);
  void OnCloseSuggestionsOverlay(const JSObject &obj, const JSArgs &args);
  void OnSuggestionPick(const JSObject &obj, const JSArgs &args);
  // Paste a suggestion URL into the address bar without navigating
  void OnSuggestionPaste(const JSObject &obj, const JSArgs &args);
  // Receive favicon image data (as data URL) from suggestions overlay and persist cache
  void OnFaviconReady(const JSObject &obj, const JSArgs &args);

  // Compute a best-effort favicon URL (origin + "/favicon.ico") for http/https URLs
  String GetFaviconURL(const String &page_url);
  // Get origin string (scheme+host+optional port)
  static std::string GetOriginStringFromURL(const std::string &url);
  // Favicon disk cache
  void LoadFaviconDiskCache();
  void SaveFaviconDiskCache();
  std::string EnsureFaviconCacheDir();
  static std::string Base64Decode(const std::string &in);
  double GetOriginScore(const std::string &origin);
  void PruneFaviconDiskCacheToLimit();

  Tab *active_tab() { return tabs_.empty() ? nullptr : tabs_[active_tab_id_].get(); }

  RefPtr<View> view() { return overlay_->view(); }

  RefPtr<Window> window_;
  RefPtr<Overlay> overlay_;
  int ui_height_;
  int base_ui_height_;
  int tab_height_;
  RefPtr<Overlay> menu_overlay_;
  RefPtr<Overlay> downloads_overlay_;
  RefPtr<Overlay> context_menu_overlay_;
  RefPtr<Overlay> suggestions_overlay_;
  float scale_;
  // Optional ad/tracker blocker references (may be unused in this build)
  AdBlocker *adblock_ = nullptr;
  AdBlocker *trackerblock_ = nullptr;
  std::unique_ptr<DownloadManager> download_manager_;
  bool downloads_overlay_had_active_ = false;
  bool downloads_overlay_user_dismissed_ = false;
  uint64_t downloads_last_sequence_seen_ = 0;
  // Transient context menu state
  std::pair<int, int> pending_ctx_position_ = {0, 0};
  ultralight::String pending_ctx_info_json_;
  // Which view the context menu operates on (0=none,1=UI overlay,2=page/tab)
  int ctx_target_ = 0;
  // Transient suggestions state
  std::pair<int, int> pending_sugg_position_ = {0, 0};
  int pending_sugg_width_ = 0;
  ultralight::String pending_sugg_json_;

  std::map<uint64_t, std::unique_ptr<Tab>> tabs_;
  uint64_t active_tab_id_ = 0;
  uint64_t tab_id_counter_ = 0;
  Cursor cur_cursor_;
  bool is_resizing_inspector_;
  bool is_over_inspector_resize_drag_handle_;
  int inspector_resize_begin_height_;
  int inspector_resize_begin_mouse_y_;
  bool address_bar_is_focused_ = false;
  bool adblock_enabled_cached_ = true;
  bool suggestions_enabled_ = true;
  bool show_download_badge_ = true;
  bool auto_open_download_panel_ = true;
  bool clear_history_on_exit_ = false;
  bool experimental_transparent_toolbar_enabled_ = false;
  bool experimental_compact_tabs_enabled_ = false;
  bool reduce_motion_enabled_ = false;
  bool high_contrast_ui_enabled_ = false;
  bool vibrant_window_theme_enabled_ = false;

  JSFunction updateBack;
  JSFunction updateForward;
  JSFunction updateLoading;
  JSFunction updateURL;
  JSFunction addTab;
  JSFunction updateTab;
  JSFunction closeTab;
  JSFunction focusAddressBar;
  JSFunction isAddressBarFocused;
  JSFunction updateAdblockEnabled;
  JSFunction applySettings;
  JSFunction applySettingsPanel;
  // Context menu setup function in overlay view
  JSFunction setupContextMenu;
  JSFunction setupSuggestions;

  // Cache favicon URL per site origin so multiple tabs/pages reuse it
  // Key: origin string (eg, https://example.com), Value: favicon URL
  std::map<std::string, std::string> favicon_cache_;
  // Disk-persisted favicon file cache (origin -> file path)
  std::map<std::string, std::string> favicon_file_cache_;
  size_t favicon_cache_limit_ = 128;

  // Simple in-memory history
  struct HistoryEntry
  {
    std::string url;
    std::string title;
    uint64_t timestamp_ms;
    uint32_t visit_count;
  };
  std::vector<HistoryEntry> history_;
  // Always enabled (disable-history feature removed)

  // Popular sites loaded from assets/popular_sites.json
  std::vector<std::string> popular_sites_;

  // Suggestions favicons toggle (read from assets/suggestions_favicons.txt: on/off)
  bool suggestion_favicons_enabled_ = true;
  void LoadSuggestionsFaviconsFlag();

  // Auto Dark Mode state
  bool dark_mode_enabled_ = false;
  void ApplyDarkModeToView(RefPtr<View> v);
  void RemoveDarkModeFromView(RefPtr<View> v);

  // Shortcuts mapping (eg, "Ctrl+T" -> "new-tab") loaded from assets/shortcuts.json
  std::map<std::string, std::string> shortcuts_;
  void LoadShortcuts();
  bool RunShortcutAction(const std::string &action);

  BrowserSettings settings_;

  BrowserSettings saved_settings_;
  bool settings_dirty_ = false;
  std::string settings_storage_path_;

  friend class Tab;
};
