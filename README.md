# Ultralight Web Browser ✨
> Ultra‑fast / Ultra‑light / Ultra‑portable

[![Build - Linux (x64)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-linux.yml/badge.svg?branch=dev)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-linux.yml?query=branch%3Amain)
[![Build - macOS (x64)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-macos.yml/badge.svg?branch=dev)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-macos.yml?query=branch%3Amain)
[![Build - Windows (x64)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-windows.yml/badge.svg?branch=dev)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-windows.yml?query=branch%3Amain)
[![Build - Linux (ARM64)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-linux-arm64.yml/badge.svg)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-linux-arm64.yml)
[![Build - macOS (ARM64)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-macos-arm64.yml/badge.svg)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-macos-arm64.yml)

<p align="center">
  <img src="https://github.com/ultralight-ux/Ultralight/raw/master/media/logo.png" width="200" alt="Ultralight Logo">
</p>

<strong>A native C++ proof‑of‑concept browser focused on minimal overhead, cold‑start speed, and resource efficiency.</strong><br/>
No multi‑process bloat, no background daemons, no gigabytes of RAM for a handful of tabs—just a lean renderer + native UI.

> Status: Development / Experimental / Educational

---

<img width="1366" height="768" alt="ultralight-downloads" src="https://github.com/user-attachments/assets/fe2c4609-6930-483c-9fa3-eab64664b539" />

---

