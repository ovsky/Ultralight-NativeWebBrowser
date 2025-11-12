#include "UI.h"
#include <cstring>
#include <cmath>
#include <iostream>
#include <Ultralight/Renderer.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <ctime>
#include <array>
#include <vector>
#include <cstdlib>
#include "DownloadManager.h"
#include "AdBlocker.h"
#ifdef _WIN32
#include <direct.h> // _mkdir, _getcwd
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h> // GetModuleFileNameW
#else
#include <sys/stat.h> // mkdir
#include <unistd.h>   // getcwd
#endif

static UI *g_ui = 0;

#define UI_HEIGHT 80
#define UI_HEIGHT_COMPACT 60 // Reduced height when compact tabs mode is enabled

namespace
{
  constexpr int kDownloadsOverlaySpacing = 8;

  struct SettingDescriptor
  {
    const char *key;
    const char *name;
    const char *description;
    const char *category;
    const char *note;
    bool UI::BrowserSettings::*member;
    bool default_value;
  };

  constexpr std::array<SettingDescriptor, 26> kFallbackSettingsCatalog = {
      // Appearance
      SettingDescriptor{"launch_dark_theme", "Launch in dark theme",
                        "Start Ultralight with dark chrome, toolbars, and tabs by default.",
                        "appearance", nullptr, &UI::BrowserSettings::launch_dark_theme, false},
      SettingDescriptor{"vibrant_window_theme", "Vibrant window theme",
                        "Apply a subtle color wash to the window frame for a livelier finish.",
                        "appearance", nullptr, &UI::BrowserSettings::vibrant_window_theme, false},
      SettingDescriptor{"experimental_transparent_toolbar", "Transparent toolbar",
                        "Blend the toolbar into page content with a translucent, glass-like surface.",
                        "appearance", "Experimental", &UI::BrowserSettings::experimental_transparent_toolbar, false},
      SettingDescriptor{"experimental_compact_tabs", "Compact tabs",
                        "Reduce tab height and spacing so more tabs stay visible without scrolling.",
                        "appearance", "Experimental", &UI::BrowserSettings::experimental_compact_tabs, false},

      // Privacy & Security
      SettingDescriptor{"enable_adblock", "Enable ad blocking",
                        "Filter network requests using bundled block lists to hide intrusive ads.",
                        "privacy", nullptr, &UI::BrowserSettings::enable_adblock, true},
      SettingDescriptor{"log_blocked_requests", "Log blocked requests",
                        "Write each blocked network request to the console for debugging rules.",
                        "privacy", nullptr, &UI::BrowserSettings::log_blocked_requests, false},
      SettingDescriptor{"clear_history_on_exit", "Clear history on exit",
                        "Remove browsing history when Ultralight closes and skip saving new visits.",
                        "privacy", nullptr, &UI::BrowserSettings::clear_history_on_exit, true},
      SettingDescriptor{"enable_javascript", "Enable JavaScript",
                        "Allow websites to run JavaScript code for interactive features and dynamic content.",
                        "privacy", nullptr, &UI::BrowserSettings::enable_javascript, true},
      SettingDescriptor{"enable_web_security", "Enable web security",
                        "Enforce same-origin policy and other web security restrictions.",
                        "privacy", nullptr, &UI::BrowserSettings::enable_web_security, true},
      SettingDescriptor{"block_third_party_cookies", "Block third-party cookies",
                        "Prevent websites from setting cookies that track you across different sites.",
                        "privacy", nullptr, &UI::BrowserSettings::block_third_party_cookies, false},
      SettingDescriptor{"do_not_track", "Send Do Not Track header",
                        "Request that websites not track your browsing activity.",
                        "privacy", nullptr, &UI::BrowserSettings::do_not_track, true},

      // Address Bar & Suggestions
      SettingDescriptor{"enable_suggestions", "Show address bar suggestions",
                        "Surface history matches and popular sites while typing in the address bar.",
                        "suggestions", nullptr, &UI::BrowserSettings::enable_suggestions, true},
      SettingDescriptor{"enable_suggestion_favicons", "Show favicons in suggestions",
                        "Display site icons next to suggestion rows whenever an icon is available.",
                        "suggestions", nullptr, &UI::BrowserSettings::enable_suggestion_favicons, true},

      // Downloads
      SettingDescriptor{"show_download_badge", "Show download badge",
                        "Highlight the toolbar downloads button whenever transfers are active.",
                        "downloads", nullptr, &UI::BrowserSettings::show_download_badge, true},
      SettingDescriptor{"auto_open_download_panel", "Open downloads panel automatically",
                        "Pop open the quick downloads overlay as soon as a new download begins.",
                        "downloads", nullptr, &UI::BrowserSettings::auto_open_download_panel, true},
      SettingDescriptor{"ask_download_location", "Ask where to save downloads",
                        "Show a file picker dialog for each download instead of using default location.",
                        "downloads", nullptr, &UI::BrowserSettings::ask_download_location, false},

      // Performance
      SettingDescriptor{"smooth_scrolling", "Smooth scrolling",
                        "Enable smooth animated scrolling for a more fluid browsing experience.",
                        "performance", nullptr, &UI::BrowserSettings::smooth_scrolling, true},
      SettingDescriptor{"hardware_acceleration", "Hardware acceleration",
                        "Use GPU to accelerate graphics rendering for better performance.",
                        "performance", nullptr, &UI::BrowserSettings::hardware_acceleration, true},
      SettingDescriptor{"enable_local_storage", "Enable local storage",
                        "Allow websites to store data locally for offline functionality.",
                        "performance", nullptr, &UI::BrowserSettings::enable_local_storage, true},
      SettingDescriptor{"enable_database", "Enable database storage",
                        "Allow websites to use IndexedDB and Web SQL for data storage.",
                        "performance", nullptr, &UI::BrowserSettings::enable_database, true},

      // Accessibility
      SettingDescriptor{"reduce_motion", "Reduce motion effects",
                        "Limit animated transitions and parallax flourishes for a calmer experience.",
                        "accessibility", nullptr, &UI::BrowserSettings::reduce_motion, false},
      SettingDescriptor{"high_contrast_ui", "High contrast UI",
                        "Boost contrast for overlays, menus, and dialogs to improve readability.",
                        "accessibility", nullptr, &UI::BrowserSettings::high_contrast_ui, false},
      SettingDescriptor{"enable_caret_browsing", "Enable caret browsing",
                        "Navigate web pages using keyboard cursor like in a text editor.",
                        "accessibility", nullptr, &UI::BrowserSettings::enable_caret_browsing, false},

      // Developer
      SettingDescriptor{"enable_remote_inspector", "Enable remote inspector",
                        "Allow remote debugging via Chrome DevTools Protocol.",
                        "developer", nullptr, &UI::BrowserSettings::enable_remote_inspector, false},
      SettingDescriptor{"show_performance_overlay", "Show performance overlay",
                        "Display FPS counter and rendering statistics on screen.",
                        "developer", nullptr, &UI::BrowserSettings::show_performance_overlay, false}};

  struct ParsedCatalogEntry
  {
    std::string key;
    std::string name;
    std::string description;
    std::string category;
    std::string note;
    bool has_note = false;
    bool has_default = false;
    bool default_value = false;
  };

  struct RuntimeSettingDescriptor
  {
    std::string key;
    std::string name;
    std::string description;
    std::string category;
    std::string note;
    bool UI::BrowserSettings::*member = nullptr;
    bool default_value = false;
  };

  std::vector<RuntimeSettingDescriptor> g_settings_catalog;
  std::unordered_map<std::string, size_t> g_settings_index;
  bool g_settings_initialized = false;

  std::string::size_type FindMatchingBrace(const std::string &text, std::string::size_type open_pos)
  {
    size_t depth = 0;
    for (size_t i = open_pos; i < text.size(); ++i)
    {
      char c = text[i];
      if (c == '{')
        ++depth;
      else if (c == '}')
      {
        if (depth == 0)
          return std::string::npos;
        --depth;
        if (depth == 0)
          return i;
      }
      else if (c == '"')
      {
        // Skip quoted strings entirely (handle escapes)
        ++i;
        bool escape = false;
        for (; i < text.size(); ++i)
        {
          char qc = text[i];
          if (escape)
          {
            escape = false;
            continue;
          }
          if (qc == '\\')
          {
            escape = true;
            continue;
          }
          if (qc == '"')
            break;
        }
      }
    }
    return std::string::npos;
  }

