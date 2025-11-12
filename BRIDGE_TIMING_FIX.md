# Settings Fix - Bridge Timing Issue

## Problem Identified

The error `GetSettingsSnapshot not available, bridge not ready` occurs because:

1. **Bridge binding scope issue**: The settings bridge functions (`GetSettingsSnapshot`, `OnUpdateSetting`, etc.) were being bound in `OnDOMReady()` for the UI overlay view
2. **Settings page in different view**: When settings.html opens in a new tab, it runs in a **different JavaScript context** (different View object)
3. **Bridge not available**: The settings page's JavaScript context doesn't have the bridge functions bound to it

## The Fix

### 1. Bind Settings Bridge Functions to ALL Views

**Changed in:** `src/UI.cpp` - `OnDOMReady()` function

**Before:**
```cpp
// These were only bound once to the main UI overlay
global["GetSettingsSnapshot"] = BindJSCallbackWithRetval(&UI::OnGetSettings);
global["OnUpdateSetting"] = BindJSCallback(&UI::OnUpdateSetting);
// etc...
```

**After:**
```cpp
// Bind settings bridge functions to ALL views (not just UI overlay)
// This ensures settings.html can call GetSettingsSnapshot when loaded in a tab
global["GetSettingsSnapshot"] = BindJSCallbackWithRetval(&UI::OnGetSettings);
global["OnUpdateSetting"] = BindJSCallback(&UI::OnUpdateSetting);
global["OnRestoreSettingsDefaults"] = BindJSCallbackWithRetval(&UI::OnRestoreSettingsDefaults);
global["OnSaveSettings"] = BindJSCallback(&UI::OnSaveSettings);
```

**Why this works:**
- `OnDOMReady()` is called for **every view** that loads
- By binding settings functions in the common section (before the `if (is_ctx_view)` checks), they get bound to ALL views
- Now when settings.html loads in a tab, its JavaScript context has the bridge functions available

### 2. Removed Quick Settings from Menu

**Changed in:** `assets/menu.html`

**Removed:**
- Quick settings heading
- Dark theme toggle
- Ad blocking toggle
- Address suggestions toggle
- All related JavaScript code (`getSettingsValues()`, `setToggleState()`, `refreshQuickSettings()`)

**Why:**
- Consolidates all settings management to the dedicated Settings page
- Eliminates redundant UI that could conflict with Settings page
- Simplifies menu and reduces code complexity

## Updated Flow

### Settings Page Load (BEFORE FIX):

```
User Opens Settings
  └─> Settings.html loads in new tab
  └─> Tab has its own JavaScript context (View)
  └─> OnDOMReady() called for this view
      ├─> is_settings_page_view = true
      ├─> But GetSettingsSnapshot was only bound to UI overlay!
      └─> Settings page JavaScript context doesn't have bridge
  └─> JavaScript calls GetSettingsSnapshot()
      └─> ❌ ERROR: "GetSettingsSnapshot not available, bridge not ready"
```

### Settings Page Load (AFTER FIX):

```
User Opens Settings
  └─> Settings.html loads in new tab
  └─> Tab has its own JavaScript context (View)
  └─> OnDOMReady() called for this view
      ├─> Binds bridge functions to THIS view's context:
      │   ├─> GetSettingsSnapshot ✅
      │   ├─> OnUpdateSetting ✅
      │   ├─> OnRestoreSettingsDefaults ✅
      │   └─> OnSaveSettings ✅
      └─> is_settings_page_view = true
          └─> Calls applySettingsState() with payload
  └─> JavaScript calls GetSettingsSnapshot()
      └─> ✅ Works! Bridge function is available
  └─> Settings load correctly into checkboxes ✅
```

## Files Modified

1. **src/UI.cpp**
   - Moved settings bridge bindings to common section (applies to all views)
   - Added comment explaining why this is needed

2. **assets/menu.html**
   - Removed "Quick settings" heading and all toggle items (dark theme, adblock, suggestions)
   - Removed `getSettingsValues()` function
   - Removed `setToggleState()` function
   - Removed `refreshQuickSettings()` function and its calls
   - Simplified click handler (removed toggle cases)

## Testing Instructions

1. **Close any running browser instance** (important for rebuild)
2. Rebuild: `cmake --build build --config Release`
3. Run: `.\test-settings.ps1`
4. Open menu (three dots) - verify no "Quick settings" section
5. Click "Settings" - page should load without errors
6. Open F12 DevTools console
7. Should see:
   ```
   [Settings] refreshSnapshot called
   [Settings] Raw payload type: string
   [Settings] Raw payload: {"values":{...},"meta":{...}}
   [Settings] Coerced payload: {values: {...}, meta: {...}}
   [Settings] Setting launch_dark_theme from values: true → true
   ```
8. Checkboxes should reflect values from setup/settings.json:
   - ✅ Launch in dark theme: checked
   - ❌ Enable ad blocking: unchecked
   - ❌ Show download badge: unchecked
   - ✅ Vibrant window theme: checked

## Expected Results

- ✅ No "GetSettingsSnapshot not available" errors
- ✅ Settings page loads with correct checkbox values
- ✅ Menu has no Quick Settings section
- ✅ All settings functionality works in dedicated Settings page
- ✅ Toggle settings → immediate runtime effect
- ✅ Save settings → persists to setup/settings.json
- ✅ Restart browser → settings load correctly

## Summary

The issue was a **JavaScript context scope problem**. Ultralight creates separate JavaScript contexts for each View (UI overlay, tabs, overlays). Bridge functions must be explicitly bound to each context. By moving the settings bridge bindings to the common section of `OnDOMReady()`, they now get bound to all views, including the settings page tab.

This is a common pattern in Ultralight applications when bridge functions need to be available across multiple views/contexts.
