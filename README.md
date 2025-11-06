# Ultralight Web Browser
> Ultra‑fast • Ultra‑light • Ultra‑portable

[![Build - Linux (x64)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-linux.yml/badge.svg?branch=dev)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-linux.yml?query=branch%3Adev)
[![Build - macOS (x64/arm64)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-macos.yml/badge.svg?branch=dev)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-macos.yml?query=branch%3Adev)
[![Build - Windows (x64)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-windows.yml/badge.svg?branch=dev)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-windows.yml?query=branch%3Adev)

<p align="center">
  <img src="https://github.com/ultralight-ux/Ultralight/raw/master/media/logo.png" width="200" alt="Ultralight Logo">
</p>

**A native C++ proof‑of‑concept browser focused on minimal overhead, cold‑start speed, and resource efficiency.**  
No multi‑process bloat, no background daemons, no gigabytes of RAM for a handful of tabs—just a lean renderer + native UI.

> Status: Experimental / Educational. Not intended as a production security‑hardened browser.

---

## Table of Contents
1. [Why Ultralight?](#-why-ultralight-ditch-the-bloat)  
2. [Project Philosophy & Goals](#-project-philosophy--goals)  
3. [Supported Platforms & Architectures](#-supported-platforms--architectures)  
4. [Downloads](#-get-the-app)  
5. [Installation](#-installation)  
6. [Features](#-features)  
7. [Tech Stack](#-tech-stack)  
8. [Building From Source](#-build-from-source)  
9. [ARM64 Build Notes](#-arm64-build-notes)  
10. [JavaScript Bridge API](#-javascript-bridge-api-window__ul)  
11. [Packaging](#-create-packages-locally-optional)  
12. [CI / Automation](#-ci--automation)  
13. [Roadmap](#-roadmap--ideas)  
14. [Troubleshooting](#-troubleshooting)  
15. [Contributing](#-contributing)  
16. [Security & Privacy](#-security--privacy)  
17. [License](#-license)  
18. [Acknowledgements](#-acknowledgements)

---

## 🚀 Why Ultralight? Ditch the Bloat.
Traditional browsers (and desktop web stacks like Electron / CEF) embed full, sandboxed operating systems (Chromium). They are powerful—but heavy. This project explores how far you can go by combining a **GPU‑accelerated HTML/CSS/JS renderer** (Ultralight) with a **thin native shell**.

![Ultralight Memory Usage](https://ultralig.ht/media/base-memory-usage.webp)

Result: lower memory pressure, near‑instant cold starts, smaller footprint, simple embedding.

---

## 🎯 Project Philosophy & Goals
| Feature | Ultralight (This Project) | Electron / CEF |
|--------|---------------------------|----------------|
| Performance | Up to 6× faster in simple page render ops | Chromium baseline |
| Memory Usage | ~1/10 RAM (no multi-process sandbox) | High (multi-process JS + GPU + extensions) |
| Startup | < 1s typical | 3–5s cold start |
| Disk Footprint | ~30–50 MB packaged | 1+ GB (runtime + cache) |
| Rendering | Lightweight GPU | Full Chromium stack |
| Architecture | Native C++ + pixel buffer compositing | Node.js + Chromium + interop bridge |

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
| Linux (Ubuntu/Fedora etc.) | x64 (arm64 SDK logic present) | TGZ / DEB / RPM | arm64 requires arm64 runner; workflow includes detection & fallback |

ARM64 archives are probed automatically when available in the `base-sdk` branch (eg: `ultralight-free-sdk-<ver>-linux-arm64.7z`, `...-mac-arm64.7z`). Current public CI uses x64 runners; arm64 builds must be triggered via:
- Self‑hosted runner (Apple Silicon / aarch64 Linux)
- Future strategy matrix addition (see CI section)

---

## 📥 Get the App
Official tagged releases:  
[**Releases Page → Ultralight Web Browser**](https://github.com/ovsky/Ultralight-WebBrowser/releases)

Development (continuous) artifacts (latest successful `dev` workflow runs):
| Platform | Packages | Latest Runs |
|----------|----------|-------------|
| Linux | TGZ, DEB, RPM | [Open runs](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-linux.yml?query=branch%3Adev) |
| macOS | TGZ, DMG | [Open runs](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-macos.yml?query=branch%3Adev) |
| Windows | ZIP (portable) / optional Installer | [Open runs](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-windows.yml?query=branch%3Adev) |

How to fetch artifacts:
1. Open the workflow link.
2. Click the latest green run.
3. Scroll to “Artifacts” (bottom) and download.

---

## 📦 Installation

### Windows
1. Download `Ultralight-WebBrowser-*-Windows-Portable.zip`.
2. Extract & run `Ultralight-WebBrowser.exe`.
3. Optional: Run installer (`*-Windows-Installer.exe`) if generated.

### macOS
- TGZ: Extract, move folder anywhere, run `Ultralight-WebBrowser`.
- DMG: Mount, drag the app folder to `/Applications` (or preferred location), run the binary.

### Linux
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
- **GPU‑Accelerated Rendering** (Ultralight core)
- **Low Memory Footprint** (single process shell)
- **HTML5 / CSS3 / Modern JS Support**
- **Native C++17 Application Layer**
- **Multi‑Tab Interface**
- **Navigation Controls** (Back / Forward / Reload / Stop / Address bar)
- **Dynamic Page Title + Loading Indicators**
- **Responsive Resize**
- **Lightweight Ad & Tracker Filtering**  
  - Domain + substring + simple glob patterns  
  - Rule sources: `assets/blocklist.txt` + all additional `.txt` in `assets/filters/`  
  - Formats: `example.com`, `0.0.0.0 example.com`, `||example.com^`, `/ads.js`, `*://*/*analytics*.js`  
  - Always allowed: `file://`, `data:`  
  - Requires SDK network interception capabilities  
- **Dark Mode (Global Toggle)**
- **Local In‑Memory History** (non‑persistent)
- **Favicon Support**
- **Autosuggestion / Autocompletion**
- **Download Manager + UI**
- **Context Menu / Shortcut Map Systems**
- **JavaScript ↔ Native Bridge** (`window.__ul`)

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

Navigation:
```js
__ul.back(); __ul.forward(); __ul.reload(); __ul.stop();
__ul.navigate("https://example.com");
__ul.newTab("https://example.org");
__ul.closeTab(); // or __ul.closeTab(id)
__ul.openHistory();
```

History:
```js
const h = __ul.getHistory(); // { items: [{ url, title, time }, ...] }
__ul.clearHistory();
```

Theme:
```js
if (!__ul.isDarkModeEnabled()) __ul.toggleDarkMode();
```

App Info:
```js
const info = __ul.getAppInfo(); // { name, version }
```

Notes:
- Bridge not injected into subframes.
- History is in‑memory; cleared on exit.
- Calls become no‑ops if underlying state unavailable.

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

### Suggested Future Enhancement (ARM64 Matrix)
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
Completed:
- Context Menu
- Local History
- Optimized Filtering System
- Universal Menu
- Shortcut Mapping
- Dark Theme (WIP polish)
- JS Bridge Enhancements
- Favicon Support
- Autosuggestion / Autocompletion
- Download Manager & UI
- Tab UX Improvements

Planned / Open:
- Bookmark System
- Plugin / Script Injection API
- Settings Panel (flags & experimental toggles)
- Persistent History / Sessions
- Multi‑process isolation options (research)
- Enhanced privacy filters (cosmetic blocking)

---

## 🔧 Troubleshooting
| Issue | Cause | Resolution |
|-------|-------|-----------|
| Blank window / no render | Missing GPU context / failed SDK load | Verify OpenGL 3.3 support; check `lib` / `dylib` / `dll` presence |
| Cannot load external pages | Network interception not available | Use full Ultralight SDK version; confirm `ULTRALIGHT_SDK_ROOT` contents |
| High CPU on resize | Continuous repaint loop | Known in rapid resize scenarios; consider throttle patch |
| Rules not applied | SDK lacks interception | Build with proper network layer; confirm rule file paths |
| macOS gatekeeper warning | Unsigned binaries | Right‑click “Open” once or sign with local cert |
| Arm64 archive not detected | CI runner architecture mismatch | Provide `sdk_url` manually or run on arm64 runner |

---

## 🔒 Security & Privacy
This project:
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
This is an educational project illustrating a minimal browser shell. It does not aim to replicate full Chromium feature parity (e.g., comprehensive security sandbox, extension ecosystem, advanced privacy tooling). Use at your discretion.

---
