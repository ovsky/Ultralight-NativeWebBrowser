

# Ultralight - Native Web Browser 

[![Build - Windows](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-windows.yml/badge.svg?branch=dev)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-windows.yml)
[![Build - Linux](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-linux.yml/badge.svg?branch=dev)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-linux.yml)
[![Build - macOS](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-macos.yml/badge.svg?branch=dev)](https://github.com/ovsky/Ultralight-WebBrowser/actions/workflows/build-macos.yml)

#### [Ultra-Fast + Ultra-Light + Ultra-Portable]
ㅤ

<img src="https://github.com/ultralight-ux/Ultralight/raw/master/media/logo.png" width="200">

ㅤ

**Have you ever thought about a browser being just... a browser?**

No background services, no 2GB RAM usage for a single tab, no 5-second startup times? 

Here it is: **Ultralight - Web Browser**!

---

This project is an fully open-source experimental exploration of the fully de-bloated and fastest possible browser idea.

It's a lightweight, native C++ web browser built using the [**Ultralight SDK**](https://ultralig.ht/ "null") for all HTML/CSS/JS rendering. It is not a commercial product but a powerful proof-of-concept demonstrating that high-performance, low-resource browsing is achievable today!

![Ultralight Browser Preview](https://github.com/ovsky/Ultralight-NativeWebBrowser/blob/main/data/screenshots/preview.png)

---

## 🚀 Why Ultralight? Ditch the Bloat.

Traditional web browsers and embedded frameworks (like Electron or CEF) are built on monolithic browser engines like Chromium. While powerful, they are designed as massive, sandboxed operating systems, bringing significant overhead in memory, CPU, and disk space.

![Ultralight Memory Usage](https://ultralig.ht/media/base-memory-usage.webp)

This browser takes a different approach by using **Ultralight**, a next-generation HTML renderer designed from the ground up for speed, efficiency, and seamless integration into native apps.

**Here’s how it compares to the status quo:**

---
| **Feature** | **Ultralight (This Browser)** | **Electron / CEF**  |
|-- |--|--| 
| **Performance** |  **Up to 6x faster** | Standard Chromium |
| **Memory Usage** | **~1/10th the RAM** | High (Full browser instance) |
| **Startup Time** | Near-instant (<1 sec) | Noticeable delay (3+ sec) |
| **Storage** | ~30 MB footprint | ~1000+ MB footprint |
| **Rendering** | GPU-accelerated, lightweight | GPU-accelerated, heavyweight |
| **Architecture** | Native C++, renders to a pixel buffer | JS Bridge + Node.js + Chromium |


By leveraging Ultralight's high-performance, GPU-accelerated renderer, this application achieves a level of resource efficiency that is simply not possible with other frameworks. It's designed to be a component, not an OS.

## 🎯 Project Philosophy & Goals

This repository serves as a practical, open-source example for developers. The primary goals are:

-   **Speed:** Demonstrate near-instant application startup and fluid, responsive rendering.
    
-   **Efficiency:** Showcase the minimal resource footprint (RAM/CPU) of the Ultralight engine in a real-world use case.
    
-   **Integration:** Provide a clear C++ example of how to embed a web renderer into a native application (using GLFW/OpenGL) and build a tabbed UI around it.
    
-   **Simplicity:** Keep the browser's own C++ code minimal and readable, focusing on the _integration_ rather than bolting on complex, non-essential features.
    

## ✨ Features

-   **Blazing Fast Rendering:** Thanks to the Ultralight core, pages load and respond quickly.
    
-   **Extremely Low Memory Footprint:** Browse more without your system grinding to a halt.
    
-   **Modern Web Support:** Renders HTML5, CSS3, and modern JavaScript (ES6+).
    
-   **Native C++ Core:** No JavaScript bridge, no Node.js backend. Just pure native speed for the UI and app logic.
    
-   **Core Browser UI:**
    
    -   Multi-tab interface
        
    -   Address bar with navigation (Go, Back, Forward, Reload)
        
    -   Loading indicators and page title display
        
    -   Responsive window resizing
        

## 🛠️ Tech Stack

-   **Core Renderer:** [Ultralight SDK](https://ultralig.ht/ "null")
    
-   **Language:** C++17
    
-   **Windowing & Input:** [GLFW](https://www.glfw.org/ "null")
    
-   **Graphics:** OpenGL 3.3
    
-   **Build System:** CMake
    

## 📦 Building from Source

### Prerequisites

1.  **CMake** (3.10 or higher)
    
2.  A C++17 compliant compiler (e.g., Visual Studio 2019+, GCC 9+, Clang 9+)
    
3.  **Ultralight SDK:**
    
    -   [Download the latest SDK](https://ultralig.ht/download/ "null") for your OS (e.g., Windows x64).
        
    -   Unzip the SDK to a stable location on your drive (e.g., `C:\dev\ultralight-sdk`).
        
    -   Create an environment variable named `ULTRALIGHT_SDK_DIR` and set its value to the path of the unzipped SDK folder.
        

### Build Steps

```bash
# 1. Clone the repository
git clone [https://github.com/ovsky/Ultralight-NativeWebBrowser.git](https://github.com/ovsky/Ultralight-NativeWebBrowser.git)
cd Ultralight-NativeWebBrowser

# 2. Initialize submodules (for GLFW)
# If you cloned without --recursive, run this:
git submodule update --init --recursive

# 3. Create a build directory
mkdir build
cd build

# 4. Configure with CMake
# (Make sure ULTRALIGHT_SDK_DIR is set in your environment)
cmake ..

# 5. Compile the project

# On Windows (Visual Studio)
cmake --build . --config Release

# On MacOS/Linux
cmake --build .

```

The final executable will be located in the `build/bin/` (or `build/Release/`) directory.

## 🗺️ Future Ideas

This is a proof-of-concept, but there's plenty of room for improvement. Pull requests are welcome!

-   [ ] Bookmark System
    
-   [ ] Local Browsing History
    
-   [ ] Basic ad/tracker blocking (perhaps via a hosts file or filter list)
    
-   [ ] Deeper integration with Ultralight's JS C++ bridge (expose native functions to JS)
    
-   [ ] Improved UI/UX (a settings panel, right-click context menus)
    
    

## 🤝 How to Contribute

Contributions are what make the open-source community such an amazing place. Any contributions you make are **greatly appreciated**.

1.  Fork the Project
    
2.  Create your Feature Branch (`git checkout -b feature/some-feature`)
    
3.  Commit your Changes (`git commit -m 'Add some feature'`)
    
4.  Push to the Branch (`git push origin feature/some-feature`)
    
5.  Open a Pull Request
    

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.

## 🙏 Acknowledgements

-   This project would be impossible without the incredible work of the [Ultralight](https://ultralig.ht/ "null") ❤ team.
    
-   [GLFW](https://www.glfw.org/ "null") for simple, cross-platform windowing and input.
