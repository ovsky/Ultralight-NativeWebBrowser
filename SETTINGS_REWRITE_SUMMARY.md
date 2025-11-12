# Settings Workflow Rewrite - Summary

## Overview
Completely rewrote the settings workflow to be simpler, more reliable, and easier to debug. The settings now properly load from disk, display in the Settings page, can be changed in runtime, and persist when saved.

## Key Changes

### 1. Simplified Settings Serialization (C++)

**File:** `src/UI.cpp`

**What Changed:**
- Rewrote `BuildSettingsJSON()` to directly serialize the `settings_` struct members instead of using complex catalog iteration with member pointers
- Each setting is now explicitly written: `"launch_dark_theme": true/false`
- Eliminates possibility of catalog/member pointer mismatches

**Benefits:**
- Direct mapping ensures actual values are serialized
- Easier to debug (can see exact values being written)
- No reliance on runtime catalog initialization

### 2. Enhanced Debug Logging

**Files:** `src/UI.cpp`

**What Changed:**
- Added `#include <iostream>` for console output
- Added debug logging in `BuildSettingsJSON()` to show:
  - Current values of key settings (dark theme, adblock, suggestions)
  - Complete JSON output
- Added debug logging in `OnGetSettings()` to show:
  - Payload being returned to JavaScript
- Added debug logging in `OnDOMReady()` for settings page hydration

**Benefits:**
- Can trace complete data flow from disk → settings_ → JSON → JavaScript
- Identifies exactly where values might get lost or corrupted
- Console shows all bridge communication

### 3. Simplified Settings Page Hydration (JavaScript)

**File:** `assets/settings.html`

**What Changed:**
- Enhanced `refreshSnapshot()` with detailed logging:
  - Logs when bridge function is not available
  - Logs raw payload type and content
- Enhanced `applySettingsState()` with comprehensive debugging:
  - Logs raw payload received
  - Logs coerced payload (parsed JSON)
  - Logs extracted values and meta objects
  - Logs each setting as it's applied: source (values/defaults/current), computed value, final checkbox state
  - Logs completion status and dirty flag

**Benefits:**
- Can see exactly what JavaScript receives from C++
- Can trace how each checkbox value is determined
- Identifies if values object is empty, wrong, or checkboxes aren't updating

### 4. Removed Complex Deferred Hydration

**File:** `src/UI.cpp` - `OnDOMReady()` function

**What Changed:**
- Removed 40-iteration retry loop with 50ms delays
- Simplified to direct call when `applySettingsState` is available
- Added clear logging for both success and failure cases

**Benefits:**
- Faster page load (no artificial delays)
- Clearer error messages if hydration fails
- Easier to debug timing issues

## Settings Flow

### Complete Workflow

```
1. Browser Startup
   └─> UI() constructor
       └─> LoadSettingsFromDisk()
           ├─> Reads setup/settings.json
           ├─> Parses "values" object
           ├─> Updates settings_ struct
           └─> Sets saved_settings_ = settings_

2. Settings Page Opens
   └─> OnDOMReady() detects settings.html
       ├─> Binds GetSettingsSnapshot bridge function
       ├─> Calls BuildSettingsPayload(true)
       │   └─> BuildSettingsJSON() serializes settings_ to JSON
       └─> Calls applySettingsState(payload) in JavaScript
           ├─> Parses JSON string to object
           ├─> Extracts values: {launch_dark_theme: true, ...}
           └─> Sets each checkbox.checked = values[key]

3. User Toggles Setting
   └─> JavaScript change event handler
       ├─> Updates currentValues[key]
       ├─> Marks row as dirty
       └─> Calls OnUpdateSetting(key, value) bridge
           └─> HandleSettingMutation()
               ├─> Updates settings_[key]
               ├─> Calls ApplySettings() (applies to runtime)
               └─> Marks settings_dirty_ = true

4. User Clicks "Save Settings"
   └─> JavaScript calls OnSaveSettings()
       └─> SaveSettingsToDisk()
           ├─> Rotates backups (.2 ← .1 ← active)
           ├─> Writes setup/settings.json with values + meta
           └─> Sets saved_settings_ = settings_
       └─> SyncSettingsStateToUI()
           └─> Refreshes Settings page (clears dirty flags)

5. Browser Restart
   └─> Cycle repeats from step 1
       └─> LoadSettingsFromDisk() reads persisted settings.json
```

### Data Structures

**C++ (settings_ struct):**
```cpp
struct BrowserSettings {
    bool launch_dark_theme = false;
    bool enable_adblock = true;
    bool log_blocked_requests = false;
    bool enable_suggestions = true;
    bool enable_suggestion_favicons = true;
    bool show_download_badge = true;
    bool auto_open_download_panel = true;
    bool clear_history_on_exit = false;
    bool experimental_transparent_toolbar = false;
    bool experimental_compact_tabs = false;
    bool reduce_motion = false;
    bool high_contrast_ui = false;
    bool vibrant_window_theme = false;
};
```

