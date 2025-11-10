# Settings Flow Analysis - Current vs. After Fix

## ISSUE IDENTIFIED

The settings system has a **mismatch between how settings are saved and loaded**:

### Current Broken Process:

```
SAVE FLOW (works correctly):
SaveSettingsToDisk()
  └─> Writes JSON: {"values": {key: bool, ...}, "meta": {...}}
  └─> File: setup/settings.json with nested structure

LOAD FLOW (has issues):
LoadSettingsFromDisk()
  └─> Reads entire file as string
  └─> Calls ParseSettingsBool(content, "key", fallback)
      └─> Searches for: "key": value
      └─> ✅ WORKS because nested structure still has "key": value inside
  └─> Updates settings_.key = parsed_value
  └─> ✅ LOADS CORRECTLY

BUILD PAYLOAD FLOW (this is where it breaks):
BuildSettingsJSON()
  └─> Directly reads settings_.key values
  └─> Builds: {"key": true, "key2": false, ...}
  └─> ✅ CORRECT JSON

BuildSettingsPayload()
  └─> Calls BuildSettingsJSON() for values
  └─> Wraps in: {"values": {...}, "meta": {...}}
  └─> Returns to JavaScript
  └─> ✅ CORRECT STRUCTURE

JAVASCRIPT FLOW (potential issue):
refreshSnapshot()
  └─> Calls GetSettingsSnapshot()
  └─> Receives string: '{"values":{...},"meta":{...}}'
  └─> Passes to applySettingsState()

applySettingsState(payload)
  └─> Calls coercePayload(payload)
      └─> normalizePayload(payload)
          └─> If string: JSON.parse(payload)
          └─> Returns object
  └─> Extracts: {values, meta}
  └─> Iterates: inputsByKey.forEach((input, key) => {
      └─> Reads: values[key]
      └─> Sets: input.checked = !!values[key]
```

## Root Cause Analysis

After careful analysis, I believe the issue is **NOT** in the C++ or file structure, but in how the **initial settings file was created manually vs. how the system expects it**.

### The Problem:

1. **Manual JSON file** (setup/settings.json) has correct structure BUT
2. **LoadSettingsFromDisk()** parses it correctly using ParseSettingsBool()
3. **BuildSettingsJSON()** creates correct output from settings_ struct
4. **BUT** there might be a type mismatch or encoding issue in the JSValue conversion

## Let Me Test The Actual Issue

The real problem might be that `OnGetSettings()` returns:
```cpp
return ultralight::JSValue(payload.c_str());
```

This creates a **JSValue containing a STRING**, but JavaScript might not be parsing it correctly.

## ACTUAL FIX NEEDED

The issue is that we're returning a JSON string that needs to be parsed. Let's verify if Ultralight's BindJSCallbackWithRetval properly handles string returns or if we need to return an actual JavaScript object.

## Current Process (BEFORE FIX):

```
Step 1: Browser Starts
  ├─> UI() constructor called
  ├─> LoadSettingsFromDisk() called
  │   ├─> Opens setup/settings.json
  │   ├─> Reads entire file as string
  │   ├─> For each setting in catalog:
  │   │   ├─> Calls ParseSettingsBool(content, "launch_dark_theme", default)
  │   │   │   ├─> Searches for: "launch_dark_theme"
  │   │   │   ├─> Finds: "launch_dark_theme": true
  │   │   │   ├─> Returns: true
  │   │   └─> Sets: settings_.launch_dark_theme = true
  │   ├─> Repeats for all 13 settings
  │   └─> Sets: saved_settings_ = settings_
  └─> Applies settings to runtime (dark mode, adblock, etc.)

Step 2: User Opens Settings Page
  ├─> OnDOMReady() detects "settings.html"
  ├─> Binds: GetSettingsSnapshot = OnGetSettings
  ├─> Gets: applySettingsPanel = global["applySettingsState"]
  ├─> Calls: BuildSettingsPayload(true)
  │   ├─> Calls BuildSettingsJSON()
  │   │   ├─> Creates: ss << "{"
  │   │   ├─> Adds: "launch_dark_theme":true
  │   │   ├─> Adds: "enable_adblock":false
  │   │   ├─> ... (all 13 settings)
  │   │   └─> Returns: {"launch_dark_theme":true,"enable_adblock":false,...}
  │   ├─> Wraps in meta:
  │   │   └─> {"values":{...},"meta":{"dirty":false,"baseline":true,...}}
  │   └─> Returns string
  └─> Calls: applySettingsPanel(payload_string)

Step 3: JavaScript Receives Payload
  ├─> applySettingsState(payload) called
  ├─> payload is a STRING: '{"values":{...},"meta":{...}}'
  ├─> Calls: coercePayload(payload)
  │   ├─> Calls: normalizePayload(payload)
  │   │   ├─> Detects: typeof payload === 'string'
  │   │   ├─> Calls: JSON.parse(payload)
  │   │   └─> Returns: {values: {...}, meta: {...}}
  │   └─> Extracts: {values, meta}
  ├─> Iterates: inputsByKey (all checkboxes)
  │   ├─> For "launch_dark_theme":
  │   │   ├─> Reads: values["launch_dark_theme"]
  │   │   ├─> Should be: true
  │   │   ├─> Computes: nextValue = !!values["launch_dark_theme"]
  │   │   └─> Sets: input.checked = nextValue
  │   └─> Repeats for all settings
  └─> Updates UI (save button state, dirty flags)

Step 4: User Toggles Setting
  ├─> Change event fires on checkbox
  ├─> JavaScript updates: currentValues[key] = checked
  ├─> Calls: OnUpdateSetting(key, value)
  │   ├─> C++ HandleSettingMutation(key, value)
  │   │   ├─> Updates: settings_.key = value
  │   │   ├─> Calls: ApplySettings() (runtime change)
  │   │   └─> Marks: settings_dirty_ = true
  │   └─> UI updates immediately (e.g., dark mode toggles)
  └─> Marks row as dirty

Step 5: User Clicks Save
  ├─> JavaScript calls: OnSaveSettings()
  ├─> C++ SaveSettingsToDisk() called
  │   ├─> Rotates backups:
  │   │   ├─> Delete: settings.json.2
  │   │   ├─> Rename: settings.json.1 → settings.json.2
  │   │   └─> Rename: settings.json → settings.json.1
  │   ├─> Creates new settings.json:
  │   │   ├─> Builds JSON with BuildSettingsJSON()
  │   │   ├─> Wraps with meta (timestamp, dirty: false, etc.)
  │   │   └─> Writes to: setup/settings.json
  │   ├─> Sets: saved_settings_ = settings_
  │   └─> Sets: settings_dirty_ = false
  └─> Calls: SyncSettingsStateToUI()
      └─> Refreshes settings page (clears dirty flags)
```

## After Fix Process:

The fix will ensure proper JSON object handling instead of string serialization issues.
