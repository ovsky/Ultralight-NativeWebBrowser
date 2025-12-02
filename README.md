<p align="center">
  <img src="https://github.com/ultralight-ux/Ultralight/raw/master/media/logo.png" width="180" alt="Ultralight Logo">
</p>

<h1 align="center">Ultralight Web Browser</h1>

<p align="center">
  <strong>Ultra‑fast · Ultra‑light · Ultra‑portable</strong><br/>
  A native C++ browser focused on minimal overhead, instant startup, and resource efficiency.
</p>

<p align="center">
  <a href="https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-all.yml?query=branch%3Adev">
    <img src="https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-all.yml/badge.svg?branch=dev" alt="Build Status">
  </a>
  <a href="https://github.com/ovsky/Ultralight-WebBrowser/releases">
    <img src="https://img.shields.io/github/v/release/ovsky/Ultralight-WebBrowser?include_prereleases&label=release" alt="Release">
  </a>
  <a href="./LICENSE">
    <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License">
  </a>
  <img src="https://img.shields.io/badge/platforms-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey" alt="Platforms">
</p>

<p align="center">
  <a href="https://github.com/ovsky/Ultralight-WebBrowser/releases">📦 Download</a> ·
  <a href="#-features">✨ Features</a> ·
  <a href="#-build-from-source">🔧 Build</a> ·
  <a href="#-documentation">📚 Docs</a>
</p>

---

<p align="center">
  <img src="https://github.com/user-attachments/assets/fe2c4609-6930-483c-9fa3-eab64664b539" width="100%" alt="Ultralight Browser Screenshot">
</p>

---

## 🎯 Why Ultralight?

Traditional browsers embed full, sandboxed operating systems. They're powerful—but heavy. This project explores how far you can go with a **lightweight GPU renderer** + **native shell** for dramatically smaller footprint and near-instant startup.

<table>
<tr>
<td width="50%">

### ⚡ Performance Comparison

| Metric | Ultralight | Electron/CEF |
|--------|:----------:|:------------:|
| Startup Time | **< 1s** | 3–5s |
| Memory Usage | **~50 MB** | 500+ MB |
| Disk Footprint | **~40 MB** | 1+ GB |
| Render Speed | **6× faster** | Baseline |

</td>
<td width="50%">

### 🧠 Architecture Benefits

- ✅ No multi-process bloat
- ✅ No background daemons  
- ✅ No gigabytes of RAM for a few tabs
- ✅ Native C++ performance
- ✅ Simple embedding & extension

</td>
</tr>
</table>

<p align="center">
  <img src="https://ultralig.ht/media/base-memory-usage.webp" width="600" alt="Memory Comparison">
</p>

---

## ✨ Features

<table>
<tr>
<td width="50%" valign="top">

### 🌐 Core Browser
- **GPU-Accelerated Rendering** – Ultralight engine
- **Multi-Tab Interface** – Chrome-style draggable tabs
- **Full Web Standards** – HTML5, CSS3, Modern JS
- **Smart Navigation** – Back, Forward, Reload, Stop
- **Intelligent Suggestions** – URL autocompletion with favicons

### 🎨 Modern Interface
- **Glassmorphic Design** – Semi-transparent overlays with blur
- **Dark Mode** – System-wide theme with persistence
- **Compact Tabs** – Space-saving layout option
- **Responsive Layout** – Fluid adaptation to any size

### 🔒 Privacy & Security
- **Ad & Tracker Blocking** – Domain, glob, and pattern matching
- **Do Not Track** – Configurable DNT header
- **Privacy Controls** – JavaScript, cookies, storage permissions
- **Auto-Clear History** – Optional cleanup on exit

</td>
<td width="50%" valign="top">

### ⚙️ Power Features
- **Bookmark System** – Star icon, bookmarks bar, manager
- **Download Manager** – Progress tracking, notifications
- **Settings Panel** – 26+ configurable options across 7 categories
- **Settings Search** – Quick-find any preference
- **Auto-Save** – Automatic preference persistence
- **Custom User Agent** – Configurable browser identity
- **Keyboard Shortcuts** – Fully customizable mappings

