;DRM Sidecar Integration

This sample includes a hybrid DRM "sidecar" subsystem to host a Chromium/CEF-based renderer for Widevine EME-enabled content.

Files added:
- `src/DrmSidecarManager.h` / `.cpp` - cross-platform manager for installing, launching and resizing a sidecar child process.
- `src/sidecar_main.cpp` - a small stubbed sidecar entry point that parses `--url` and `--parent-window-id` arguments and demonstrates where to initialize CEF.
- UI wiring: `assets/ui.html` and `src/UI.*` were updated to provide a toolbar button and JS bridge to install/launch the sidecar.
- `CMakeLists.txt` now has an option `-DBUILD_SIDECAR=ON` to build the stub `sidecar` target.

Install / Build

1. To build the sidecar stub locally using CMake (optional):

```pwsh
cmake -S . -B build -DBUILD_SIDECAR=ON
cmake --build build --config Release
```

3. To build the sidecar linked against a local CEF SDK:

```pwsh
# Point CMake at your CEF SDK root and enable CEF support
cmake -S . -B build -DBUILD_SIDECAR=ON -DUSE_CEF=ON -DCEF_ROOT="C:/path/to/cef"
cmake --build build --config Release
```

Notes: the `CEF_ROOT` path must contain `include/cef_app.h` and the CEF import libraries
under `lib/` or a similar subdirectory. Linking names and layout vary between CEF packages —
you may need to adjust `CMakeLists.txt` or provide full paths to the CEF import libraries.

Auto-downloading CEF (Windows)

This project can auto-download a prebuilt CEF archive on Windows to simplify
development. Auto-download is disabled by default to avoid large, unexpected
downloads; enable it with CMake cache options. Always verify downloaded
artifacts before using them in production.

Basic example (pick a real, reachable URL — placeholder URLs will 404):

```pwsh
cmake -S . -B build -DBUILD_SIDECAR=ON -DCEF_AUTO_DOWNLOAD=ON -DCEF_AUTO_URL="https://example.com/cef_windows_bundle.zip"
cmake --build build --config Release
```

Recommended: provide an integrity hash to verify the download. Use the
`CEF_AUTO_URL_HASH` option and supply either a raw 64-character SHA256 hex or
the `ALGO=hex` form (e.g. `SHA256=<hex>`):

```pwsh
cmake -S . -B build -DBUILD_SIDECAR=ON -DCEF_AUTO_DOWNLOAD=ON -DCEF_AUTO_URL="https://host/cef.zip" -DCEF_AUTO_URL_HASH=SHA256=<64hex>
cmake --build build --config Release
```

Notes:
- `CEF_AUTO_URL` must point to a direct-download archive (ZIP/tar) containing
	a CEF SDK layout with `include/cef_app.h` and the import libraries under `lib/`.
- The download/extract step uses CMake's `ExternalProject_Add` and places the
	extracted files under the build directory (e.g. `build/cef_auto/src/cef_auto`).
- The CMake script performs strict validation on `CEF_AUTO_URL_HASH` and will
	reject placeholders or malformed hashes — provide a real 64-character SHA256
	hex or an explicit `ALGO=hex` value.
- This is a convenience feature only — for production use you should manage CEF
	artifacts explicitly and verify their integrity and signatures before use.

Helper script:
- A convenience PowerShell helper is included at `scripts/download_cef.ps1`.
	It downloads a CEF archive, computes its SHA256, and prints the exact CMake
	command you should run (including `-DCEF_AUTO_URL_HASH=SHA256=<hex>`). Example:

```pwsh
.\scripts\download_cef.ps1 -Url "https://host/cef_windows_bundle.zip"
# after it prints the SHA256, run the printed cmake command to configure
```

2. The `DrmSidecarManager::InstallSidecar` will attempt to download a ZIP from a GitHub Releases URL generated from OS/arch:

`https://github.com/yourorg/ultralight-sidecar/releases/latest/download/sidecar-<os-arch>.zip`

Replace `yourorg/ultralight-sidecar` with your release repository. The download step in the manager currently uses `curl` on UNIX or PowerShell `Invoke-WebRequest` on Windows as a pragmatic bootstrap. In production, replace this with a proper libcurl implementation and verify signatures/hashes.

Usage from UI

- Click the new "DRM Sidecar" toolbar icon (single-click to install, double-click to launch for the current address).
- The "Enable DRM Sidecar" setting is available in the settings UI (Experimental / Privacy category) and is persisted.

Security and production notes

- ALWAYS verify downloaded binaries (use code signing or checksums).
- The sidecar should be built from a trusted CEF/Chromium distribution including Widevine CDM. This example only provides process and window embedding scaffolding.
- Consider using a more robust IPC channel (sockets / pipes) for status and control messages instead of ad-hoc window messages.

If you want, I can:
- Replace the system download calls with `libcurl` + `libarchive` extraction.
- Implement real CEF initialization/embedding in `sidecar_main.cpp` for Windows and Linux (requires linking against CEF).
- Add verification (SHA256 + signature) of downloaded archives.