  bool ExtractJsonStringField(const std::string &object, const char *field, std::string &out)
  {
    if (!field)
      return false;
    std::string needle = std::string("\"") + field + "\"";
    size_t pos = object.find(needle);
    if (pos == std::string::npos)
      return false;
    pos = object.find(':', pos + needle.size());
    if (pos == std::string::npos)
      return false;
    ++pos;
    while (pos < object.size() && std::isspace(static_cast<unsigned char>(object[pos])))
      ++pos;
    if (pos >= object.size())
      return false;
    if (object[pos] == 'n' || object[pos] == 'N')
    {
      // Treat explicit null as absence
      if (object.compare(pos, 4, "null") == 0 || object.compare(pos, 4, "NULL") == 0)
        return false;
    }
    if (object[pos] != '"')
      return false;
    ++pos;
    std::string value;
    bool escape = false;
    while (pos < object.size())
    {
      char c = object[pos++];
      if (escape)
      {
        escape = false;
        switch (c)
        {
        case '"':
          value.push_back('"');
          break;
        case '\\':
          value.push_back('\\');
          break;
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        default:
          value.push_back(c);
          break;
        }
        continue;
      }
      if (c == '\\')
      {
        escape = true;
        continue;
      }
      if (c == '"')
        break;
      value.push_back(c);
    }
    out = std::move(value);
    return true;
  }

  bool ExtractJsonBoolField(const std::string &object, const char *field, bool &out)
  {
    if (!field)
      return false;
    std::string needle = std::string("\"") + field + "\"";
    size_t pos = object.find(needle);
    if (pos == std::string::npos)
      return false;
    pos = object.find(':', pos + needle.size());
    if (pos == std::string::npos)
      return false;
    ++pos;
    while (pos < object.size() && std::isspace(static_cast<unsigned char>(object[pos])))
      ++pos;
    if (pos >= object.size())
      return false;
    if (object.compare(pos, 4, "true") == 0 || object[pos] == '1')
    {
      out = true;
      return true;
    }
    if (object.compare(pos, 5, "false") == 0 || object[pos] == '0')
    {
      out = false;
      return true;
    }
    return false;
  }

  void LoadSettingsCatalogFromFile(std::unordered_map<std::string, ParsedCatalogEntry> &out)
  {
    std::ifstream in("assets/settings_catalog.json", std::ios::in | std::ios::binary);
    if (!in.is_open())
      return;
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string json = ss.str();
    in.close();

    size_t pos = 0;
    while (pos < json.size())
    {
      size_t open = json.find('{', pos);
      if (open == std::string::npos)
        break;
      size_t close = FindMatchingBrace(json, open);
      if (close == std::string::npos)
        break;
      std::string object = json.substr(open, close - open + 1);
      pos = close + 1;

      ParsedCatalogEntry entry;
      if (!ExtractJsonStringField(object, "key", entry.key) || entry.key.empty())
        continue;
      ExtractJsonStringField(object, "name", entry.name);
      ExtractJsonStringField(object, "description", entry.description);
      ExtractJsonStringField(object, "category", entry.category);
      std::string noteValue;
      if (ExtractJsonStringField(object, "note", noteValue))
      {
        entry.note = std::move(noteValue);
        entry.has_note = true;
      }
      else if (object.find("\"note\"") != std::string::npos)
      {
        // note was present but null
        entry.note.clear();
        entry.has_note = true;
      }
      bool defValue = false;
      if (ExtractJsonBoolField(object, "default", defValue))
      {
        entry.has_default = true;
        entry.default_value = defValue;
      }

      out[entry.key] = std::move(entry);
    }
  }

  void EnsureSettingsCatalogInitialized()
  {
    if (g_settings_initialized)
      return;
    g_settings_initialized = true;

    std::unordered_map<std::string, ParsedCatalogEntry> parsed_entries;
    LoadSettingsCatalogFromFile(parsed_entries);

    g_settings_catalog.clear();
    g_settings_index.clear();
    g_settings_catalog.reserve(kFallbackSettingsCatalog.size());

    for (const auto &fallback : kFallbackSettingsCatalog)
    {
      RuntimeSettingDescriptor runtime;
      runtime.key = fallback.key ? fallback.key : "";
      runtime.name = fallback.name ? fallback.name : runtime.key;
      runtime.description = fallback.description ? fallback.description : "";
      runtime.category = fallback.category ? fallback.category : "";
      runtime.note = fallback.note ? fallback.note : "";
      runtime.member = fallback.member;
      runtime.default_value = fallback.default_value;

      auto it = parsed_entries.find(runtime.key);
      if (it != parsed_entries.end())
      {
        const ParsedCatalogEntry &meta = it->second;
        if (!meta.name.empty())
          runtime.name = meta.name;
        if (!meta.description.empty())
          runtime.description = meta.description;
        if (!meta.category.empty())
          runtime.category = meta.category;
        if (meta.has_note)
          runtime.note = meta.note;
        if (meta.has_default)
          runtime.default_value = meta.default_value;
      }

      g_settings_index[runtime.key] = g_settings_catalog.size();
      g_settings_catalog.push_back(std::move(runtime));
    }
  }

  const std::vector<RuntimeSettingDescriptor> &GetSettingsCatalog()
  {
    EnsureSettingsCatalogInitialized();
    return g_settings_catalog;
  }

  const RuntimeSettingDescriptor *FindSettingDescriptor(const std::string &key)
  {
    EnsureSettingsCatalogInitialized();
    auto it = g_settings_index.find(key);
    if (it == g_settings_index.end())
      return nullptr;
    return &g_settings_catalog[it->second];
  }