## 🧭 Table of Contents
1. [Why Ultralight?](#-why-ultralight-ditch-the-bloat)
2. [Project Philosophy & Goals](#-project-philosophy--goals)
3. [Supported Platforms & Architectures](#-supported-platforms--architectures)
4. [Get the App](#-get-the-app)
5. [Installation](#-installation)
6. [Features](#-features)
7. [Tech Stack](#-tech-stack)
8. [Build From Source](#-build-from-source)
9. [ARM64 Build Notes](#-arm64-build-notes)
10. [JavaScript Bridge API](#-javascript-bridge-api-window__ul)
11. [Create Packages Locally](#-create-packages-locally-optional)
12. [CI / Automation](#-ci--automation)
13. [Roadmap](#-roadmap--ideas)
14. [Troubleshooting](#-troubleshooting)
15. [Contributing](#-contributing)
16. [Security & Privacy](#-security--privacy)
17. [License](#-license)
18. [Acknowledgements](#-acknowledgements)
19. [Disclaimer](#-disclaimer)

---

## 🚀 Why Ultralight? Ditch the Bloat
Traditional browsers (and desktop web stacks like Electron / CEF) embed full, sandboxed operating systems (Chromium). They are powerful—but heavy. This project explores how far you can go by combining a lightweight GPU renderer with a native shell for a dramatically smaller footprint and near‑instant startup.

![Ultralight Memory Usage](https://ultralig.ht/media/base-memory-usage.webp)

Result: lower memory pressure, near‑instant cold starts, smaller footprint, simple embedding.

---

## 🎯 Project Philosophy & Goals
| Feature | Ultralight (This Project) | Electron / CEF |
|--------|---------------------------|----------------|
| Performance | Up to 6× faster in simple page render ops ⚡ | Chromium baseline |
| Memory Usage | ~1/10 RAM (no multi-process sandbox) 🧠 | High (multi-process JS + GPU + extensions) |
| Startup | < 1s typical 🚀 | 3–5s cold start |
| Disk Footprint | ~30–50 MB packaged 📦 | 1+ GB (runtime + cache) |
| Rendering | Lightweight GPU 🎨 | Full Chromium stack |
| Architecture | Native C++ + pixel buffer compositing 🧱 | Node.js + Chromium + interop bridge |

Goals:
- Showcase minimal native browser shell design.
- Provide reference for integrating Ultralight SDK.
- Experiment with lightweight content / ad blocking.
- Highlight performance vs conventional frameworks.

---

## 🖥️ Supported Platforms & Architectures
| Platform | Architectures | CI Artifacts | Notes |
|----------|---------------|--------------|-------|
| Windows 10+ | x64 | Portable ZIP, optional NSIS installer | arm64 not yet published (needs arm64 SDK + runner) |
| macOS 12+ | x64, arm64 | TGZ, optional DMG | arm64 auto‑detected when runner host is arm64 |
| Linux (Ubuntu/Fedora etc.) | x64 (arm64 logic present) | TGZ / DEB / RPM | arm64 requires aarch64 runner; workflow includes detection & fallback |

ARM64 archives are probed automatically when available in the `base-sdk` branch (eg: `ultralight-free-sdk-<ver>-linux-arm64.7z`, `...-mac-arm64.7z`). Current public CI uses x64 runners; arm64 builds may require:
- Self‑hosted runner (Apple Silicon / aarch64 Linux)
- Future strategy matrix addition (see CI section)

---

## 📥 Get the App
Official tagged releases:
[🎉 Releases Page → Ultralight Web Browser](https://github.com/ovsky/Ultralight-WebBrowser/releases)

Development (continuous) artifacts (latest successful `dev` workflow runs):

| Platform | Architectures | Packages | Latest Runs |
|----------|----------------|----------|-------------|
| Linux | x64, arm64* | TGZ, DEB, RPM | [Open runs](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-linux.yml?query=branch%3Adev) |
| macOS | x64, arm64 | TGZ, DMG | [Open runs](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-macos.yml?query=branch%3Adev) |
| Windows | x64 | ZIP (portable) / optional Installer | [Open runs](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-windows.yml?query=branch%3Adev) |

Notes:
- ✅ macOS arm64 artifacts are produced automatically when the CI runner is Apple Silicon.
- ✅ Linux arm64 artifacts appear when an aarch64 runner is used or when `ULTRALIGHT_SDK_URL` points to an arm64 SDK.
- ⌛ Windows arm64 artifacts are not yet published (pending arm64 SDK + toolchain).

How to fetch artifacts:
1. Open the workflow link for your platform/arch.
2. Click the latest green run (success).
3. Scroll to “Artifacts” (bottom) and download the desired archive.

---

## 📦 Installation

### Windows (x64)
1. Download `Ultralight-WebBrowser-*-Windows-Portable.zip`.
2. Extract & run `Ultralight-WebBrowser.exe`.
3. Optional: Run installer (`*-Windows-Installer.exe`) if generated.

### macOS (x64/arm64)
- TGZ: Extract, move folder anywhere, run `Ultralight-WebBrowser`.
- DMG: Mount, drag the app folder to `/Applications` (or preferred location), run the binary.

### Linux (x64/arm64)
DEB (Debian/Ubuntu):
```bash
sudo apt install ./Ultralight-WebBrowser-*.deb
ultralight-webbrowser
```
RPM (Fedora/RHEL/openSUSE):
```bash
sudo dnf install Ultralight-WebBrowser-*.rpm
ultralight-webbrowser
```
Portable TGZ (no root):
```bash
tar -C "$HOME/.local/opt" -xzf Ultralight-WebBrowser-*.tar.gz
"$HOME/.local/opt/UltralightWebBrowser/Ultralight-WebBrowser"
```
Packages install a desktop entry and icon + CLI launcher `ultralight-webbrowser`.

---

## ✨ Features

### Core Browser Functionality
- **GPU‑Accelerated Rendering** – Powered by Ultralight core engine
- **Low Memory Footprint** – Single process shell architecture (~1/10 RAM vs. Electron)
- **HTML5 / CSS3 / Modern JS Support** – Full web standards compliance
- **Multi‑Tab Interface** – Chrome-style draggable tabs with smooth animations
- **Navigation Controls** – Back / Forward / Reload / Stop / Address bar
- **Dynamic Page Indicators** – Real-time title updates and loading states
- **Responsive Resize** – Fluid layout adaptation

### Privacy & Security
- **Lightweight Ad & Tracker Filtering**
  - Domain + substring + glob pattern matching
  - Rule sources: `assets/blocklist.txt` + all `.txt` in `assets/filters/`
  - Formats: `example.com`, `0.0.0.0 example.com`, `||example.com^`, `/ads.js`, `*://*/*analytics*.js`
  - Always allowed: `file://`, `data:`
  - Toggle via toolbar icon or Settings
  - Requires SDK network interception capabilities
- **Do Not Track (DNT)** – Configurable header setting
- **Clear History on Exit** – Optional automatic cleanup
- **Web Security Controls** – JavaScript, cookies, storage permissions

### User Interface & Experience
- **Modern Glassmorphic Design** – Semi-transparent overlays with backdrop blur
- **Dark Mode** – Global theme toggle with persistent preferences
- **Compact Tabs Mode** – Space-saving layout (60px UI height, 12em tab width)
- **Toolbar Icons** – Quick access to Inspector, Downloads, AdBlock, Menu
- **Download Manager** – Full-featured UI with progress tracking and notifications
- **Settings Panel** – Comprehensive configuration across 7 categories:
  - Appearance (Dark Mode, Vibrant Window, Transparent Toolbar, Compact Tabs)
  - Privacy & Security (AdBlock, Trackers, JavaScript, Web Security, Cookies, DNT, History)
  - Address Bar & Suggestions (Autocompletion, Favicons)
  - Downloads (Badge, Auto-open Panel, Ask Location)
  - Performance (Smooth Scrolling, Hardware Acceleration, Local Storage, Database)
  - Accessibility (Reduce Motion, High Contrast, Caret Browsing)
  - Developer (Remote Inspector, Performance Overlay)
- **Persistent Settings** – JSON-based storage with runtime updates
- **Context Menu** – Right-click actions and shortcuts
- **Keyboard Shortcuts** – Customizable shortcut mapping system
- **Favicon Support** – Site icons in tabs and suggestions
- **Autosuggestion** – Intelligent URL/search completions with popular sites

### Developer Features
- **JavaScript ↔ Native Bridge** – `window.__ul` API for deep integration
- **Local History API** – In-memory browsing history (non-persistent by default)
- **Shortcut Mapping** – JSON-based keyboard shortcut configuration
- **Debug Panel** – Runtime settings inspection and diagnostics

---

## 🛠️ Tech Stack
| Layer | Technology |
|-------|------------|
| Renderer | [Ultralight SDK](https://ultralig.ht/) |
| Language | C++17 |
| Windowing/Input | [GLFW](https://www.glfw.org/) |
| Graphics | OpenGL 3.3 |
| Build | CMake + CPack |
| CI | GitHub Actions |

---

## 🧪 Build From Source

### Prerequisites
1. CMake ≥ 3.10
2. C++17 compiler (MSVC 2019+, GCC 9+, Clang 9+)
3. Ultralight SDK (bundled fallback under `data/`; override via `ULTRALIGHT_SDK_ROOT`)

### Steps
```bash
git clone https://github.com/ovsky/Ultralight-WebBrowser.git
cd Ultralight-WebBrowser

# (Optional) Submodules
git submodule update --init --recursive

# Configure
cmake -S . -B build \
  -DULTRALIGHT_SDK_ROOT="/absolute/path/to/ultralight-sdk" \
  -DWEBBROWSER_VERSION="dev"

# Build (Linux/macOS)
cmake --build build --parallel

# Build (Windows multi-config)
cmake --build build --config Release
```

Windows helper scripts:
```powershell
./build.bat   # configure + build (Release)
./run.bat     # run build\Release\Ultralight-WebBrowser.exe
```

Tests (if enabled):
```bash
ctest --test-dir build --output-on-failure
```

---

## 🧬 ARM64 Build Notes

### macOS (Apple Silicon)
- CI auto‑probes arm64 archives if the runner hardware is arm64.
- Local build: download `ultralight-free-sdk-<version>-mac-arm64.7z` (or `...-macos-arm64.7z` / `...-osx-arm64.7z`) into a directory and set:
  ```bash
  cmake -S . -B build -DULTRALIGHT_SDK_ROOT=/path/to/arm64/sdk
  ```

### Linux (aarch64)
- Workflow contains detection logic for `*-linux-arm64.7z`, but public CI runs on x64.
- Use a self‑hosted aarch64 runner or manually supply `ULTRALIGHT_SDK_URL` via workflow dispatch input.
- Native build steps identical; ensure system dependencies match (GTK3, NSS, etc.).

### Windows (arm64)
- Not yet supported in CI; requires arm64 Ultralight SDK + arm64 toolchain.
- Proposed steps:
  1. Acquire arm64 SDK archive (if/when published).
  2. Use Visual Studio with ARM64 configuration or cross toolchain.
  3. Add a strategy matrix entry in workflow (see CI section).

---

## 🧩 JavaScript Bridge API (`window.__ul`)
Injected into the main frame once DOM is ready.

### Navigation
```js
__ul.back();                              // Navigate back
__ul.forward();                           // Navigate forward
__ul.reload();                            // Reload current page
__ul.stop();                              // Stop loading
__ul.navigate("https://example.com");     // Navigate to URL
__ul.newTab("https://example.org");       // Open new tab with URL
__ul.closeTab();                          // Close current tab
__ul.closeTab(id);                        // Close specific tab by ID
__ul.openHistory();                       // Open history overlay
```

### Settings Management
```js
// Get complete settings snapshot
const settings = __ul.getSettingsSnapshot();
// Returns: { values: { dark_mode: true, ... }, meta: { ... } }

// Update individual setting
__ul.updateSetting("enable_adblock", true);
__ul.updateSetting("experimental_compact_tabs", false);

// Save current settings to disk
__ul.saveSettings();

// Restore factory defaults
__ul.restoreSettingsDefaults();
```

### AdBlock Control
```js
// Check AdBlock status
const isEnabled = __ul.getAdblockEnabled();

// Toggle AdBlock
__ul.toggleAdblock();
```

### History
```js
// Get browsing history
const h = __ul.getHistory();
// Returns: { items: [{ url, title, time }, ...] }

// Clear all history
__ul.clearHistory();
```

### Theme
```js
// Check dark mode status
if (!__ul.isDarkModeEnabled()) {
    __ul.toggleDarkMode();
}
```

### App Info
```js
// Get application metadata
const info = __ul.getAppInfo();
// Returns: { name: "Ultralight WebBrowser", version: "1.4.0" }
```

### Notes
- Bridge not injected into subframes (security consideration)
- History is in-memory; cleared on exit unless persistence enabled
- Settings are persisted to `setup/settings.json` on save
- Calls become no-ops if underlying state unavailable
- All settings changes trigger runtime updates (no restart required)

---

## 📦 Create Packages Locally (Optional)
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Linux
cpack --config build/CPackConfig.cmake -C Release -G TGZ
cpack --config build/CPackConfig.cmake -C Release -G DEB
cpack --config build/CPackConfig.cmake -C Release -G RPM

# macOS
cpack --config build/CPackConfig.cmake -C Release -G TGZ
# (DMG enabled via workflow or DragNDrop generator)
```

---

## 🔄 CI / Automation
Three workflows:
- `build-linux.yml` – Detects latest x64/arm64 SDK, builds, tests, packages (TGZ/DEB/RPM).
- `build-macos.yml` – Detects macOS SDK (supports multiple naming conventions), packages TGZ/DMG, cleans large artifacts.
- `build-windows.yml` – Detects Windows SDK, builds, optional NSIS installer when `create_installer` is true.

Environment variables / inputs:
| Variable | Purpose |
|----------|---------|
| `ULTRALIGHT_SDK_URL` | Override auto-detected SDK archive URL |
| `ULTRALIGHT_VERSION` | Override version embedded / probed |
| `WEBBROWSER_VERSION` | App version label during build |
| `PACKAGE_GENERATORS` (Linux/macOS) | CPack generator list (`TGZ;DEB;RPM`, `TGZ;DMG`, etc.) |
| `CREATE_INSTALLER` (Windows) | Build NSIS installer when true |

### 🚧 Suggested Future Enhancement (ARM64 Matrix)
```yaml
strategy:
  matrix:
    os: [ubuntu-latest, self-hosted-arm64]
    arch: [x64, arm64]
runs-on: ${{ matrix.os }}
```
Set `TARGET_ARCH` and select appropriate SDK archive per matrix entry.

---

## 🗺️ Roadmap / Ideas

### ✅ Completed
- ✓ Context Menu System
- ✓ Local History Management
- ✓ Optimized Content Filtering
- ✓ Universal Menu Interface
- ✓ Keyboard Shortcut Mapping
- ✓ Dark Theme Support
- ✓ JavaScript Bridge API
- ✓ Favicon Support
- ✓ Autosuggestion / Autocompletion
- ✓ Download Manager with UI
- ✓ Tab UX Improvements (Chrome-style draggable tabs)
- ✓ **Settings System** – Comprehensive preferences panel with 26+ options
- ✓ **Glassmorphic UI** – Modern semi-transparent design with backdrop blur
- ✓ **Compact Tabs Mode** – Space-saving layout option
- ✓ **AdBlock Toolbar Icon** – Quick toggle access
- ✓ **Runtime Settings Updates** – No restart required for changes
- ✓ **Persistent Configuration** – JSON-based settings storage

### 🚧 In Progress
- Performance Optimization (smooth scrolling, hardware acceleration toggles)
- Accessibility Features (high contrast, reduced motion, caret browsing)

### 🧩 Planned / Open
- **Bookmark System** – Save and organize favorite sites
- **Session Management** – Restore tabs on startup
- **Plugin / Extension API** – Script injection framework
- **Persistent History** – Optional long-term storage
- **Advanced Privacy Filters** – Cosmetic blocking (CSS selectors)
- **Multi-profile Support** – Separate settings/history per profile
- **Sync Service Integration** – Cross-device settings sync
- **Enhanced Developer Tools** – Integrated console and network inspector
- **Custom Themes** – User-defined color schemes
- **Password Manager** – Secure credential storage
- **Tab Groups** – Organize tabs into collapsible groups
- **Reader Mode** – Distraction-free article reading
- **Screenshot Tool** – Capture full page or visible area
- **Multi-process Isolation** – Optional sandboxing (research phase)

---

## 🔧 Troubleshooting
| Issue | Cause | Resolution |
|-------|-------|------------|
| Blank window / no render | Missing GPU context / failed SDK load | Verify OpenGL 3.3 support; check `lib` / `dylib` / `dll` presence |
| Cannot load external pages | Network interception not available | Use full Ultralight SDK version; confirm `ULTRALIGHT_SDK_ROOT` contents |
| High CPU on resize | Continuous repaint loop | Known in rapid resize scenarios; consider throttle patch |
| Rules not applied | SDK lacks interception | Build with proper network layer; confirm rule file paths |
| Settings not loading | Bridge initialization failed | Check browser console for "Bridge not ready" errors; verify `settings.json` exists in `setup/` directory |
| Settings won't save | File permission issues | Ensure write permissions for `setup/settings.json`; check disk space |
| AdBlock not working | Network interception disabled | Verify SDK supports request filtering; check AdBlock toggle in toolbar/settings |
| Compact tabs gap | Dynamic resize failed | Try toggling compact mode off/on; restart browser if issue persists |
| macOS gatekeeper warning | Unsigned binaries | Right‑click "Open" once or sign with local cert |
| Arm64 archive not detected | CI runner architecture mismatch | Provide `sdk_url` manually or run on arm64 runner |
| Downloads not appearing | Download manager initialization | Check permissions for downloads directory; verify UI overlay loaded |
| Shortcuts not working | JSON parse error | Validate `assets/shortcuts.json` syntax; check console for errors |

### Debug Tips
1. **Enable Debug Panel** – Settings → Developer → Show Performance Overlay
2. **Check Console Output** – Launch from terminal to see initialization logs
3. **Verify Settings File** – `setup/settings.json` should contain valid JSON structure
4. **Reset Settings** – Delete `setup/settings.json` to restore defaults
5. **Clear Cache** – Remove temporary files if rendering issues occur

---

## 🔒 Security & Privacy
This project currently:
- Does NOT implement hardened sandboxing found in mainstream browsers.
- Stores only ephemeral in‑memory history (no disk persistence by default).
- Performs lightweight filtering—no advanced tracker heuristics.
- Should not be used for security‑critical browsing tasks (banking, credentials) without further auditing.

---

## 🤝 Contributing
1. Fork the repository.
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Commit: `git commit -m "Add my feature"`
4. Push: `git push origin feature/my-feature`
5. Open a Pull Request.

Please:
- Keep PRs focused & small.
- Include before/after perf notes if relevant.
- Add tests when touching core logic (enable `BUILD_TESTING`).

---

## 📄 License
MIT License – see [LICENSE](./LICENSE).

---

## 🙏 Acknowledgements
- [Ultralight](https://ultralig.ht/) team for the renderer.
- [GLFW](https://www.glfw.org/) for cross‑platform window/input.
- Open‑source ecosystem contributors.

---

## 📌 Disclaimer
This is an educational project illustrating a minimal browser shell. It does not aim to replicate full Chromium feature parity (e.g., comprehensive security sandbox, extension ecosystem, advanced privacy tooling).