**JSON (setup/settings.json):**
```json
{
  "values": {
    "launch_dark_theme": true,
    "enable_adblock": false,
    ...
  },
  "meta": {
    "updated_at": "2025-11-09T12:00:00Z",
    "dirty": false,
    "storage_path": "setup/settings.json",
    "dirty_keys": [],
    "catalog": [...]
  }
}
```

**JavaScript (applySettingsState):**
```javascript
const payload = {
  values: {launch_dark_theme: true, enable_adblock: false, ...},
  meta: {dirty: false, baseline: true, ...}
};
// Sets: input.checked = payload.values[key]
```

## Testing Instructions

### 1. Verify Initial Load
```powershell
# Run the test script
.\test-settings.ps1
```

Expected console output:
```
[BuildSettingsJSON] settings_ values:
  launch_dark_theme: 1
  enable_adblock: 0
  enable_suggestions: 1
[BuildSettingsJSON] Result: {"launch_dark_theme":true,"enable_adblock":false,...}
```

### 2. Verify Settings Page Load
1. Click Menu (three dots) → Settings
2. Open browser DevTools (F12)
3. Check console for:
```
[OnDOMReady] Settings page loaded, calling applySettingsState with payload
[Settings] refreshSnapshot called
[Settings] Raw payload type: string
[Settings] Raw payload: {"values":{...},"meta":{...}}
[Settings] Coerced payload: {values: {...}, meta: {...}}
[Settings] Extracted values: {launch_dark_theme: true, enable_adblock: false, ...}
[Settings] Setting launch_dark_theme from values: true → true
[Settings]  → Final state for launch_dark_theme: value= true , checkbox.checked= true
```

### 3. Verify Checkboxes Match Test Settings
With the provided `setup/settings.json`:
- ✅ Launch in dark theme: **checked**
- ❌ Enable ad blocking: **unchecked**
- ✅ Show address bar suggestions: **checked**
- ❌ Show download badge: **unchecked**
- ✅ Vibrant window theme: **checked**

### 4. Verify Runtime Changes
1. Toggle "Enable ad blocking" checkbox
2. Console should show:
```
[Settings] Setting enable_adblock from values: true → true
```
3. Setting should apply immediately (adblock enables/disables)
4. Row should show "dirty" indicator

### 5. Verify Persistence
1. Toggle several settings
2. Click "Save Settings" button
3. Console should show save operation
4. Restart browser
5. Open Settings page
6. Verify all checkboxes match saved state

## Debugging Tips

### If checkboxes are always false:
1. Check C++ console for `BuildSettingsJSON` output
   - Verify values are `1` (not `0`)
2. Check JavaScript console for `Raw payload` log
   - Verify JSON string contains `"key":true`
3. Check `Coerced payload` log
   - Verify values object has boolean `true` values
4. Check individual setting logs
   - Verify `values[key]` is `true`, not `"true"` (string)

### If settings don't persist:
1. Check `setup/settings.json` exists and is writable
2. Verify `OnSaveSettings` is called (add breakpoint or console.log)
3. Check file contents after save (should have updated timestamp)
4. Verify `LoadSettingsFromDisk` reads from correct path

### If runtime changes don't work:
1. Verify `OnUpdateSetting` receives correct parameters
2. Check `HandleSettingMutation` finds the descriptor
3. Verify `ApplySettings` is called after mutation
4. For adblock/dark mode, check the specific handler runs

## Files Modified

1. **src/UI.cpp**
   - Added `#include <iostream>`
   - Rewrote `BuildSettingsJSON()` (direct serialization)
   - Enhanced `OnGetSettings()` (debug logging)
   - Simplified `OnDOMReady()` settings page hydration
   - Added debug logs throughout

2. **assets/settings.html**
   - Enhanced `refreshSnapshot()` with detailed logging
   - Enhanced `applySettingsState()` with comprehensive debugging
   - Logs show complete data flow from bridge to checkboxes

3. **setup/settings.json** (created)
   - Test configuration with mixed true/false values
   - Used to verify load/save/persistence workflow

4. **test-settings.ps1** (created)
   - Quick test script with expected behavior checklist
   - Launches browser with instructions

## Expected Results

After these changes:
1. ✅ Settings load from `setup/settings.json` on browser start
2. ✅ Settings page checkboxes reflect actual saved values
3. ✅ Toggling checkboxes updates runtime state immediately
4. ✅ Clicking "Save Settings" persists changes to disk
5. ✅ Restarting browser loads previously saved settings
6. ✅ Console provides complete visibility into settings flow
7. ✅ No complex deferred hydration delays or timing issues

## Next Steps

If issues persist after this rewrite:
1. Run `test-settings.ps1` and capture full console output
2. Check both C++ console (stdout) and JavaScript console (F12)
3. Verify `setup/settings.json` file format matches expected structure
4. Look for error messages in either console
5. Use debug logs to identify exact failure point

The comprehensive logging should make it immediately obvious where the data flow breaks.