### 🎬 DRM Content Support
- **Native WebView Fallback** – For protected streaming
- **Windows** – WebView2 (Edge/Chromium)
- **macOS** – WKWebView (Cocoa/WebKit)
- **Linux** – WebKit2GTK integration
- **Automatic Detection** – Seamless content switching

### 🛠️ Developer Tools
- **JS Bridge API** – `window.__ul` for deep integration
- **Debug Panel** – Runtime inspection
- **Performance Overlay** – Real-time metrics

</td>
</tr>
</table>

---

## 🖥️ Platform Support

| Platform | Architecture | Status | Package Formats |
|:--------:|:------------:|:------:|:---------------:|
| **Windows** | x64 | ✅ Full | ZIP, NSIS Installer |
| **macOS** | x64 | ✅ Full | TGZ, DMG |
| **macOS** | ARM64 (Apple Silicon) | ✅ Full | TGZ, DMG |
| **Linux** | x64 | ✅ Full | TGZ, DEB, RPM |
| **Linux** | ARM64 | ✅ Full | TGZ, DEB, RPM |
| **Windows** | ARM64 | 🔜 Soon | — |

---

## 📥 Installation

<details>
<summary><b>🪟 Windows</b></summary>

1. Download `Ultralight-WebBrowser-*-Windows-Portable.zip` from [Releases](https://github.com/ovsky/Ultralight-WebBrowser/releases)
2. Extract to any folder
3. Run `Ultralight-WebBrowser.exe`

*Optional: Use the NSIS installer (`*-Windows-Installer.exe`) if available.*

</details>

<details>
<summary><b>🍎 macOS</b></summary>

**TGZ Archive:**
```bash
tar -xzf Ultralight-WebBrowser-*.tar.gz
./Ultralight-WebBrowser
```

**DMG Image:**
1. Mount the DMG
2. Drag to Applications
3. Right-click → Open (first launch, to bypass Gatekeeper)

</details>

<details>
<summary><b>🐧 Linux</b></summary>

**Debian/Ubuntu (DEB):**
```bash
sudo apt install ./Ultralight-WebBrowser-*.deb
ultralight-webbrowser
```

**Fedora/RHEL (RPM):**
```bash
sudo dnf install ./Ultralight-WebBrowser-*.rpm
ultralight-webbrowser
```

**Portable (TGZ):**
```bash
tar -xzf Ultralight-WebBrowser-*.tar.gz -C ~/.local/opt
~/.local/opt/UltralightWebBrowser/Ultralight-WebBrowser
```

</details>

---

## 🔧 Build from Source

### Prerequisites

- CMake ≥ 3.10
- C++17 compiler (MSVC 2019+, GCC 9+, Clang 9+)
- Ultralight SDK (bundled or set `ULTRALIGHT_SDK_ROOT`)

### Quick Build

```bash
# Clone
git clone https://github.com/ovsky/Ultralight-WebBrowser.git
cd Ultralight-WebBrowser

# Configure
cmake -S . -B build -DWEBBROWSER_VERSION="dev"

# Build
cmake --build build --config Release
```

### Platform-Specific Notes

<details>
<summary><b>Windows</b></summary>

```powershell
# Quick scripts available:
./compilation_complete.ps1    # Configure + Build + Run
./compilation_fastbuild.ps1   # Incremental build + Run
```

</details>

<details>
<summary><b>macOS (ARM64)</b></summary>

```bash
# Download ARM64 SDK, then:
cmake -S . -B build -DULTRALIGHT_SDK_ROOT=/path/to/arm64/sdk
cmake --build build --parallel
```

</details>

<details>
<summary><b>Linux</b></summary>

```bash
# Install dependencies (Ubuntu/Debian):
sudo apt install libgtk-3-dev libwebkit2gtk-4.1-dev

cmake --build build --parallel
```

</details>

---

## 📚 Documentation

<details>
<summary><b>🧩 JavaScript Bridge API</b></summary>

The `window.__ul` API is injected into the main frame for native integration:

```javascript
// Navigation
__ul.navigate("https://example.com");
__ul.newTab("https://example.org");
__ul.back(); __ul.forward(); __ul.reload();

// Settings
const settings = __ul.getSettingsSnapshot();
__ul.updateSetting("enable_adblock", true);
__ul.saveSettings();

// Theme
__ul.toggleDarkMode();
if (__ul.isDarkModeEnabled()) { /* ... */ }

// History
const history = __ul.getHistory();
__ul.clearHistory();

// App Info
const info = __ul.getAppInfo();
// { name: "Ultralight WebBrowser", version: "1.4.0" }
```

</details>

<details>
<summary><b>🔐 DRM WebView System</b></summary>

For DRM-protected content (Netflix, Disney+, etc.):

| Platform | WebView | DRM Support |
|----------|---------|-------------|
| Windows | WebView2 | Widevine, PlayReady |
| macOS | WKWebView | FairPlay, Widevine |
| Linux | WebKit2GTK | Limited |

**Enable:** Settings → DRM Content → Enable DRM WebView

The DRM WebView subsystem provides native WebView integration for playing protected content that cannot be rendered by the Ultralight engine.

**How It Works:**
1. Browser detects DRM-protected content via content type or site rules
2. Automatically switches from Ultralight to native WebView
3. Native WebView overlays the main window with proper z-ordering
4. User can switch back to Ultralight for regular browsing

</details>

<details>
<summary><b>🛡️ Ad Blocking</b></summary>

Rules loaded from:
- `assets/blocklist.txt`
- `assets/filters/*.txt`

Supported formats:
```
example.com
0.0.0.0 example.com
||example.com^
/ads.js
*://*/*analytics*.js
```

Always allowed: `file://`, `data:` URLs

Toggle via toolbar icon or Settings → Privacy → Enable AdBlock

</details>

<details>
<summary><b>⚙️ Settings Categories</b></summary>

| Category | Options |
|----------|---------|
| **Appearance** | Dark Mode, Vibrant Window, Transparent Toolbar, Compact Tabs |
| **Privacy & Security** | AdBlock, Tracker Blocking, JavaScript, Cookies, DNT, Clear History |
| **Address Bar** | Autocompletion, Favicons, Suggestions |
| **Downloads** | Badge, Auto-open Panel, Location Prompt |
| **Performance** | Smooth Scrolling, Hardware Acceleration, Local Storage |
| **Accessibility** | Reduce Motion, High Contrast, Caret Browsing |
| **Developer** | Remote Inspector, Performance Overlay |
| **DRM Content** | Enable DRM WebView |

</details>

<details>
<summary><b>⌨️ Keyboard Shortcuts</b></summary>

Shortcuts are customizable via `assets/shortcuts.json`:

| Action | Default Shortcut |
|--------|------------------|
| New Tab | `Ctrl+T` |
| Close Tab | `Ctrl+W` |
| Reload | `Ctrl+R` / `F5` |
| Back | `Alt+Left` |
| Forward | `Alt+Right` |
| Address Bar | `Ctrl+L` |
| Find | `Ctrl+F` |
| Settings | `Ctrl+,` |
| Developer Tools | `F12` |

</details>

---

## 🗺️ Roadmap

<table>
<tr>
<td width="33%" valign="top">

### ✅ Complete
- GPU-Accelerated Rendering
- Multi-Tab Interface
- Ad & Tracker Blocking
- Download Manager
- Bookmark System
- Settings Panel (26+ options)
- Dark Mode & Themes
- DRM WebView (all platforms)
- ARM64 Support
- Auto-Save Settings
- Custom User Agent
- Settings Search
- Compact Tabs Mode

</td>
<td width="33%" valign="top">

### 🚧 In Progress
- Bookmark Import/Export
- Folder Organization
- DRM Detection Improvements
- Performance Optimizations
- Accessibility Enhancements

</td>
<td width="33%" valign="top">

### 🔮 Planned
- Session Restore
- Extension/Plugin API
- Persistent History
- Multi-Profile Support
- Password Manager
- Tab Groups
- Reader Mode
- Screenshot Tool
- Custom Themes
- Sync Service

</td>
</tr>
</table>

---

## 🔧 Troubleshooting

<details>
<summary><b>Common Issues</b></summary>

| Issue | Solution |
|-------|----------|
| Blank window | Verify OpenGL 3.3 support; check library files |
| Settings not loading | Check `setup/settings.json` exists and is valid JSON |
| AdBlock not working | Ensure SDK supports network interception |
| DRM not playing | Enable in Settings → DRM Content |
| DRM WebView crash (macOS) | Ensure Cocoa/WebKit frameworks available |
| DRM WebView crash (Linux) | Install `libwebkit2gtk-4.1-dev` |
| macOS Gatekeeper | Right-click → Open (first launch) |
| Bookmarks not saving | Check write permissions for storage directory |

</details>

<details>
<summary><b>Debug Tips</b></summary>

1. **Launch from terminal** to see initialization logs
2. **Enable Performance Overlay** in Settings → Developer
3. **Delete `setup/settings.json`** to reset to defaults
4. **Check `assets/blocklist.txt`** for filter syntax errors
5. **Verify SDK presence** in `data/` or `ULTRALIGHT_SDK_ROOT`

</details>

---

## 🔄 CI / Automation

### Active Workflows

| Workflow | Platform | Features |
|----------|----------|----------|
| `build-all.yml` | All | Meta-workflow (main badge) |
| `build-windows.yml` | Windows x64 | Optional NSIS installer |
| `build-macos.yml` | macOS x64 | Cocoa/WebKit linking |
| `build-macos-arm64.yml` | macOS ARM64 | Apple Silicon |
| `build-linux.yml` | Linux x64 | GTK3, WebKit2GTK |
| `build-linux-arm64.yml` | Linux ARM64 | Self-hosted/emulated |

### Environment Variables

| Variable | Purpose |
|----------|---------|
| `ULTRALIGHT_SDK_URL` | Override auto-detected SDK URL |
| `ULTRALIGHT_VERSION` | Override SDK version |
| `WEBBROWSER_VERSION` | App version label |
| `PACKAGE_GENERATORS` | CPack generators (`TGZ;DEB;RPM`) |
| `CREATE_INSTALLER` | Build NSIS installer (Windows) |

---

## 🛠️ Tech Stack

| Component | Technology |
|-----------|------------|
| **Renderer** | [Ultralight SDK](https://ultralig.ht/) |
| **Language** | C++17, Objective-C++ (macOS) |
| **Window/Input** | [GLFW](https://www.glfw.org/) |
| **Graphics** | OpenGL 3.3 |
| **Build System** | CMake + CPack |
| **CI/CD** | GitHub Actions (x64 + ARM64) |
| **DRM (Windows)** | WebView2 (Edge/Chromium) |
| **DRM (macOS)** | WKWebView (Cocoa/WebKit) |
| **DRM (Linux)** | WebKit2GTK |

---

## 🤝 Contributing

1. **Fork** the repository
2. **Create** feature branch: `git checkout -b feature/amazing-feature`
3. **Commit** changes: `git commit -m "Add amazing feature"`
4. **Push**: `git push origin feature/amazing-feature`
5. **Open** a Pull Request

**Guidelines:**
- Keep PRs focused and small
- Include performance notes if relevant
- Add tests for core logic changes
- Follow existing code style

---

## 🔒 Security & Privacy

This project:
- Does **NOT** implement hardened sandboxing found in mainstream browsers
- Stores only **ephemeral in-memory history** (no disk persistence by default)
- Performs **lightweight filtering**—no advanced tracker heuristics
- Should **not** be used for security-critical tasks without further auditing

---

## 📄 License

MIT License — see [LICENSE](./LICENSE) for details.

---

## 🙏 Acknowledgements

- [Ultralight](https://ultralig.ht/) — Lightweight HTML renderer
- [GLFW](https://www.glfw.org/) — Cross-platform windowing
- Open-source community contributors

---

<p align="center">
  <sub>
    ⚠️ <b>Disclaimer:</b> This is an educational project demonstrating minimal browser architecture. 
    Not recommended for security-critical tasks without additional hardening.
  </sub>
</p>

<p align="center">
  Made with ❤️ by <a href="https://github.com/ovsky">@ovsky</a>
</p>
