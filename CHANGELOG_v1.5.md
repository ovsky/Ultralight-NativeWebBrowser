# Changelog: v1.2.0 → v1.5.0

## Release Information
- **Release Version**: v1.5.0
- **Previous Version**: v1.2.0
- **Release Date**: November 2025
- **Code Name**: "Enhanced Experience"

---

## 🎉 Major Features & Systems

### 🎨 Dark Mode System (NEW)
- **Intelligent Dark Mode** with automatic brightness detection
  - Only applies to bright pages (luminance > 0.5) to prevent over-darkening
  - Filter-based inversion approach (`invert(0.9) hue-rotate(180deg)`)
  - Preserves images, videos, and SVGs with counter-filters
  - Persistent toggle state across sessions
  - Removed "experimental" label - now production-ready

### ⚙️ Complete Settings System Overhaul
- **26 Configurable Settings** across 7 categories:
  - **Appearance** (4): Dark Mode, Vibrant Window, Transparent Toolbar, Compact Tabs
  - **Privacy & Security** (8): AdBlock, Trackers, JavaScript, Web Security, Cookies, DNT, Clear History, File URLs
  - **Address Bar & Suggestions** (2): Autocompletion, Favicons
  - **Downloads** (3): Badge notifications, Auto-open panel, Ask save location
  - **Performance** (4): Smooth scrolling, Hardware acceleration, Local storage, Database
  - **Accessibility** (3): Reduce motion, High contrast, Caret browsing
  - **Developer** (2): Remote inspector, Performance overlay

- **New Settings UI/UX**:
  - Modern glassmorphic design matching downloads panel aesthetic
  - Unified dark theme with CSS custom properties
  - Smaller, refined toggle switches (40x20px with gradients)
  - Settings open in dedicated tab (not overlay)
  - JSON-based persistence with automatic migration
  - Runtime updates without restart (where applicable)
  - Settings Bridge for JavaScript ↔ Native communication

### 📥 Advanced Download System
- **Complete Download Manager** with:
  - Progress tracking with live percentage and size indicators
  - Download notification badge on toolbar icon
  - Auto-open panel on new downloads (configurable)
  - GUID-like filename filtering for cleaner UI
  - Blob/data URL handling with intelligent suppression
  - Download history persistence
  - Custom save location support
  - Stale request pruning
  - Full-featured download overlay panel with glassmorphic design

### 🔍 URL Suggestions & Autocomplete
- **Intelligent Address Bar Suggestions**:
  - Popular sites database from `popular_sites.json`
  - Real-time URL matching and autocomplete
  - Suggestion overlay with glassmorphic design
  - Smart caching for performance
  - Favicon integration in suggestions
  - Configurable via settings

### 🔧 Quick Inspector Tool
- **Lightweight Developer Inspector**:
  - Minimal overlay for quick page inspection
  - Runtime DOM/CSS debugging
  - No heavy DevTools overhead
  - Toggle via toolbar icon
  - Glassmorphic overlay design

### ⌨️ Enhanced Keyboard Shortcuts
- **New Shortcuts Added**:
  - `Ctrl+H` - Open History
  - `Ctrl+J` - Open Downloads
  - `Ctrl+,` - Open Settings (Chrome-standard)
  - `Ctrl+L` - Focus Address Bar
  - `Ctrl+T` - New Tab
  - `Ctrl+W` - Close Tab
- JSON-based shortcut mapping system (`shortcuts.json`)
- Menu displays shortcuts next to corresponding items

---

## 🎨 UI/UX Improvements