  inline std::string ToIso8601UTC(const std::chrono::system_clock::time_point &tp)
  {
    std::time_t raw = std::chrono::system_clock::to_time_t(tp);
    std::tm utc_tm{};
#if defined(_WIN32)
    gmtime_s(&utc_tm, &raw);
#else
    gmtime_r(&raw, &utc_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
  }

  inline std::string EscapeJson(const std::string &input)
  {
    std::string out;
    out.reserve(input.size() + 8);
    for (char c : input)
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
}

UI::UI(RefPtr<Window> window) : window_(window), cur_cursor_(Cursor::kCursor_Pointer),
                                is_resizing_inspector_(false), is_over_inspector_resize_drag_handle_(false)
{
  uint32_t window_width = window_->width();
  ui_height_ = (uint32_t)std::round(UI_HEIGHT * window_->scale());
  base_ui_height_ = ui_height_;
  overlay_ = Overlay::Create(window_, window_width, ui_height_, 0, 0);
  g_ui = this;

  // Prepare settings and state BEFORE loading the main UI document so the first snapshot reflects persisted values.
  LoadSuggestionsFaviconsFlag();
  EnsureDataDirectoryExists();
  settings_storage_path_ = SettingsFilePath().string();
  LoadSettingsFromDisk();

  // Hook listeners and then load UI document
  view()->set_load_listener(this);
  view()->set_view_listener(this);
  view()->LoadURL("file:///ui.html");

  download_manager_ = std::make_unique<DownloadManager>();
  download_manager_->SetOnChangeCallback([this]()
                                         { NotifyDownloadsChanged(); });

  // Apply runtime toggles (visual sync happens on DOMReady via SyncSettingsStateToUI)
  ApplySettings(true, true);

  // Load keyboard shortcuts mapping
  LoadShortcuts();

  // Load popular sites for suggestions
  LoadPopularSites();
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

  LoadSuggestionsFaviconsFlag();
  EnsureDataDirectoryExists();
  settings_storage_path_ = SettingsFilePath().string();
  LoadSettingsFromDisk();

  view()->set_load_listener(this);
  view()->set_view_listener(this);
  view()->LoadURL("file:///ui.html");

  download_manager_ = std::make_unique<DownloadManager>();
  download_manager_->SetOnChangeCallback([this]()
                                         { NotifyDownloadsChanged(); });

  // Apply runtime toggles (visual sync happens on DOMReady via SyncSettingsStateToUI)
  ApplySettings(true, true);

  // Load keyboard shortcuts mapping
  LoadShortcuts();

  // Load popular sites for suggestions
  LoadPopularSites();
  // Load favicon disk cache
  LoadFaviconDiskCache();

  // Load history from disk
  LoadHistoryFromDisk();

  adblock_enabled_cached_ = adblock_ ? adblock_->enabled() : adblock_enabled_cached_;
}

UI::~UI()
{
  // Persist or clear history on shutdown based on settings
  if (clear_history_on_exit_)
  {
    history_.clear();
    std::remove("data/history.json");
  }
  else
  {
    SaveHistoryToDisk();
  }
  if (settings_dirty_)
    SaveSettingsToDisk();

  if (download_manager_)
    download_manager_->SetOnChangeCallback(nullptr);

  HideMenuOverlay();
  HideDownloadsOverlay();
  HideContextMenuOverlay();
  HideSuggestionsOverlay();

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
  // If downloads overlay is active, route key events to it and consume
  if (downloads_overlay_ && downloads_overlay_->view())
  {
    downloads_overlay_->view()->FireKeyEvent(evt);
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
  if (action == "open-downloads")
  {
    // Toggle downloads overlay
    ShowDownloadsOverlay();
    return true;
  }
  if (action == "open-settings")
  {
    // Open Settings in a NEW tab (like Ctrl+H opens history)
    RefPtr<View> child = CreateNewTabForChildView(String("file:///settings.html"));
    if (child)
    {
      child->LoadURL("file:///settings.html");
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
  if (downloads_overlay_ && downloads_overlay_->view())
  {
    int overlay_x = downloads_overlay_->x();
    int overlay_y = downloads_overlay_->y();
    uint32_t overlay_w = downloads_overlay_->width();
    uint32_t overlay_h = downloads_overlay_->height();

    if (evt.x >= overlay_x && evt.x < overlay_x + static_cast<int>(overlay_w) &&
        evt.y >= overlay_y && evt.y < overlay_y + static_cast<int>(overlay_h))
    {
      ultralight::MouseEvent adjusted = evt;
      adjusted.x -= overlay_x;
      adjusted.y -= overlay_y;
      downloads_overlay_->view()->FireMouseEvent(adjusted);
      return false;
    }
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
    if (downloads_overlay_)
    {
      downloads_overlay_user_dismissed_ = true;
      HideDownloadsOverlay();
    }
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
  if (downloads_overlay_)
    LayoutDownloadsOverlay();

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
  bool is_downloads_overlay_view = url_utf8.data() && std::strstr(url_utf8.data(), "downloads-panel.html") != nullptr;
  bool is_settings_page_view = url_utf8.data() && std::strstr(url_utf8.data(), "settings.html") != nullptr;

  if (!is_menu_view && !is_ctx_view && !is_sugg_view && !is_downloads_overlay_view && !is_settings_page_view)
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
    updateAdblockEnabled = global["updateAdblockEnabled"];
    applySettings = global["applySettings"];
  }

  global["OnBack"] = BindJSCallback(&UI::OnBack);
  global["OnForward"] = BindJSCallback(&UI::OnForward);
  global["OnRefresh"] = BindJSCallback(&UI::OnRefresh);
  global["OnStop"] = BindJSCallback(&UI::OnStop);
  global["OnToggleTools"] = BindJSCallback(&UI::OnToggleTools);
  global["OnMenuOpen"] = BindJSCallback(&UI::OnMenuOpen);
  global["OnMenuClose"] = BindJSCallback(&UI::OnMenuClose);
  global["OnDownloadsOverlayToggle"] = BindJSCallback(&UI::OnDownloadsOverlayToggle);
  global["OnDownloadsOverlayClose"] = BindJSCallback(&UI::OnDownloadsOverlayClose);
  global["OnToggleDarkMode"] = BindJSCallback(&UI::OnToggleDarkMode);
  global["GetDarkModeEnabled"] = BindJSCallbackWithRetval(&UI::OnGetDarkModeEnabled);
  global["OnToggleAdblock"] = BindJSCallback(&UI::OnToggleAdblock);
  global["GetAdblockEnabled"] = BindJSCallbackWithRetval(&UI::OnGetAdblockEnabled);
  global["OnOpenSettingsPanel"] = BindJSCallback(&UI::OnOpenSettingsPanel);
  global["OnCloseSettingsPanel"] = BindJSCallback(&UI::OnCloseSettingsPanel);

  // Bind settings bridge functions to ALL views (not just UI overlay)
  // This ensures settings.html can call GetSettingsSnapshot when loaded in a tab
  global["GetSettingsSnapshot"] = BindJSCallbackWithRetval(&UI::OnGetSettings);
  global["OnUpdateSetting"] = BindJSCallback(&UI::OnUpdateSetting);
  global["OnRestoreSettingsDefaults"] = BindJSCallbackWithRetval(&UI::OnRestoreSettingsDefaults);
  global["OnSaveSettings"] = BindJSCallback(&UI::OnSaveSettings);

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
  global["OnOpenDownloadsNewTab"] = BindJSCallback(&UI::OnOpenDownloadsNewTab);
  global["GetDownloadsSnapshot"] = BindJSCallbackWithRetval(&UI::OnDownloadsOverlayGet);
  global["ClearDownloadsSnapshot"] = BindJSCallback(&UI::OnDownloadsOverlayClear);
  global["OnAddressBarBlur"] = BindJSCallback(&UI::OnAddressBarBlur);
  global["OnAddressBarFocus"] = BindJSCallback(&UI::OnAddressBarFocus);
  global["GetSuggestions"] = BindJSCallbackWithRetval(&UI::OnGetSuggestions);
  global["OpenSuggestionsOverlay"] = BindJSCallback(&UI::OnOpenSuggestionsOverlay);
  global["CloseSuggestionsOverlay"] = BindJSCallback(&UI::OnCloseSuggestionsOverlay);
  global["OnSuggestOpen"] = BindJSCallback(&UI::OnSuggestOpen);
  global["OnSuggestClose"] = BindJSCallback(&UI::OnSuggestClose);

  if (is_downloads_overlay_view)
  {
    global["NativeGetDownloads"] = BindJSCallbackWithRetval(&UI::OnDownloadsOverlayGet);
    global["NativeClearDownloads"] = BindJSCallback(&UI::OnDownloadsOverlayClear);
    global["NativeOpenDownload"] = BindJSCallback(&UI::OnDownloadsOverlayOpenItem);
    global["NativeRevealDownload"] = BindJSCallback(&UI::OnDownloadsOverlayRevealItem);
    global["NativePauseDownload"] = BindJSCallback(&UI::OnDownloadsOverlayPauseItem);
    global["NativeRemoveDownload"] = BindJSCallback(&UI::OnDownloadsOverlayRemoveItem);
  }

  if (is_settings_page_view)
  {
    // Settings page is loaded - hydrate it with current settings immediately
    applySettingsPanel = global["applySettingsState"];
    if (applySettingsPanel)
    {
      std::string payload = BuildSettingsPayload(true);
      std::cout << "[OnDOMReady] Settings page loaded, calling applySettingsState with payload" << std::endl;
      applySettingsPanel({String(payload.c_str())});
    }
    else
    {
      std::cout << "[OnDOMReady] Settings page loaded but applySettingsState not yet defined" << std::endl;
    }
  }

  if (!is_menu_view && !is_ctx_view && !is_sugg_view && !is_downloads_overlay_view && !is_settings_page_view)
  {
    SyncAdblockStateToUI();
    SyncSettingsStateToUI(true);
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

void UI::OnOpenDownloadsNewTab(const JSObject &obj, const JSArgs &args)
{
  RefPtr<View> child = CreateNewTabForChildView(String("file:///downloads.html"));
  if (child)
    child->LoadURL("file:///downloads.html");
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
      if (!t.empty())
        e.title = t;
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
    size_t idx = 0;
    uint64_t oldest = UINT64_MAX;
    size_t oldest_i = 0;
    for (idx = 0; idx < history_.size(); ++idx)
    {
      if (history_[idx].timestamp_ms < oldest)
      {
        oldest = history_[idx].timestamp_ms;
        oldest_i = idx;
      }
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

String UI::GetDownloadsJSON()
{
  if (!download_manager_)
    return String("{\"items\":[]}");
  std::string json = download_manager_->GetDownloadsJSON();
  return String(json.c_str());
}

void UI::ClearCompletedDownloads()
{
  if (!download_manager_)
    return;
  download_manager_->ClearFinishedDownloads();
  NotifyDownloadsChanged();
}

bool UI::OpenDownloadItem(uint64_t id)
{
  if (!download_manager_)
    return false;
  return download_manager_->OpenDownload(static_cast<DownloadManager::DownloadId>(id));
}

bool UI::RevealDownloadItem(uint64_t id)
{
  if (!download_manager_)
    return false;
  return download_manager_->RevealDownload(static_cast<DownloadManager::DownloadId>(id));
}

bool UI::PauseDownloadItem(uint64_t id)
{
  if (!download_manager_)
    return false;
  return download_manager_->CancelDownload(static_cast<DownloadManager::DownloadId>(id));
}

bool UI::RemoveDownloadItem(uint64_t id)
{
  if (!download_manager_)
    return false;
  return download_manager_->RemoveDownload(static_cast<DownloadManager::DownloadId>(id));
}

void UI::NotifyDownloadsChanged()
{
  if (download_manager_)
  {
    download_manager_->PruneStaleRequests();
    uint64_t latest_sequence = download_manager_->last_started_sequence();
    if (latest_sequence != 0 && latest_sequence != downloads_last_sequence_seen_)
    {
      downloads_last_sequence_seen_ = latest_sequence;
      OnNewDownloadStarted();
    }
  }

  bool has_active = download_manager_ ? download_manager_->HasActiveDownloads() : false;
  if (auto_open_download_panel_ && has_active && !downloads_overlay_had_active_)
  {
    downloads_overlay_user_dismissed_ = false;
    ShowDownloadsOverlay();
  }
  downloads_overlay_had_active_ = has_active;

  for (auto &entry : tabs_)
  {
    auto &tab = entry.second;
    if (!tab)
      continue;
    RefPtr<View> view = tab->view();
    if (!view)
      continue;
    auto url_u = view->url().utf8();
    const char *url = url_u.data();
    if (url && std::strstr(url, "downloads.html"))
    {
      view->EvaluateScript("(function(){ if(window.__ul_downloads_ready) window.__ul_downloads_ready(); })();", nullptr);
    }
  }

  if (downloads_overlay_ && downloads_overlay_->view())
  {
    downloads_overlay_->view()->EvaluateScript("(function(){ if(window.__ul_downloads_panel_refresh) window.__ul_downloads_panel_refresh(); })();", nullptr);
  }

  if (overlay_ && overlay_->view())
  {
    overlay_->view()->EvaluateScript("(function(){ if(window.__ul_update_downloads_badge) window.__ul_update_downloads_badge(); })();", nullptr);
  }
}

ultralight::JSValue UI::OnDownloadsOverlayGet(const JSObject &, const JSArgs &)
{
  return ultralight::JSValue(GetDownloadsJSON());
}

void UI::OnDownloadsOverlayClear(const JSObject &, const JSArgs &)
{
  ClearCompletedDownloads();
}

void UI::OnDownloadsOverlayOpenItem(const JSObject &, const JSArgs &args)
{
  if (args.empty())
    return;
  uint64_t id = static_cast<uint64_t>((double)args[0]);
  OpenDownloadItem(id);
}

void UI::OnDownloadsOverlayRevealItem(const JSObject &, const JSArgs &args)
{
  if (args.empty())
    return;
  uint64_t id = static_cast<uint64_t>((double)args[0]);
  RevealDownloadItem(id);
}

void UI::OnDownloadsOverlayPauseItem(const JSObject &, const JSArgs &args)
{
  if (args.empty())
    return;
  uint64_t id = static_cast<uint64_t>((double)args[0]);
  PauseDownloadItem(id);
}

void UI::OnDownloadsOverlayRemoveItem(const JSObject &, const JSArgs &args)
{
  if (args.empty())
    return;
  uint64_t id = static_cast<uint64_t>((double)args[0]);
  RemoveDownloadItem(id);
}

void UI::OnDownloadsOverlayToggle(const JSObject &, const JSArgs &)
{
  if (downloads_overlay_)
  {
    downloads_overlay_user_dismissed_ = true;
    HideDownloadsOverlay();
  }
  else
  {
    downloads_overlay_user_dismissed_ = false;
    ShowDownloadsOverlay();
  }
}

void UI::OnDownloadsOverlayClose(const JSObject &, const JSArgs &)
{
  downloads_overlay_user_dismissed_ = true;
  HideDownloadsOverlay();
}

void UI::ShowDownloadsOverlay()
{
  if (downloads_overlay_)
    return;

  downloads_overlay_user_dismissed_ = false;

  ultralight::ViewConfig cfg;
  cfg.is_transparent = true;
  cfg.initial_device_scale = window_->scale();
  if (overlay_ && overlay_->view())
  {
    cfg.is_accelerated = overlay_->view()->is_accelerated();
    cfg.display_id = overlay_->view()->display_id();
  }

  int overlay_top = ui_height_ + kDownloadsOverlaySpacing;
  if (overlay_top < 0)
    overlay_top = 0;
  uint32_t overlay_height = window_->height();
  if (overlay_height > (uint32_t)overlay_top)
    overlay_height -= static_cast<uint32_t>(overlay_top);
  else
    overlay_height = 1;

  auto view = App::instance()->renderer()->CreateView(window_->width(), overlay_height, cfg, nullptr);
  downloads_overlay_ = Overlay::Create(window_, view, 0, overlay_top);
  LayoutDownloadsOverlay();
  downloads_overlay_->Show();
  downloads_overlay_->Focus();
  view->set_load_listener(this);
  view->set_view_listener(this);
  view->LoadURL("file:///downloads-panel.html");
}

void UI::HideDownloadsOverlay()
{
  if (!downloads_overlay_)
    return;

  downloads_overlay_->Hide();
  downloads_overlay_->Unfocus();
  if (overlay_)
    overlay_->Focus();

  auto view = downloads_overlay_->view();
  if (view)
  {
    view->set_load_listener(nullptr);
    view->set_view_listener(nullptr);
  }

  downloads_overlay_ = nullptr;
}

void UI::LayoutDownloadsOverlay()
{
  if (!downloads_overlay_)
    return;

  int overlay_top = ui_height_ + kDownloadsOverlaySpacing;
  if (overlay_top < 0)
    overlay_top = 0;

  uint32_t overlay_width = window_->width();
  uint32_t overlay_height = window_->height();
  if (overlay_height > static_cast<uint32_t>(overlay_top))
    overlay_height -= static_cast<uint32_t>(overlay_top);
  else
    overlay_height = 1;

  downloads_overlay_->MoveTo(0, overlay_top);
  downloads_overlay_->Resize(overlay_width, overlay_height);

  if (auto view = downloads_overlay_->view())
    view->Resize(overlay_width, overlay_height);
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
  HandleSettingMutation("launch_dark_theme", !dark_mode_enabled_);
}

void UI::OnOpenSettingsPanel(const JSObject &, const JSArgs &)
{
  HideMenuOverlay();
  RefPtr<View> child = CreateNewTabForChildView(String("file:///settings.html"));
  if (child)
    child->LoadURL("file:///settings.html");
}

void UI::OnCloseSettingsPanel(const JSObject &, const JSArgs &)
{
  // Legacy no-op: settings now open in a dedicated tab.
}

ultralight::JSValue UI::OnGetSettings(const JSObject &, const JSArgs &)
{
  // Build a fresh snapshot of current settings state
  std::string payload = BuildSettingsPayload(false);

  // Debug: log the payload being returned (first 200 chars)
  if (payload.length() > 200)
  {
    std::string preview = payload.substr(0, 200) + "...";
    std::cout << "[UI::OnGetSettings] Returning payload: " << preview << std::endl;
  }
  else
  {
    std::cout << "[UI::OnGetSettings] Returning payload: " << payload << std::endl;
  }

  // Convert std::string to ultralight::String for proper JSValue conversion
  ultralight::String ul_payload(payload.c_str());
  return ultralight::JSValue(ul_payload);
}

void UI::OnUpdateSetting(const JSObject &, const JSArgs &args)
{
  if (args.size() < 2 || !args[0].IsString())
    return;

  ultralight::String key_ul = args[0].ToString();
  auto key_str = key_ul.utf8();
  std::string key = key_str.data() ? key_str.data() : "";
  if (key.empty())
    return;

  bool value = false;
  if (args[1].IsBoolean())
  {
    value = args[1].ToBoolean();
  }
  else if (args[1].IsNumber())
  {
    value = args[1].ToInteger() != 0;
  }
  else if (args[1].IsString())
  {
    ultralight::String val_ul = args[1].ToString();
    auto val_str = val_ul.utf8();
    std::string text = val_str.data() ? val_str.data() : "";
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    value = (text == "true" || text == "1" || text == "on" || text == "yes");
  }
  else
  {
    return;
  }

  HandleSettingMutation(key, value);
}

ultralight::JSValue UI::OnRestoreSettingsDefaults(const JSObject &, const JSArgs &)
{
  RestoreSettingsToDefaults();
  UpdateSettingsDirtyFlag();
  ApplySettings(false, false);
  UpdateSettingsDirtyFlag();
  std::string payload = BuildSettingsPayload(false);
  ultralight::String ul_payload(payload.c_str());
  return ultralight::JSValue(ul_payload);
}

void UI::OnSaveSettings(const JSObject &, const JSArgs &)
{
  bool saved = SaveSettingsToDisk();
  SyncSettingsStateToUI(saved);
}

ultralight::JSValue UI::OnGetDarkModeEnabled(const JSObject &obj, const JSArgs &args)
{
  return ultralight::JSValue(dark_mode_enabled_);
}

void UI::OnToggleAdblock(const JSObject &obj, const JSArgs &args)
{
  bool next_state = !(adblock_ ? adblock_->enabled() : adblock_enabled_cached_);
  HandleSettingMutation("enable_adblock", next_state);
}

ultralight::JSValue UI::OnGetAdblockEnabled(const JSObject &obj, const JSArgs &args)
{
  bool enabled = adblock_ ? adblock_->enabled() : adblock_enabled_cached_;
  adblock_enabled_cached_ = enabled;
  return ultralight::JSValue(enabled);
}

void UI::SyncAdblockStateToUI()
{
  if (adblock_)
    adblock_enabled_cached_ = adblock_->enabled();
  if (updateAdblockEnabled)
  {
    updateAdblockEnabled({adblock_enabled_cached_ ? 1.0 : 0.0});
  }
}

void UI::SyncSettingsStateToUI(bool snapshot_is_baseline)
{
  UpdateSettingsDirtyFlag();
  std::string json = BuildSettingsJSON();
  ultralight::String json_str(json.c_str());

  if (applySettings)
  {
    RefPtr<JSContext> lock(view()->LockJSContext());
    applySettings({json_str});
  }

  if (applySettingsPanel)
  {
    RefPtr<View> settings_view;
    for (auto &entry : tabs_)
    {
      if (!entry.second)
        continue;
      auto v = entry.second->view();
      if (!v)
        continue;
      auto url = v->url().utf8();
      if (url.data() && std::strstr(url.data(), "settings.html"))
      {
        settings_view = v;
        break;
      }
    }

    if (settings_view)
    {
      std::string payload = BuildSettingsPayload(snapshot_is_baseline);
      RefPtr<JSContext> lock(settings_view->LockJSContext());
      applySettingsPanel({String(payload.c_str())});
    }
    else
    {
      applySettingsPanel = JSFunction();
    }
  }
}

void UI::ApplySettings(bool initial, bool snapshot_is_baseline)
{
  // Appearance
  SetDarkModeEnabled(settings_.launch_dark_theme);
  vibrant_window_theme_enabled_ = settings_.vibrant_window_theme;
  experimental_transparent_toolbar_enabled_ = settings_.experimental_transparent_toolbar;

  // Handle compact tabs mode - adjust UI height and trigger resize
  bool was_compact = experimental_compact_tabs_enabled_;
  experimental_compact_tabs_enabled_ = settings_.experimental_compact_tabs;

  if (was_compact != experimental_compact_tabs_enabled_)
  {
    // Calculate new UI height based on compact mode
    double scale = window_ ? window_->scale() : 1.0;
    uint32_t target_height = experimental_compact_tabs_enabled_ ? (uint32_t)std::round(UI_HEIGHT_COMPACT * scale) : (uint32_t)std::round(UI_HEIGHT * scale);

    if (ui_height_ != target_height)
    {
      ui_height_ = target_height;
      // Trigger resize to reposition all tabs
      if (window_)
        OnResize(window_.get(), window_->width(), window_->height());
    }
  }

  // Privacy & Security
  if (adblock_)
  {
    adblock_->set_enabled(settings_.enable_adblock);
    adblock_->set_log_blocked(settings_.log_blocked_requests);
  }
  if (trackerblock_)
  {
    trackerblock_->set_enabled(settings_.enable_adblock);
    trackerblock_->set_log_blocked(settings_.log_blocked_requests);
  }
  adblock_enabled_cached_ = settings_.enable_adblock;
  clear_history_on_exit_ = settings_.clear_history_on_exit;

  // Note: JavaScript, web security, cookies, DNT would require View config changes
  // These settings are stored and can be applied on next tab creation

  // Address Bar & Suggestions
  suggestions_enabled_ = settings_.enable_suggestions;
  suggestion_favicons_enabled_ = settings_.enable_suggestion_favicons;
  if (!suggestions_enabled_)
    HideSuggestionsOverlay();

  // Downloads
  show_download_badge_ = settings_.show_download_badge;
  auto_open_download_panel_ = settings_.auto_open_download_panel;
  // ask_download_location would be checked when download starts

  // Performance
  // smooth_scrolling, hardware_acceleration, local_storage, database
  // These would typically be applied during View/Config creation

  // Accessibility
  reduce_motion_enabled_ = settings_.reduce_motion;
  high_contrast_ui_enabled_ = settings_.high_contrast_ui;
  // enable_caret_browsing would require page-level script injection

  // Developer
  // enable_remote_inspector, show_performance_overlay
  // These would require additional implementation

  SyncAdblockStateToUI();
  UpdateSettingsDirtyFlag();
  SyncSettingsStateToUI(snapshot_is_baseline);
}

void UI::SetDarkModeEnabled(bool enabled)
{
  if (dark_mode_enabled_ == enabled)
  {
    settings_.launch_dark_theme = enabled;
    return;
  }

  dark_mode_enabled_ = enabled;
  settings_.launch_dark_theme = enabled;

  auto apply_to = [&](RefPtr<View> v)
  {
    if (!v)
      return;
    if (enabled)
      ApplyDarkModeToView(v);
    else
      RemoveDarkModeFromView(v);
  };

  apply_to(view());
  for (auto &entry : tabs_)
  {
    if (entry.second)
      apply_to(entry.second->view());
  }
  if (menu_overlay_)
    apply_to(menu_overlay_->view());
  if (downloads_overlay_)
    apply_to(downloads_overlay_->view());
  if (context_menu_overlay_)
    apply_to(context_menu_overlay_->view());
  if (suggestions_overlay_)
    apply_to(suggestions_overlay_->view());
}

void UI::EnsureDataDirectoryExists()
{
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::create_directories(LegacySettingsFilePath().parent_path(), ec);
  fs::create_directories(SettingsDirectory(), ec);
}

void UI::RestoreSettingsToDefaults()
{
  settings_ = BrowserSettings();
  const auto &catalog = GetSettingsCatalog();
  for (const auto &entry : catalog)
  {
    if (!entry.member)
      continue;
    settings_.*(entry.member) = entry.default_value;
  }
}

bool UI::ParseSettingsBool(const std::string &buffer, const char *key, bool fallback) const
{
  if (!key)
    return fallback;
  std::string needle = std::string("\"") + key + "\"";
  size_t pos = buffer.find(needle);
  if (pos == std::string::npos)
    return fallback;
  pos = buffer.find(':', pos);
  if (pos == std::string::npos)
    return fallback;
  ++pos;
  while (pos < buffer.size() && std::isspace(static_cast<unsigned char>(buffer[pos])))
    ++pos;
  if (pos >= buffer.size())
    return fallback;
  if (buffer.compare(pos, 4, "true") == 0)
    return true;
  if (buffer.compare(pos, 5, "false") == 0)
    return false;
  if (buffer[pos] == '1')
    return true;
  if (buffer[pos] == '0')
    return false;
  return fallback;
}

void UI::LoadSettingsFromDisk()
{
  RestoreSettingsToDefaults();

  auto apply_from_stream = [&](std::istream &stream)
  {
    std::ostringstream ss;
    ss << stream.rdbuf();
    std::string content = ss.str();

    const auto &catalog = GetSettingsCatalog();
    for (const auto &desc : catalog)
    {
      if (!desc.member)
        continue;
      bool fallback = settings_.*(desc.member);
      settings_.*(desc.member) = ParseSettingsBool(content, desc.key.c_str(), fallback);
    }
  };

  bool loaded_primary = false;
  bool migrated_from_legacy = false;

  {
    std::ifstream in(SettingsFilePath(), std::ios::in | std::ios::binary);
    if (in.is_open())
    {
      apply_from_stream(in);
      in.close();
      loaded_primary = true;
      settings_storage_path_ = SettingsFilePath().string();
    }
  }

  if (!loaded_primary)
  {
    std::ifstream legacy(LegacySettingsFilePath(), std::ios::in | std::ios::binary);
    if (legacy.is_open())
    {
      apply_from_stream(legacy);
      legacy.close();
      migrated_from_legacy = true;
      settings_storage_path_ = LegacySettingsFilePath().string();
    }
  }

  if (!loaded_primary && !migrated_from_legacy)
  {
    settings_storage_path_ = SettingsFilePath().string();
  }

  saved_settings_ = settings_;
  settings_dirty_ = false;

  if (migrated_from_legacy)
  {
    // Persist immediately to the new storage location so future loads use it.
    SaveSettingsToDisk();
  }
}

bool UI::SaveSettingsToDisk()
{
  EnsureDataDirectoryExists();
  const auto &catalog = GetSettingsCatalog();
  const std::string timestamp = ToIso8601UTC(std::chrono::system_clock::now());

  const std::string values_json = BuildSettingsJSON();

  auto build_document_for_path = [&](const std::string &path_str) -> std::string
  {
    std::ostringstream doc;
    doc << "{\n";
    doc << "  \"values\": " << values_json << ",\n";
    doc << "  \"meta\": {\n";
    doc << "    \"updated_at\": \"" << timestamp << "\",\n";
    doc << "    \"dirty\": false,\n";
    doc << "    \"storage_path\": \"" << EscapeJson(path_str) << "\",\n";
    doc << "    \"settings\": [\n";

    bool first_entry = true;
    for (const auto &desc : catalog)
    {
      if (!desc.member)
        continue;
      bool current = settings_.*(desc.member);
      bool default_value = desc.default_value;

      if (!first_entry)
        doc << ",\n";

      doc << "      {\"key\":\"" << EscapeJson(desc.key) << "\",";
      doc << "\"name\":\"" << EscapeJson(desc.name) << "\",";
      doc << "\"description\":\"" << EscapeJson(desc.description) << "\",";
      doc << "\"category\":\"" << EscapeJson(desc.category) << "\",";
      doc << "\"value\":" << (current ? "true" : "false") << ",";
      doc << "\"default\":" << (default_value ? "true" : "false");
      if (!desc.note.empty())
        doc << ",\"note\":\"" << EscapeJson(desc.note) << "\"";
      doc << "}";
      first_entry = false;
    }

    if (!first_entry)
      doc << "\n";

    doc << "    ]\n";
    doc << "  }\n";
    doc << "}\n";
    return doc.str();
  };

  auto write_payload = [](const std::filesystem::path &path, const std::string &payload) -> bool
  {
    if (path.empty())
      return false;
    std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.is_open())
      return false;
    out << payload;
    out.flush();
    bool ok = out.good();
    out.close();
    return ok;
  };

  const std::filesystem::path primary_path = SettingsFilePath();
  const std::filesystem::path fallback_path = LegacySettingsFilePath();
  const std::string primary_path_str = primary_path.string();
  const std::string fallback_path_str = fallback_path.string();

  std::string primary_payload = build_document_for_path(primary_path_str);
  bool ok = write_payload(primary_path, primary_payload);

  if (ok)
  {
    settings_storage_path_ = primary_path_str;
  }
  else
  {
    std::error_code ec;
    if (!fallback_path.empty())
      std::filesystem::create_directories(fallback_path.parent_path(), ec);
    std::string fallback_payload = build_document_for_path(fallback_path_str);
    ok = write_payload(fallback_path, fallback_payload);
    if (ok)
      settings_storage_path_ = fallback_path_str;
  }

  if (ok)
  {
    saved_settings_ = settings_;
    settings_dirty_ = false;

    if (settings_storage_path_ == primary_path_str && !fallback_path.empty() && fallback_path != primary_path)
    {
      std::error_code ec;
      std::filesystem::create_directories(fallback_path.parent_path(), ec);
      std::string mirror_payload = build_document_for_path(fallback_path_str);
      write_payload(fallback_path, mirror_payload);
    }
  }

  return ok;
}

std::string UI::BuildSettingsJSON() const
{
  // Debug: log what we're about to serialize
  std::cout << "[BuildSettingsJSON] settings_ values:" << std::endl;
  std::cout << "  launch_dark_theme: " << settings_.launch_dark_theme << std::endl;
  std::cout << "  enable_adblock: " << settings_.enable_adblock << std::endl;
  std::cout << "  enable_suggestions: " << settings_.enable_suggestions << std::endl;

  // Directly serialize all settings_ struct members
  std::ostringstream ss;
  ss << "{";
  // Appearance
  ss << "\"launch_dark_theme\":" << (settings_.launch_dark_theme ? "true" : "false") << ",";
  ss << "\"vibrant_window_theme\":" << (settings_.vibrant_window_theme ? "true" : "false") << ",";
  ss << "\"experimental_transparent_toolbar\":" << (settings_.experimental_transparent_toolbar ? "true" : "false") << ",";
  ss << "\"experimental_compact_tabs\":" << (settings_.experimental_compact_tabs ? "true" : "false") << ",";
  // Privacy & Security
  ss << "\"enable_adblock\":" << (settings_.enable_adblock ? "true" : "false") << ",";
  ss << "\"log_blocked_requests\":" << (settings_.log_blocked_requests ? "true" : "false") << ",";
  ss << "\"clear_history_on_exit\":" << (settings_.clear_history_on_exit ? "true" : "false") << ",";
  ss << "\"enable_javascript\":" << (settings_.enable_javascript ? "true" : "false") << ",";
  ss << "\"enable_web_security\":" << (settings_.enable_web_security ? "true" : "false") << ",";
  ss << "\"block_third_party_cookies\":" << (settings_.block_third_party_cookies ? "true" : "false") << ",";
  ss << "\"do_not_track\":" << (settings_.do_not_track ? "true" : "false") << ",";
  // Address Bar & Suggestions
  ss << "\"enable_suggestions\":" << (settings_.enable_suggestions ? "true" : "false") << ",";
  ss << "\"enable_suggestion_favicons\":" << (settings_.enable_suggestion_favicons ? "true" : "false") << ",";
  // Downloads
  ss << "\"show_download_badge\":" << (settings_.show_download_badge ? "true" : "false") << ",";
  ss << "\"auto_open_download_panel\":" << (settings_.auto_open_download_panel ? "true" : "false") << ",";
  ss << "\"ask_download_location\":" << (settings_.ask_download_location ? "true" : "false") << ",";
  // Performance
  ss << "\"smooth_scrolling\":" << (settings_.smooth_scrolling ? "true" : "false") << ",";
  ss << "\"hardware_acceleration\":" << (settings_.hardware_acceleration ? "true" : "false") << ",";
  ss << "\"enable_local_storage\":" << (settings_.enable_local_storage ? "true" : "false") << ",";
  ss << "\"enable_database\":" << (settings_.enable_database ? "true" : "false") << ",";
  // Accessibility
  ss << "\"reduce_motion\":" << (settings_.reduce_motion ? "true" : "false") << ",";
  ss << "\"high_contrast_ui\":" << (settings_.high_contrast_ui ? "true" : "false") << ",";
  ss << "\"enable_caret_browsing\":" << (settings_.enable_caret_browsing ? "true" : "false") << ",";
  // Developer
  ss << "\"enable_remote_inspector\":" << (settings_.enable_remote_inspector ? "true" : "false") << ",";
  ss << "\"show_performance_overlay\":" << (settings_.show_performance_overlay ? "true" : "false");
  ss << "}";

  std::string result = ss.str();
  std::cout << "[BuildSettingsJSON] Result: " << result << std::endl;
  return result;
}

std::string UI::BuildSettingsPayload(bool snapshot_is_baseline) const
{
  std::string values_json = BuildSettingsJSON();
  const auto &catalog = GetSettingsCatalog();
  std::vector<const char *> dirty_keys;
  dirty_keys.reserve(catalog.size());

  std::string storage_path = settings_storage_path_.empty() ? SettingsFilePath().string() : settings_storage_path_;

  for (const auto &desc : catalog)
  {
    if (!desc.member)
      continue;
    bool current = settings_.*(desc.member);
    bool saved = saved_settings_.*(desc.member);
    if (current != saved)
      dirty_keys.push_back(desc.key.c_str());
  }

  std::ostringstream ss;
  ss << "{";
  ss << "\"values\":" << values_json << ",";
  ss << "\"meta\":{";
  ss << "\"dirty\":" << (settings_dirty_ ? "true" : "false") << ",";
  ss << "\"baseline\":" << (snapshot_is_baseline ? "true" : "false") << ",";
  ss << "\"storage_path\":\"" << EscapeJson(storage_path) << "\",";
  ss << "\"dirty_keys\":[";
  for (size_t i = 0; i < dirty_keys.size(); ++i)
  {
    if (i > 0)
      ss << ",";
    ss << "\"" << dirty_keys[i] << "\"";
  }
  ss << "],";
  ss << "\"catalog\":[";
  bool first = true;
  for (const auto &desc : catalog)
  {
    if (!desc.member)
      continue;
    if (!first)
      ss << ",";
    bool current = settings_.*(desc.member);
    bool default_value = desc.default_value;
    bool saved_value = saved_settings_.*(desc.member);
    ss << "{";
    ss << "\"key\":\"" << EscapeJson(desc.key) << "\",";
    ss << "\"name\":\"" << EscapeJson(desc.name) << "\",";
    ss << "\"description\":\"" << EscapeJson(desc.description) << "\",";
    ss << "\"category\":\"" << EscapeJson(desc.category) << "\",";
    ss << "\"value\":" << (current ? "true" : "false") << ",";
    ss << "\"default\":" << (default_value ? "true" : "false") << ",";
    ss << "\"saved\":" << (saved_value ? "true" : "false");
    if (!desc.note.empty())
      ss << ",\"note\":\"" << EscapeJson(desc.note) << "\"";
    ss << "}";
    first = false;
  }
  ss << "]";
  ss << "}";
  ss << "}";
  return ss.str();
}

void UI::UpdateSettingsDirtyFlag()
{
  settings_dirty_ = (settings_ != saved_settings_);
}

void UI::HandleSettingMutation(const std::string &key, bool value)
{
  const auto *descriptor = FindSettingDescriptor(key);
  if (!descriptor || !descriptor->member)
    return;

  bool &field = settings_.*(descriptor->member);
  if (field == value)
    return;

  field = value;
  UpdateSettingsDirtyFlag();
  ApplySettings(false, false);
  UpdateSettingsDirtyFlag();

  if (key == "clear_history_on_exit")
  {
    if (value)
      std::remove("data/history.json");
    else
      SaveHistoryToDisk();
  }
}

void UI::AdjustUIHeight(uint32_t new_height)
{
  if (new_height == (uint32_t)ui_height_)
    return;

  ui_height_ = (int)new_height;

  // Resize top UI overlay
  overlay_->Resize(window_->width(), ui_height_);

  // Note: Do NOT move or resize tabs here; we only enlarge the UI overlay canvas.
  if (downloads_overlay_)
    LayoutDownloadsOverlay();
}

void UI::OnSuggestOpen(const JSObject &obj, const JSArgs &args)
{
  // No-op: suggestions are shown in a top overlay; do not move layout.
}

void UI::OnSuggestClose(const JSObject &obj, const JSArgs &args)
{
  // No-op.
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
          // Prefer storing settings next to the executable under a "setup" folder
          auto exe_dir = []() -> std::filesystem::path {
        #if defined(_WIN32)
            wchar_t buf[MAX_PATH];
            DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
            if (n > 0 && n < MAX_PATH) {
              std::filesystem::path p(buf);
              return p.parent_path();
            }
            return std::filesystem::current_path();
        #elif defined(__APPLE__)
            // Fallback: current_path (resolving actual executable dir on macOS requires _NSGetExecutablePath)
            return std::filesystem::current_path();
        #else
            // Linux/Unix: try /proc/self/exe, else current_path
            char linkpath[4096];
            ssize_t len = readlink("/proc/self/exe", linkpath, sizeof(linkpath)-1);
            if (len > 0) {
              linkpath[len] = '\0';
              std::filesystem::path p(linkpath);
              return p.parent_path();
            }
            return std::filesystem::current_path();
        #endif
          }();
          return std::filesystem::absolute(exe_dir / "setup");
          try { selText = String(window.getSelection ? window.getSelection() : ''); } catch(_){ }
          if (!selText) return false;
          try { if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(selText); } catch(_){ }
          // Delete selection
          return SettingsDirectory() / "settings.json";
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
      var prev=document.getElementById(sid);
      if(prev) prev.remove();

      // Check if page is already dark or very bright
      function getPageBrightness(){
        var bgColor = window.getComputedStyle(document.body).backgroundColor;
        if(!bgColor || bgColor === 'transparent' || bgColor === 'rgba(0, 0, 0, 0)'){
          bgColor = window.getComputedStyle(document.documentElement).backgroundColor;
        }
        if(!bgColor || bgColor === 'transparent' || bgColor === 'rgba(0, 0, 0, 0)'){
          return 1.0; // Assume bright if we can't detect
        }
        var m = bgColor.match(/rgba?\(([^)]+)\)/);
        if(!m) return 1.0;
        var parts = m[1].split(',').map(function(x){ return parseFloat(x); });
        var r = parts[0]/255, g = parts[1]/255, b = parts[2]/255;
        // Calculate relative luminance
        function srgb(c){ return (c<=0.03928)? c/12.92 : Math.pow((c+0.055)/1.055, 2.4); }
        return 0.2126*srgb(r) + 0.7152*srgb(g) + 0.0722*srgb(b);
      }

      var brightness = getPageBrightness();

      // Only apply dark mode to bright pages (luminance > 0.5)
      if(brightness < 0.5){
        // Page is already dark, don't apply filter
        return false;
      }

      // Apply filter-based dark mode for bright pages
      var css = 'html { filter: invert(0.9) hue-rotate(180deg) !important; background: #1e1e1e !important; }\n';
      css += 'img, video, [style*="background-image"], picture, svg, iframe { filter: invert(1.11) hue-rotate(-180deg) !important; }';

      var s=document.createElement('style');
      s.id=sid;
      s.type='text/css';
      s.appendChild(document.createTextNode(css));
      (document.head||document.documentElement).appendChild(s);

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
        while (num_start < obj.size() && (obj[num_start] == ' ' || obj[num_start] == '\t'))
          num_start++;
        size_t num_end = num_start;
        while (num_end < obj.size() && std::isdigit(obj[num_end]))
          num_end++;
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
  if (clear_history_on_exit_)
  {
    std::remove("data/history.json");
    return;
  }

  EnsureDataDirectoryExists();
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
  struct Scored
  {
    std::string url;
    double score;
  };
  std::vector<Scored> scored;
  auto push_scored = [&](const std::string &u, double s)
  { scored.push_back({u, s}); };

  auto now_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();

  auto score_history = [&](const std::string &u, const HistoryEntry &e, const std::string &q) -> double
  {
    std::string ul = u, ql = q;
    std::transform(ul.begin(), ul.end(), ul.begin(), ::tolower);
    std::transform(ql.begin(), ql.end(), ql.begin(), ::tolower);
    double prefix = 0.0, contains = 0.0;
    // check domain part prefix
    size_t proto = ul.find("://");
    size_t start = (proto == std::string::npos ? 0 : proto + 3);
    if (ul.find(ql, start) == start)
      prefix = 1.0; // strong boost for prefix
    if (ul.find(ql) != std::string::npos)
      contains = 0.5;
    // recency: within ~30 days fades to 0
    double age_days = std::max<double>(0.0, (double)(now_ms - e.timestamp_ms) / (1000.0 * 60.0 * 60.0 * 24.0));
    double recency = std::max(0.0, 1.0 - (age_days / 30.0));
    // frequency: log scale
    double freq = std::log(1.0 + (double)std::max<uint32_t>(1, e.visit_count)) / std::log(10.0);
    return 2.0 * prefix + 1.0 * contains + 2.0 * recency + 2.0 * freq;
  };

  // If no input, suggest top by recency/frequency
  if (input.empty())
  {
    std::unordered_set<std::string> seen;
    // History candidates
    for (const auto &e : history_)
    {
      double s = score_history(e.url, e, std::string());
      if (seen.insert(e.url).second)
        push_scored(e.url, s);
    }
    // Sort by score desc
    std::sort(scored.begin(), scored.end(), [](const Scored &a, const Scored &b)
              { return a.score > b.score; });
    for (const auto &p : scored)
    {
      if ((int)suggestions.size() >= maxResults)
        break;
      suggestions.push_back(p.url);
    }
    // Fill with popular sites
    for (const auto &site : popular_sites_)
    {
      if ((int)suggestions.size() >= maxResults)
        break;
      if (std::find(suggestions.begin(), suggestions.end(), site) == suggestions.end())
        suggestions.push_back(site);
    }
    return suggestions;
  }

  std::string input_lower = input;
  std::transform(input_lower.begin(), input_lower.end(), input_lower.begin(), ::tolower);

  // Helper lambda to check if a URL matches the input
  auto matches = [&input_lower](const std::string &url) -> bool
  {
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
        if (seen.insert(e.url).second)
          push_scored(e.url, s);
      }
    }
  }

  // Then, add matching popular sites (lower score)
  for (const auto &site : popular_sites_)
  {
    if (matches(site))
    {
      // Low baseline score; small prefix boost
      double s = 0.6;
      std::string sl = site, il = input;
      std::transform(sl.begin(), sl.end(), sl.begin(), ::tolower);
      std::transform(il.begin(), il.end(), il.begin(), ::tolower);
      if (sl.find(il) == sl.find("://") + 3)
        s += 0.6;
      push_scored(site, s);
    }
  }

  // Sort by score and emit unique URLs
  std::sort(scored.begin(), scored.end(), [](const Scored &a, const Scored &b)
            { return a.score > b.score; });
  std::unordered_set<std::string> emitted;
  for (const auto &p : scored)
  {
    if ((int)suggestions.size() >= maxResults)
      break;
    if (emitted.insert(p.url).second)
      suggestions.push_back(p.url);
  }
  return suggestions;
}

JSValue UI::OnGetSuggestions(const JSObject &obj, const JSArgs &args)
{
  if (args.size() < 1 || !args[0].IsString())
    return JSValue();

  if (!suggestions_enabled_)
    return JSValue("[]");

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
    if (i > 0)
      json += ",";
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
      if (!f.empty())
        json += ",\"favicon\":\"" + jsonEscape(f) + "\"";
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
  if (scheme == std::string::npos)
    return std::string();
  size_t host_start = scheme + 3;
  size_t slash = url.find('/', host_start);
  if (slash == std::string::npos)
    return url; // whole string is origin
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
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      62,
      -1,
      -1,
      -1,
      63,
      52,
      53,
      54,
      55,
      56,
      57,
      58,
      59,
      60,
      61,
      -1,
      -1,
      -1,
      0,
      -1,
      -1,
      -1,
      0,
      1,
      2,
      3,
      4,
      5,
      6,
      7,
      8,
      9,
      10,
      11,
      12,
      13,
      14,
      15,
      16,
      17,
      18,
      19,
      20,
      21,
      22,
      23,
      24,
      25,
      -1,
      -1,
      -1,
      -1,
      -1,
      -1,
      26,
      27,
      28,
      29,
      30,
      31,
      32,
      33,
      34,
      35,
      36,
      37,
      38,
      39,
      40,
      41,
      42,
      43,
      44,
      45,
      46,
      47,
      48,
      49,
      50,
      51,
      -1,
      -1,
      -1,
      -1,
      -1,
      // rest -1
  };
  std::string out;
  out.reserve(in.size() * 3 / 4);
  int val = 0, valb = -8;
  for (unsigned char c : in)
  {
    int d = -1;
    if (c < 128)
      d = T[c];
    if (d == -1)
    {
      if (c == '=')
        break;
      else
        continue;
    }
    val = (val << 6) + d;
    valb += 6;
    if (valb >= 0)
    {
      out.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

void UI::LoadFaviconDiskCache()
{
  favicon_file_cache_.clear();
  // Ensure directory exists
  EnsureFaviconCacheDir();
  std::ifstream in("data/favicons/index.json", std::ios::in | std::ios::binary);
  if (!in.is_open())
    return;
  std::ostringstream ss;
  ss << in.rdbuf();
  std::string txt = ss.str();
  in.close();
  // Parse simple object { "origin": "file:///...", ... }
  size_t pos = 0;
  while (true)
  {
    size_t k1 = txt.find('"', pos);
    if (k1 == std::string::npos)
      break;
    size_t k2 = txt.find('"', k1 + 1);
    if (k2 == std::string::npos)
      break;
    std::string key = txt.substr(k1 + 1, k2 - k1 - 1);
    size_t c = txt.find(':', k2 + 1);
    if (c == std::string::npos)
      break;
    size_t v1 = txt.find('"', c + 1);
    if (v1 == std::string::npos)
      break;
    size_t v2 = txt.find('"', v1 + 1);
    if (v2 == std::string::npos)
      break;
    std::string val = txt.substr(v1 + 1, v2 - v1 - 1);
    if (!key.empty() && !val.empty())
      favicon_file_cache_[key] = val;
    pos = v2 + 1;
    if (favicon_file_cache_.size() > favicon_cache_limit_)
      break;
  }
  if (favicon_file_cache_.size() > favicon_cache_limit_)
  {
    PruneFaviconDiskCacheToLimit();
    SaveFaviconDiskCache();
  }
}

void UI::SaveFaviconDiskCache()
{
  EnsureFaviconCacheDir();
  std::ofstream out("data/favicons/index.json", std::ios::out | std::ios::binary | std::ios::trunc);
  if (!out.is_open())
    return;
  out << "{";
  bool first = true;
  for (auto &p : favicon_file_cache_)
  {
    if (!first)
      out << ",";
    first = false;
    out << "\"" << jsonEscape(p.first) << "\":\"" << jsonEscape(p.second) << "\"";
  }
  out << "}";
  out.close();
}

void UI::OnFaviconReady(const JSObject &obj, const JSArgs &args)
{
  // args: url, dataUrl (data:image/png;base64,....)
  if (args.size() < 2 || !args[0].IsString() || !args[1].IsString())
    return;
  if (!suggestion_favicons_enabled_)
    return;
  ultralight::String url_ul = args[0].ToString();
  ultralight::String data_ul = args[1].ToString();
  auto u8 = url_ul.utf8();
  std::string url = u8.data() ? u8.data() : "";
  auto d8 = data_ul.utf8();
  std::string data = d8.data() ? d8.data() : "";
  if (url.empty() || data.empty())
    return;
  std::string origin = GetOriginStringFromURL(url);
  if (origin.empty())
    return;

  // Respect cache size limit with smarter eviction based on origin score
  double new_score = GetOriginScore(origin);
  if (favicon_file_cache_.find(origin) == favicon_file_cache_.end() && favicon_file_cache_.size() >= favicon_cache_limit_)
  {
    // Find the lowest-score existing origin
    std::string worst_origin;
    double worst_score = 1e18;
    bool found = false;
    for (const auto &p : favicon_file_cache_)
    {
      double s = GetOriginScore(p.first);
      if (!found || s < worst_score)
      {
        worst_score = s;
        worst_origin = p.first;
        found = true;
      }
    }
    if (!found || new_score <= worst_score)
    {
      // New origin is not better than the worst cached one; skip caching
      return;
    }
    // Evict worst
    auto itw = favicon_file_cache_.find(worst_origin);
    if (itw != favicon_file_cache_.end())
    {
      // Try delete file on disk
      std::string furl = itw->second;
      std::string path;
      const std::string prefix = "file:///";
      if (furl.rfind(prefix, 0) == 0)
      {
        path = furl.substr(prefix.size());
#ifdef _WIN32
        for (auto &ch : path)
        {
          if (ch == '/')
            ch = '\\';
        }
#endif
        std::remove(path.c_str());
      }
      favicon_file_cache_.erase(itw);
    }
  }

  // Expect data URL like data:image/png;base64,....
  size_t comma = data.find(",");
  if (comma == std::string::npos)
    return;
  std::string b64 = data.substr(comma + 1);
  std::string bytes = Base64Decode(b64);
  if (bytes.size() < 8)
    return;

  std::string dir = EnsureFaviconCacheDir();
  // Hash filename from origin
  std::hash<std::string> hasher;
  size_t h = hasher(origin);
  std::ostringstream path;
  path << dir << "/" << std::hex << h << ".png";
  std::string file = path.str();
  // Write file
  std::ofstream f(file, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!f.is_open())
    return;
  f.write(bytes.data(), (std::streamsize)bytes.size());
  f.close();
  // Store as file:/// absolute URL
  char cwd_buf[1024] = {0};
#ifdef _WIN32
  _getcwd(cwd_buf, sizeof(cwd_buf));
  std::string abs = std::string("file:///") + std::string(cwd_buf) + "/" + file;
  // replace backslashes
  for (auto &ch : abs)
  {
    if (ch == '\\')
      ch = '/';
  }
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
  if (origin.empty())
    return 0.0;
  auto now_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  double total = 0.0;
  for (const auto &e : history_)
  {
    if (GetOriginStringFromURL(e.url) != origin)
      continue;
    double age_days = std::max<double>(0.0, (double)(now_ms - e.timestamp_ms) / (1000.0 * 60.0 * 60.0 * 24.0));
    double recency = std::max(0.0, 1.0 - (age_days / 30.0));
    double freq = std::log(1.0 + (double)std::max<uint32_t>(1, e.visit_count)) / std::log(10.0);
    total += (2.0 * recency + 2.0 * freq);
  }
  return total;
}

void UI::PruneFaviconDiskCacheToLimit()
{
  if (favicon_file_cache_.size() <= favicon_cache_limit_)
    return;
  // Collect and sort by score desc
  std::vector<std::pair<std::string, double>> arr;
  arr.reserve(favicon_file_cache_.size());
  for (const auto &p : favicon_file_cache_)
  {
    arr.emplace_back(p.first, GetOriginScore(p.first));
  }
  std::sort(arr.begin(), arr.end(), [](const auto &a, const auto &b)
            { return a.second > b.second; });
  // Determine origins to keep
  std::unordered_set<std::string> keep;
  for (size_t i = 0; i < arr.size() && i < favicon_cache_limit_; ++i)
    keep.insert(arr[i].first);
  // Remove the rest (and try delete files)
  for (auto it = favicon_file_cache_.begin(); it != favicon_file_cache_.end();)
  {
    if (keep.find(it->first) != keep.end())
    {
      ++it;
      continue;
    }
    std::string furl = it->second;
    const std::string prefix = "file:///";
    if (furl.rfind(prefix, 0) == 0)
    {
      std::string path = furl.substr(prefix.size());
#ifdef _WIN32
      for (auto &ch : path)
      {
        if (ch == '/')
          ch = '\\';
      }
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
  if (!in.is_open())
    return;
  std::ostringstream ss;
  ss << in.rdbuf();
  std::string txt = ss.str();
  in.close();
  // Normalize
  std::string s;
  s.reserve(txt.size());
  for (char c : txt)
  {
    if (!std::isspace((unsigned char)c))
      s += (char)std::tolower((unsigned char)c);
  }
  if (s == "off" || s == "0" || s == "false")
    suggestion_favicons_enabled_ = false;
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
  if (url.empty())
    return;
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
    if (child)
      child->LoadURL(s);
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

void UI::OnNewDownloadStarted()
{
  if (!auto_open_download_panel_)
    return;

  downloads_overlay_user_dismissed_ = false;
  ShowDownloadsOverlay();
}

bool UI::BrowserSettings::operator==(const BrowserSettings &other) const
{
  return launch_dark_theme == other.launch_dark_theme &&
         vibrant_window_theme == other.vibrant_window_theme &&
         experimental_transparent_toolbar == other.experimental_transparent_toolbar &&
         experimental_compact_tabs == other.experimental_compact_tabs &&
         enable_adblock == other.enable_adblock &&
         log_blocked_requests == other.log_blocked_requests &&
         clear_history_on_exit == other.clear_history_on_exit &&
         enable_javascript == other.enable_javascript &&
         enable_web_security == other.enable_web_security &&
         block_third_party_cookies == other.block_third_party_cookies &&
         do_not_track == other.do_not_track &&
         enable_suggestions == other.enable_suggestions &&
         enable_suggestion_favicons == other.enable_suggestion_favicons &&
         show_download_badge == other.show_download_badge &&
         auto_open_download_panel == other.auto_open_download_panel &&
         ask_download_location == other.ask_download_location &&
         smooth_scrolling == other.smooth_scrolling &&
         hardware_acceleration == other.hardware_acceleration &&
         enable_local_storage == other.enable_local_storage &&
         enable_database == other.enable_database &&
         reduce_motion == other.reduce_motion &&
         high_contrast_ui == other.high_contrast_ui &&
         enable_caret_browsing == other.enable_caret_browsing &&
         enable_remote_inspector == other.enable_remote_inspector &&
         show_performance_overlay == other.show_performance_overlay;
}

std::filesystem::path UI::SettingsDirectory()
{
  namespace fs = std::filesystem;
#if defined(_WIN32)
  if (const char *appdata = std::getenv("APPDATA"); appdata && *appdata)
    return fs::path(appdata) / "UltralightWebBrowser";
#elif defined(__APPLE__)
  if (const char *home = std::getenv("HOME"); home && *home)
    return fs::path(home) / "Library/Application Support/UltralightWebBrowser";
#else
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
    return fs::path(xdg) / "UltralightWebBrowser";
  if (const char *home = std::getenv("HOME"); home && *home)
    return fs::path(home) / ".config/UltralightWebBrowser";
#endif
  return fs::current_path() / "data";
}

std::filesystem::path UI::SettingsFilePath()
{
  return SettingsDirectory() / "settings.json";
}

std::filesystem::path UI::LegacySettingsFilePath()
{
  namespace fs = std::filesystem;
  return fs::path("data") / "settings.json";
}