### Appearance & Design
- **Unified Color Palette**:
  - CSS custom properties for consistent theming
  - Neutral grayscale backgrounds (#1e1e1e → #2d2d2d)
  - Single accent color (#8b7cf5 purple) replacing mixed blues/purples
  - Consistent border opacity system (0.06/0.08/0.12)
  - Three-tier text hierarchy (#e4e4e7, #a1a1aa, #71717a)

- **Settings Panel Redesign**:
  - Complete glassmorphic makeover
  - Backdrop blur effects with semi-transparency
  - Smooth animations and transitions
  - Better visual hierarchy with section grouping
  - Improved toggle switch aesthetics

- **Toolbar & Icons**:
  - Enhanced button tooltips with better positioning
  - "Ad Blocking" renamed to "AdBlock"
  - Improved icon accessibility
  - Vibrant window theme option
  - Transparent toolbar option (experimental)

### Tab System Enhancements
- **Compact Tabs Mode**:
  - Reduced tab height for more screen space
  - Configurable via settings
  - Better tab positioning and spacing
  - Refined add-tab button styling and placement
  - Smooth drag-and-drop improvements

### Animations & Transitions
- **Global Page Transitions**:
  - Smooth fade-in effects for page loads
  - Consistent animation timing across UI
  - Reduced motion option for accessibility
  - Transition effects in HTML assets

---

## 🔒 Privacy & Security

### AdBlock System
- **Enhanced Ad & Tracker Filtering**:
  - Toggle via toolbar icon (shield badge)
  - Settings integration with persistent state
  - Runtime enable/disable without restart
  - Filter synchronization across tabs
  - UI state sync improvements

### Privacy Controls
- **Do Not Track (DNT)** header configuration
- **Clear History on Exit** option
- **Web Security Controls** (JavaScript, cookies, storage)
- **File URL Access** control
- **Cookie Management** settings

---

## 🏗️ Architecture & Build System

### Cross-Platform ARM64 Support
- **Complete ARM64 Build Pipeline**:
  - Linux ARM64 automated builds with aarch64 detection
  - macOS ARM64 (Apple Silicon) automatic SDK selection
  - Improved SDK extraction and root detection logic
  - GCC aarch64 workaround for Linux ARM compilation
  - ARM64-specific CI workflows with fallback to x64

### CI/CD Enhancements
- **GitHub Actions Improvements**:
  - Orchestration workflows for multi-architecture builds
  - Workflow dispatch inputs for custom builds
  - Git config integration in CI
  - Override options for build customization
  - Permission configurations
  - macOS runner updated to latest version
  - Removed pull_request triggers from build workflows

### Package Creation
- **Build Artifacts**:
  - TGZ archives for Linux/macOS
  - DEB packages for Debian/Ubuntu
  - RPM packages for Fedora/RHEL
  - ZIP portable packages for Windows
  - Optional NSIS installer for Windows
  - Optional DMG for macOS

---

## 📄 Static Content & Assets

### Custom Static Pages
- **Google-like Start Page**:
  - Custom static Google homepage design
  - Hosted in `assets/static-sites/`
  - Fallback for offline browsing
  - Improved visual design matching modern Google

### Asset Organization
- **New Asset Files**:
  - `shortcuts.json` - Keyboard shortcut mappings
  - `popular_sites.json` - Popular site suggestions database
  - `blocklist.txt` - Main ad/tracker blocklist
  - `filters/` directory - Additional filter lists (adservers.txt, trackers.txt, urlpatterns.txt)
  - `contextmenu.html` - Context menu overlay
  - `suggestions.html` - URL suggestion overlay
  - `quick-inspector.html` - Developer inspector tool

---

## 🐛 Bug Fixes & Refinements

### Settings System
- Improved settings file handling and migration logic
- Fixed settings persistence across sessions
- Better error handling for malformed JSON
- Runtime settings catalog generation
- Legacy settings migration support

### Download System
- GUID-like filename detection and filtering
- Blob/data URL handling improvements
- Stale download request cleanup
- Progress tracking accuracy improvements
- Download state synchronization

### UI Refinements
- Fixed indentation in HTML assets
- Improved tooltip positioning for toolbar buttons
- Better AdBlock toggle state handling
- Enhanced button accessibility
- Fixed formatting and spacing in tab UI files
- Reformat HTML for consistency

### Build System
- Improved Ultralight SDK root detection
- Better ARM64 SDK handling
- CMake configuration improvements
- CPack packaging enhancements

---

## 📚 Documentation

### README Overhaul
- **Comprehensive Feature Documentation**:
  - Expanded feature list with detailed descriptions
  - Table of contents for better navigation
  - Platform and architecture support matrix
  - ARM64 build notes and requirements
  - JavaScript Bridge API documentation
  - Detailed installation instructions
  - CI/automation documentation
  - Troubleshooting section
  - Contributing guidelines
  - Security and privacy information

### New Documentation Files
- `compile_and_run.bat` - Quick compile and run script for Windows
- Improved `run.bat` for easier execution
- Better code comments throughout

---

## 🔧 Developer Experience

### JavaScript Bridge API (`window.__ul`)
- Enhanced native bridge capabilities
- Better error handling and debugging
- Settings Bridge for runtime configuration
- Local history API improvements
- Shortcut mapping API

### Code Quality
- Improved code formatting and consistency
- Better variable naming conventions
- Enhanced error handling throughout
- More descriptive comments
- TypeScript-style JSDoc comments

---

## 📊 Performance

### Optimizations
- **Intelligent Caching**:
  - Suggestion autocomplete caching
  - Improved memory management
  - Better resource cleanup

- **Rendering Improvements**:
  - Smooth scrolling optimization
  - Hardware acceleration toggles
  - Reduced unnecessary repaints

---

## 🎯 Accessibility

### New Accessibility Features
- **Reduce Motion** setting for users with vestibular disorders
- **High Contrast** mode option
- **Caret Browsing** support
- Better keyboard navigation throughout UI
- ARIA improvements in HTML components

---

## 🗂️ Settings Categories Breakdown

### Before v1.2.0
- Minimal or no persistent settings
- No UI for configuration
- Limited customization options

### After v1.5.0
26 settings across 7 categories with full UI integration and persistence:

1. **Appearance (4 settings)**
   - Dark Theme
   - Vibrant Window Theme
   - Transparent Toolbar (experimental)
   - Compact Tabs (needs restart)

2. **Privacy & Security (8 settings)**
   - Enable AdBlock
   - Block Trackers
   - Enable JavaScript
   - Enable Web Security
   - Enable Cookies
   - Send Do Not Track
   - Clear History on Exit
   - Allow File URLs

3. **Address Bar & Suggestions (2 settings)**
   - Enable Suggestions Autocomplete
   - Show Favicons in Suggestions

4. **Downloads (3 settings)**
   - Show Download Badge
   - Auto-open Downloads Panel
   - Ask Where to Save Downloads

5. **Performance (4 settings)**
   - Enable Smooth Scrolling
   - Enable Hardware Acceleration
   - Enable Local Storage
   - Enable Database

6. **Accessibility (3 settings)**
   - Reduce Motion
   - High Contrast Mode
   - Enable Caret Browsing

7. **Developer (2 settings)**
   - Enable Remote Inspector
   - Show Performance Overlay

---

## 🎨 Visual Changes

### Color Scheme Evolution
- **v1.2.0**: Mixed colors with inconsistent theming
- **v1.5.0**: Unified palette with CSS custom properties
  - Primary background: `#1e1e1e`
  - Secondary: `#252525`
  - Tertiary: `#2a2a2a`
  - Elevated: `#2d2d2d`
  - Accent: `#8b7cf5` (single purple tone)
  - Text hierarchy: `#e4e4e7` / `#a1a1aa` / `#71717a`

### Toggle Switches
- **v1.2.0**: Large, basic toggles
- **v1.5.0**: Refined 40x20px toggles with gradients, smooth animations (cubic-bezier)

---

## 🔄 Migration Notes

### Settings Migration
- Automatic migration from old settings format
- Backwards compatibility maintained
- Default values applied for new settings
- JSON validation and error recovery

### Breaking Changes
⚠️ **None** - All changes are backwards compatible

---

## 📈 Statistics

### Commits Summary (v1.2.0 → v1.5.0)
- **Total Commits**: ~95+
- **Contributors**: 1 (ovsky)
- **Files Changed**: 50+
- **Lines Added**: ~8,000+
- **Lines Removed**: ~2,000+

### Pull Requests Merged
- #12 - ARM64 and x64 build orchestration workflows
- #13 - Custom Static Page
- #14 - Enhance Complete Build Pipeline
- #15 - Add permissions to build-all workflow files
- #17 - URL Suggestion and Autocomplete Feature
- #18 - Enhance Add Tab Button Workflow and Style
- #19 - Create Download System, Download Manager and UI
- #20 - Upgrade Download System
- #21 - Enhance Buttons UI/UX
- #22 - Fix Linux ARM Build

---

## 🏆 Highlights

### Most Impactful Features
1. **Dark Mode System** - Production-ready adaptive theming
2. **Complete Settings System** - 26 configurable options with persistence
3. **Download Manager** - Full-featured download handling
4. **ARM64 Support** - Complete cross-platform build pipeline
5. **URL Suggestions** - Intelligent autocomplete system

### Best UI/UX Improvements
1. Unified glassmorphic design language
2. Consistent color palette with CSS variables
3. Refined toggle switches and controls
4. Keyboard shortcuts with Chrome-standard bindings
5. Smooth animations and transitions

---

## 🔮 Future Roadmap (Post v1.5.0)

### Planned for v1.6+
- Extensions system support
- Bookmark management
- Session restore
- Picture-in-Picture mode
- Reading mode
- Password manager integration
- Sync capabilities

---

## 🙏 Acknowledgements

Special thanks to:
- [Ultralight SDK Team](https://ultralig.ht/) for the amazing renderer
- All beta testers and early adopters
- Community contributors for feedback and bug reports

---

## 📝 Notes

### Testing
This release has been tested on:
- ✅ Windows 10/11 (x64)
- ✅ macOS 12+ (x64, arm64)
- ✅ Ubuntu 20.04/22.04 (x64)
- ✅ Fedora 38+ (x64)
- ✅ ARM64 Linux (limited testing)


## 📦 Download v1.5.0

### Official Release
🎉 [Download from GitHub Releases](https://github.com/ovsky/Ultralight-WebBrowser/releases/tag/v1.5.0)

### Supported Packages
- **Windows**: ZIP (portable), NSIS Installer
- **macOS**: TGZ, DMG (x64 & arm64)
- **Linux**: TGZ, DEB, RPM (x64 & arm64)

---

**Full Changelog**: [v1.2.0...v1.5.0](https://github.com/ovsky/Ultralight-WebBrowser/compare/v1.2.0...v1.5.0)
