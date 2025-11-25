cmake_minimum_required(VERSION 3.8)
project(UltralightWebBrowser LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SIDECAR "Build the DRM sidecar helper executable (CEF wrapper stub)" OFF)
find_package(CURL QUIET)
if(CURL_FOUND)
  message(STATUS "libcurl found: enabling libcurl downloader support")
  add_compile_definitions(HAVE_LIBCURL)
  set(HAVE_LIBCURL ON)
else()
  message(STATUS "libcurl not found: sidecar installer will fall back to system download commands")
  set(HAVE_LIBCURL OFF)
endif()

# Optional: CEF integration for the sidecar executable. Leave OFF by default since
# the CEF SDK is large and not included in this repository. To enable, set
# `-DUSE_CEF=ON -DCEF_ROOT=/path/to/cef` when configuring CMake. The sidecar
# target will then attempt to link against CEF libraries under `CEF_ROOT`.
option(USE_CEF "Link the sidecar against a local CEF SDK (must set CEF_ROOT)" ON)
set(CEF_ROOT "" CACHE PATH "Path to local CEF SDK root (contains include/ and lib/)")

# If the user didn't provide a CEF_ROOT explicitly, prefer a locally-extracted
# copy under `third_party/cef/extracted` when present (convenient for offline
# workflows where the archive has already been downloaded and extracted).
if(NOT CEF_ROOT)
  set(_default_cef_extracted "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef/extracted")
  if(EXISTS "${_default_cef_extracted}/include")
    # Basic sanity: presence of include and either Windows or Unix runtime libs
    if(EXISTS "${_default_cef_extracted}/Release/libcef.dll" OR EXISTS "${_default_cef_extracted}/lib/libcef.so")
      set(CEF_ROOT "${_default_cef_extracted}" CACHE PATH "Path to local CEF SDK root (contains include/ and lib/)" FORCE)
      message(STATUS "Using extracted CEF SDK at: ${CEF_ROOT}")
    endif()
  endif()
endif()

# Optional: automatically download a prebuilt CEF binary bundle on Windows.
# Set -DCEF_AUTO_DOWNLOAD=ON -DCEF_AUTO_URL="https://.../cef_binary_windows.zip"
# to enable. The ExternalProject will extract into the build directory and set
# the internal variable `CEF_AUTO_ROOT` which is used as the CEF_ROOT for
# subsequent configuration.
option(CEF_AUTO_DOWNLOAD "Automatically download a prebuilt CEF binary bundle (Windows only)" OFF)
set(CEF_AUTO_URL "" CACHE STRING "URL to prebuilt CEF archive to download when CEF_AUTO_DOWNLOAD=ON (leave empty to provide your own URL)")
set(CEF_AUTO_URL_HASH "" CACHE STRING "Optional SHA256 (or other) hash for CEF_AUTO_URL in the form ALGO=hexvalue, e.g. SHA256=<hex>. If empty, no URL_HASH is passed to ExternalProject_Add.")



  # (Browser resource file will be configured after the icon-detection block
  # later in this file using configure_file(... @ONLY).)

# Option: create an installer on Windows via CPack (NSIS)
option(CREATE_INSTALLER "Create a Windows installer via CPack (NSIS)" OFF)

# --- Testing (CTest) ---
include(CTest)

# --- Ultralight SDK root ---
# Prefer a user-provided path, otherwise fall back to the local 'data' folder in this sample.
# Structure expected under ULTRALIGHT_SDK_ROOT:
#   include/ (AppCore, Ultralight, WebCore headers)
#   lib/     (import libraries)
#   bin/     (runtime DLLs and ICU folder)
set(ULTRALIGHT_SDK_ROOT "" CACHE PATH "Path to Ultralight SDK root (contains include, lib, bin)")

# Optional: automatically download a prebuilt Ultralight SDK into the build
# directory when no local `ULTRALIGHT_SDK_ROOT` is provided. This is useful
# for new users who don't have the SDK locally. Set -DULTRALIGHT_AUTO_DOWNLOAD=ON
# and -DULTRALIGHT_AUTO_URL="https://.../ultralight_sdk.zip" to enable.
option(ULTRALIGHT_AUTO_DOWNLOAD "Automatically download a prebuilt Ultralight SDK when ULTRALIGHT_SDK_ROOT not provided" OFF)
set(ULTRALIGHT_AUTO_URL "" CACHE STRING "URL to prebuilt Ultralight SDK archive to download when ULTRALIGHT_AUTO_DOWNLOAD=ON")
set(ULTRALIGHT_AUTO_URL_HASH "" CACHE STRING "Optional hash for ULTRALIGHT_AUTO_URL in the form ALGO=hexvalue or raw hex (assumed SHA256). If empty, no URL_HASH is passed to ExternalProject_Add.")

# Default data folder shipped with the sample (may contain a minimal SDK)
set(_default_ultralight_data "${CMAKE_CURRENT_SOURCE_DIR}/data")

# If user didn't set ULTRALIGHT_SDK_ROOT, try the default data folder first.
if(NOT ULTRALIGHT_SDK_ROOT)
  if(EXISTS "${_default_ultralight_data}/include" AND EXISTS "${_default_ultralight_data}/lib")
    set(ULTRALIGHT_SDK_ROOT "${_default_ultralight_data}")
  else()
    # No local SDK found under the sample data. Optionally attempt auto-download.
    if(ULTRALIGHT_AUTO_DOWNLOAD)
      if(NOT ULTRALIGHT_AUTO_URL)
        message(FATAL_ERROR "ULTRALIGHT_AUTO_DOWNLOAD is ON but ULTRALIGHT_AUTO_URL is empty. Set -DULTRALIGHT_AUTO_URL=... to a downloadable Ultralight SDK archive URL.")
      endif()

    endif()
  endif()
endif()

# Ensure we have an Ultralight SDK available or auto-download enabled.
# If the user has not provided `ULTRALIGHT_SDK_ROOT` and automatic download
# is disabled, fail early with a clear message describing how to proceed.
if(NOT ULTRALIGHT_AUTO_DOWNLOAD)
  if(NOT ULTRALIGHT_SDK_ROOT OR NOT EXISTS "${ULTRALIGHT_SDK_ROOT}/lib"
     OR NOT EXISTS "${ULTRALIGHT_SDK_ROOT}/include/AppCore/AppCore.h"
     OR NOT EXISTS "${ULTRALIGHT_SDK_ROOT}/include/Ultralight/Listener.h")
    message(FATAL_ERROR "No Ultralight SDK found or the SDK appears incomplete.\n\nProvide a valid SDK path: -DULTRALIGHT_SDK_ROOT=\"/path/to/ultralight_sdk\"\n (the path must contain 'include/AppCore/AppCore.h' and 'include/Ultralight/Listener.h' and a 'lib/' folder).\n\nOr enable automatic download: -DULTRALIGHT_AUTO_DOWNLOAD=ON -DULTRALIGHT_AUTO_URL=\"<url>\" -DULTRALIGHT_AUTO_URL_HASH=\"SHA256=<hex>\"\n")
  endif()
endif()

function(add_app NAME)
      if (WIN32)
        add_executable(${NAME} WIN32 ${ARGN})
      else()
        add_executable(${NAME} ${ARGN})
      endif()

      # Disable RTTI
      if(MSVC)
        target_compile_options(${NAME} PRIVATE /GR-)
      else()
        target_compile_options(${NAME} PRIVATE -fno-rtti)
      endif()

      # Locate Ultralight libraries cross-platform
      find_library(APP_CORE_LIB NAMES AppCore PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)
      find_library(ULTRALIGHT_LIB NAMES Ultralight PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)
      find_library(ULTRALIGHT_CORE_LIB NAMES UltralightCore PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)
      find_library(WEB_CORE_LIB NAMES WebCore PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)

      if(NOT APP_CORE_LIB OR NOT ULTRALIGHT_LIB OR NOT ULTRALIGHT_CORE_LIB OR NOT WEB_CORE_LIB)
        message(FATAL_ERROR "Failed to locate Ultralight libraries under ${ULTRALIGHT_SDK_ROOT}. Found:\n AppCore='${APP_CORE_LIB}'\n Ultralight='${ULTRALIGHT_LIB}'\n UltralightCore='${ULTRALIGHT_CORE_LIB}'\n WebCore='${WEB_CORE_LIB}'")
      endif()

      target_link_libraries(${NAME} PRIVATE
        ${APP_CORE_LIB}
        ${ULTRALIGHT_LIB}
        ${ULTRALIGHT_CORE_LIB}
        ${WEB_CORE_LIB}
      )

      # Set RPATH so the app can find the copied shared libs on macOS/Linux
      if(APPLE OR UNIX)
        set_target_properties(${NAME} PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
      endif()
    endfunction()

# --- Original project definition ---
set(SOURCES "src/Browser.h"
            "src/Browser.cpp"
            "src/AdBlocker.h"
            "src/AdBlocker.cpp"
            "src/DownloadManager.h"
            "src/DownloadManager.cpp"
            "src/DrmSidecarManager.h"
            "src/DrmSidecarManager.cpp"
            "src/Tab.h"
            "src/Tab.cpp"
            "src/UI.h"
            "src/UI.cpp"
            "src/main.cpp"
            "${CMAKE_CURRENT_BINARY_DIR}/browser.rc"
)

add_app(Ultralight-WebBrowser ${SOURCES})

if(ULTRALIGHT_SDK_ROOT)
  # Add the Ultralight SDK include directory so source files can find headers
  target_include_directories(Ultralight-WebBrowser PRIVATE "${ULTRALIGHT_SDK_ROOT}/include")
endif()

if(HAVE_LIBCURL)
  # Link libcurl to the main app so DrmSidecarManager can use it.
  target_link_libraries(Ultralight-WebBrowser PRIVATE CURL::libcurl)
endif()

# If we auto-downloaded the Ultralight SDK, ensure the main app depends on the
# download target so the SDK is available before attempting to link against it.
if(ULTRALIGHT_AUTO_DOWNLOAD)
  add_dependencies(Ultralight-WebBrowser ultralight_auto)
endif()

if(CEF_AUTO_DOWNLOAD)
  if(NOT WIN32)
    message(FATAL_ERROR "CEF_AUTO_DOWNLOAD is currently supported on Windows only.")
  endif()
  # If user did not provide an explicit URL, attempt to auto-detect the latest
  # Windows x64 build from the public CEF builds index using a helper script.
  if(NOT CEF_AUTO_URL)
    find_program(_POWERSHELL pwsh.exe)
    if(NOT _POWERSHELL)
      find_program(_POWERSHELL powershell.exe)
    endif()
    if(_POWERSHELL)
      execute_process(
        COMMAND ${_POWERSHELL} -NoProfile -NonInteractive -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/find_cef_build.ps1"
        RESULT_VARIABLE _cef_detect_res
        OUTPUT_VARIABLE _cef_detect_url
        ERROR_VARIABLE _cef_detect_err
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(_cef_detect_res EQUAL 0 AND _cef_detect_url)
        message(STATUS "Auto-detected CEF URL: ${_cef_detect_url}")
        set(CEF_AUTO_URL "${_cef_detect_url}" CACHE STRING "Auto-detected CEF URL" FORCE)
      else()
        message(WARNING "Could not auto-detect CEF URL: ${_cef_detect_err}. Provide -DCEF_AUTO_URL=... to continue.")
      endif()
    else()
      message(WARNING "No PowerShell available to auto-detect CEF builds. Provide -DCEF_AUTO_URL=...")
    endif()
    if(NOT CEF_AUTO_URL)
      message(FATAL_ERROR "CEF_AUTO_DOWNLOAD is ON but CEF_AUTO_URL is empty. Set -DCEF_AUTO_URL=... to a downloadable CEF archive URL.")
    endif()
  endif()

  include(ExternalProject)

  # Download and extract CEF into the build tree under cef_auto
  # Allow an optional URL hash (provided by CEF_AUTO_URL_HASH). If the user
  # did not provide a hash, do not pass URL_HASH to ExternalProject_Add since
  # CMake requires a valid ALGO=hex value.
  set(_cef_auto_download_args
    URL ${CEF_AUTO_URL}
    PREFIX ${CMAKE_BINARY_DIR}/cef_auto
    DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/_deps/cef_auto/download
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON)

  if(CEF_AUTO_URL_HASH)
    # Normalize user-provided hash. Accept two forms:
    #  - ALGO=hexvalue (must be ALGO in allowed list and hexvalue valid)
    #  - raw hexvalue (assumed SHA256 when 64 hex chars)
    string(STRIP "${CEF_AUTO_URL_HASH}" _tmp_hash)

    # If the value is a plain hex string (only 0-9a-fA-F), accept it as SHA256
    string(REGEX MATCH "^[0-9A-Fa-f]{64}$" _is_64hex "${_tmp_hash}")
    if(_is_64hex)
      set(_normalized_hash "SHA256=${_tmp_hash}")
    else()
      # Expect ALGO=hex format. Split and validate.
      string(FIND "${_tmp_hash}" "=" _eq_pos)
      if(_eq_pos EQUAL -1)
        message(FATAL_ERROR "CEF_AUTO_URL_HASH must be either a 64-character SHA256 hex string or in the form ALGO=hexvalue (e.g. SHA256=<hex>). Got: '${CEF_AUTO_URL_HASH}'")
      endif()
      string(SUBSTRING "${_tmp_hash}" 0 ${_eq_pos} _algo)
      math(EXPR _val_start "${_eq_pos} + 1")
      string(SUBSTRING "${_tmp_hash}" ${_val_start} -1 _hexval)
      string(STRIP "${_algo}" _algo)
      string(STRIP "${_hexval}" _hexval)

      # Validate allowed algorithms
      string(TOUPPER "${_algo}" _algo_up)
      set(_allowed_algos "MD5;SHA1;SHA224;SHA256;SHA384;SHA512;SHA3_224;SHA3_256;SHA3_384;SHA3_512")
      list(FIND _allowed_algos "${_algo_up}" _found_algo)
      if(_found_algo EQUAL -1)
        message(FATAL_ERROR "CEF_AUTO_URL_HASH algorithm '${_algo}' is not supported. Use one of: ${_allowed_algos}")
      endif()

      # Validate hex characters (allow any length here but ExternalProject will expect proper length per algo)
      string(REGEX MATCH "^[0-9A-Fa-f]+$" _is_hex "${_hexval}")
      if(NOT _is_hex)
        message(FATAL_ERROR "CEF_AUTO_URL_HASH hex value contains invalid characters. Provide only hex digits (0-9, A-F). Got: '${_hexval}'")
      endif()

      # If SHA256, ensure correct length.
      if(_algo_up STREQUAL "SHA256")
        string(LENGTH "${_hexval}" _hex_len)
        if(NOT _hex_len EQUAL 64)
          message(FATAL_ERROR "CEF_AUTO_URL_HASH with SHA256 must be 64 hex characters (got ${_hex_len}).")
        endif()
      endif()

      set(_normalized_hash "${_algo_up}=${_hexval}")
    endif()

    list(APPEND _cef_auto_download_args URL_HASH ${_normalized_hash})
  endif()

  # Create a simple build-time download+extract target for CEF so we do not
  # attempt to configure/build/install the extracted tree. This downloads the
  # archive at build time and extracts it into ${CEF_AUTO_ROOT}.
  find_program(_POWERSHELL pwsh.exe QUIET)
  if(NOT _POWERSHELL)
    find_program(_POWERSHELL powershell.exe QUIET)
  endif()
  if(NOT _POWERSHELL)
    message(FATAL_ERROR "CEF_AUTO_DOWNLOAD requires PowerShell (pwsh.exe or powershell.exe) to perform the download on Windows.")
  endif()

  set(CEF_AUTO_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps/cef_auto/download")
  set(CEF_AUTO_ARCHIVE "${CEF_AUTO_DOWNLOAD_DIR}/${CEF_AUTO_URL_PATH}" CACHE FILEPATH "CEF downloaded archive path")
  set(CEF_AUTO_ROOT "${CMAKE_BINARY_DIR}/cef_auto/src/cef_auto" CACHE PATH "CEF auto-download root")

  # Ensure a simple filename for the downloaded archive
  get_filename_component(_cef_filename "${CEF_AUTO_URL}" NAME)
  string(REPLACE "%2B" "+" _cef_filename_unescaped "${_cef_filename}")
  set(CEF_AUTO_ARCHIVE "${CEF_AUTO_DOWNLOAD_DIR}/${_cef_filename_unescaped}")

  add_custom_command(
    OUTPUT "${CEF_AUTO_ROOT}/.cef_extracted"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CEF_AUTO_DOWNLOAD_DIR}"
    COMMAND ${_POWERSHELL} -NoProfile -NonInteractive -Command "try { Invoke-WebRequest -Uri '${CEF_AUTO_URL}' -OutFile '${CEF_AUTO_ARCHIVE}' -UseBasicParsing } catch { Write-Error \"CEF download failed\"; exit 1 }"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CEF_AUTO_ROOT}"
    # Prefer using the system 'tar' via PowerShell so we can strip the top-level
    # directory that many archives include (ensures headers end up at ${CEF_AUTO_ROOT}/include).
    COMMAND ${_POWERSHELL} -NoProfile -NonInteractive -Command "try { tar -xjf '${CEF_AUTO_ARCHIVE}' -C '${CEF_AUTO_ROOT}' --strip-components=1 } catch { Write-Error 'Extraction failed'; exit 1 }"
    COMMAND ${CMAKE_COMMAND} -E touch "${CEF_AUTO_ROOT}/.cef_extracted"
    COMMENT "Downloading and extracting CEF archive"
  )

  add_custom_target(cef_auto ALL DEPENDS "${CEF_AUTO_ROOT}/.cef_extracted")

  # Force-set CEF_ROOT to the auto root so the build knows where to look
  set(CEF_ROOT ${CEF_AUTO_ROOT} CACHE PATH "Path to local CEF SDK root (contains include/ and lib/)" FORCE)

  # The sidecar target will be created after this block; if auto-download
  # is enabled we will add the dependency then so the download occurs before
  # attempting to link against CEF.
endif()

# --- Sidecar target (created after optional auto-download/extract) ---
if(BUILD_SIDECAR)
  # Build a small sidecar executable. If USE_CEF=ON and a valid CEF_ROOT is
  # provided (or auto-downloaded above), the sidecar will attempt to link
  # against CEF and enable `HAVE_CEF` so the sidecar code will call into CEF
  # APIs. Otherwise the sidecar builds the lightweight stub implementation.
  add_executable(sidecar src/sidecar_main.cpp)
  if(MSVC)
    target_compile_options(sidecar PRIVATE /EHsc)
  else()
    target_compile_options(sidecar PRIVATE -fno-rtti)
  endif()

  if(USE_CEF)
    if(NOT CEF_ROOT)
      message(FATAL_ERROR "USE_CEF is ON but CEF_ROOT was not provided and auto-download did not set a CEF root. Set -DCEF_ROOT=/path/to/cef or enable CEF_AUTO_DOWNLOAD with a valid URL.")
    endif()
    # Sanity check for CEF headers
    if(CEF_AUTO_DOWNLOAD)
      message(STATUS "CEF auto-download is enabled; skipping configure-time check for headers. The build will download and extract CEF before building the sidecar.")
    else()
      if(NOT EXISTS "${CEF_ROOT}/include/cef_app.h")
        message(FATAL_ERROR "CEF headers not found under ${CEF_ROOT}/include. Please point CEF_ROOT to a valid CEF SDK root.")
      endif()
    endif()

    message(STATUS "Configuring sidecar; attempting to locate CEF under: ${CEF_ROOT}")
    # Some CEF headers use paths like "include/cef_base.h" so add both
    # the SDK root and the SDK include directory to the include path.
    target_include_directories(sidecar PRIVATE "${CEF_ROOT}" "${CEF_ROOT}/include")

    # Clear any previous compile definitions that might have been cached
    # (ensures stale HAVE_CEF=1 doesn't persist across reconfigures).
    set_target_properties(sidecar PROPERTIES COMPILE_DEFINITIONS "")

    # Try to locate common CEF import libraries. If we cannot find import
    # libraries, we will build the sidecar as a non-CEF stub (HAVE_CEF not set).
    find_library(LIBCEF NAMES libcef libcef.lib libcef_dll libcef_dll.lib PATHS
      "${CEF_ROOT}/lib" "${CEF_ROOT}/Release" "${CEF_ROOT}/Release/lib" NO_DEFAULT_PATH)
    find_library(LIBCEF_WRAPPER NAMES libcef_dll_wrapper libcef_dll_wrapper.lib libcef_dll_wrapper.a PATHS
      "${CEF_ROOT}/lib" "${CEF_ROOT}/Release" "${CEF_ROOT}/Release/lib" NO_DEFAULT_PATH)

    if(NOT LIBCEF AND NOT LIBCEF_WRAPPER)
      message(WARNING "Could not find CEF import libraries under ${CEF_ROOT}. The sidecar will be built as a non-CEF stub. To enable CEF, provide a valid CEF_ROOT or run the 'cef_auto' download target.")
    else()
      # Ensure headers appear usable before enabling CEF codepaths. Some archives
      # extract with a layout that places headers under a nested include/ directory
      # or require additional extraction steps. Verify `cef_app.h` is present.
      if(NOT EXISTS "${CEF_ROOT}/include/cef_app.h")
        message(WARNING "CEF import libraries found, but CEF headers not found under ${CEF_ROOT}/include. Building sidecar as non-CEF stub to avoid compile errors.")
      else()
      # Link libraries
      if(LIBCEF)
        target_link_libraries(sidecar PRIVATE ${LIBCEF})
      endif()
      if(LIBCEF_WRAPPER)
        target_link_libraries(sidecar PRIVATE ${LIBCEF_WRAPPER})
      endif()

      # Before enabling CEF codepaths, verify headers + compile with the
      # current include/library layout to avoid surprising compile failures.
      set(_cef_test_src "${CMAKE_BINARY_DIR}/CMakeFiles/cef_test_compile.cpp")
      file(WRITE ${_cef_test_src} "#include \"${CEF_ROOT}/include/cef_app.h\"\nint main(){return 0;}\n")
      try_compile(_cef_compiles
        ${CMAKE_BINARY_DIR}
        ${_cef_test_src}
        LINK_LIBRARIES ${LIBCEF}
        CMAKE_FLAGS "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}" "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}")

      if(NOT _cef_compiles)
        message(WARNING "CEF headers/libraries appear unusable for compilation in this environment. Building sidecar as non-CEF stub to avoid errors.")
      else()
        # Expose a compile-time macro so the sidecar source can enable CEF codepaths
        target_compile_definitions(sidecar PRIVATE HAVE_CEF=1)
      endif()

      # Copy runtime CEF artifacts next to the built sidecar binary so it can run
      # without additional system setup. This attempts to copy common files and
      # directories such as the CEF DLL, resources, icudtl.dat, and locales.
      if(WIN32)
        if(EXISTS "${CEF_ROOT}/Release/libcef.dll")
          add_custom_command(TARGET sidecar POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CEF_ROOT}/Release/libcef.dll" "$<TARGET_FILE_DIR:sidecar>/libcef.dll"
          )
        endif()
        if(EXISTS "${CEF_ROOT}/Release/icudtl.dat")
          add_custom_command(TARGET sidecar POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CEF_ROOT}/Release/icudtl.dat" "$<TARGET_FILE_DIR:sidecar>/icudtl.dat"
          )
        endif()
        if(EXISTS "${CEF_ROOT}/Resources")
          add_custom_command(TARGET sidecar POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${CEF_ROOT}/Resources" "$<TARGET_FILE_DIR:sidecar>/Resources"
          )
        endif()
        if(EXISTS "${CEF_ROOT}/locales")
          add_custom_command(TARGET sidecar POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${CEF_ROOT}/locales" "$<TARGET_FILE_DIR:sidecar>/locales"
          )
        endif()
      else()
        if(EXISTS "${CEF_ROOT}/lib/libcef.so")
          add_custom_command(TARGET sidecar POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CEF_ROOT}/lib/libcef.so" "$<TARGET_FILE_DIR:sidecar>/libcef.so"
          )
        endif()
        if(EXISTS "${CEF_ROOT}/libicudtl.dat")
          add_custom_command(TARGET sidecar POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${CEF_ROOT}/libicudtl.dat" "$<TARGET_FILE_DIR:sidecar>/icudtl.dat"
          )
        endif()
        if(EXISTS "${CEF_ROOT}/Resources")
          add_custom_command(TARGET sidecar POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${CEF_ROOT}/Resources" "$<TARGET_FILE_DIR:sidecar>/Resources"
          )
        endif()
        if(EXISTS "${CEF_ROOT}/locales")
          add_custom_command(TARGET sidecar POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${CEF_ROOT}/locales" "$<TARGET_FILE_DIR:sidecar>/locales"
          )
        endif()
      endif()
      endif()
    endif()
  endif()

  # If we auto-downloaded CEF, make the download step a dependency so it runs before building sidecar
  if(CEF_AUTO_DOWNLOAD)
    add_dependencies(sidecar cef_auto)
  endif()

  install(TARGETS sidecar RUNTIME DESTINATION ${INSTALL_DIR_NAME})
endif()

# Work around GCC 11 ICE on aarch64 when compiling UI.cpp at higher optimisation levels
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  message(STATUS "Applying GCC/aarch64 workaround: forcing -O1 for src/UI.cpp")
  set_source_files_properties(src/UI.cpp PROPERTIES COMPILE_OPTIONS "-O1")
endif()

# --- Define simple tests when enabled ---
if(BUILD_TESTING)
  # Verify SDK basics
  add_test(
    NAME sdk_headers_and_lib_present
    COMMAND ${CMAKE_COMMAND}
      -DULTRALIGHT_SDK_ROOT=${ULTRALIGHT_SDK_ROOT}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/verify_sdk.cmake
  )

  # Verify assets in repository
  add_test(
    NAME repo_assets_present
    COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/tests/verify_assets.cmake
  )
endif()

# --- Copy runtime libraries to the output directory ---
if(WIN32)
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${ULTRALIGHT_SDK_ROOT}/bin/AppCore.dll"
    "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/AppCore.dll"
  )
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${ULTRALIGHT_SDK_ROOT}/bin/Ultralight.dll"
    "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/Ultralight.dll"
  )
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${ULTRALIGHT_SDK_ROOT}/bin/UltralightCore.dll"
    "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/UltralightCore.dll"
  )
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${ULTRALIGHT_SDK_ROOT}/bin/WebCore.dll"
    "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/WebCore.dll"
  )
else()
  # On macOS/Linux, copy the shared libraries we link against next to the executable
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${APP_CORE_LIB}" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/"
  )
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${ULTRALIGHT_LIB}" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/"
  )
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${ULTRALIGHT_CORE_LIB}" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/"
  )
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${WEB_CORE_LIB}" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/"
  )
endif()

# --- Copy shared libraries next to the binary on macOS/Linux for portable runtime ---
if(APPLE OR UNIX)
  file(GLOB ULTRALIGHT_SHARED_LIBS
    "${ULTRALIGHT_SDK_ROOT}/lib/*.dylib"
    "${ULTRALIGHT_SDK_ROOT}/lib/*.so")
  foreach(UL_LIB ${ULTRALIGHT_SHARED_LIBS})
    add_custom_command(
      TARGET Ultralight-WebBrowser POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "${UL_LIB}" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/")
  endforeach()
  # Some SDK packages may place runtime libs under bin as well (non-Windows)
  file(GLOB ULTRALIGHT_BIN_LIBS
    "${ULTRALIGHT_SDK_ROOT}/bin/*.dylib"
    "${ULTRALIGHT_SDK_ROOT}/bin/*.so")
  foreach(UL_BIN_LIB ${ULTRALIGHT_BIN_LIBS})
    add_custom_command(
      TARGET Ultralight-WebBrowser POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy "${UL_BIN_LIB}" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/")
  endforeach()
endif()

# --- Copy assets to the output directory ---
add_custom_command(
  TARGET Ultralight-WebBrowser POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${CMAKE_CURRENT_SOURCE_DIR}/assets"
  "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets"
)

# --- Copy ICU data to the output directory (Windows SDK layout) ---
if(WIN32)
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources"
  )
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${ULTRALIGHT_SDK_ROOT}/bin/ICU/icudt67l.dat"
    "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources/icudt67l.dat"
  )
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${ULTRALIGHT_SDK_ROOT}/bin/ICU/cacert.pem"
    "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources/cacert.pem"
  )
endif()

# --- Copy SDK resources for macOS/Linux if present (eg. cacert.pem, ICU data) ---
if(APPLE OR UNIX)
  if(EXISTS "${ULTRALIGHT_SDK_ROOT}/resources")
    add_custom_command(
      TARGET Ultralight-WebBrowser POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources"
    )
    add_custom_command(
      TARGET Ultralight-WebBrowser POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${ULTRALIGHT_SDK_ROOT}/resources" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources"
    )
  else()
    # Fallback for SDKs that ship ICU/cacert under bin/ICU (mirrors Windows layout)
    if(EXISTS "${ULTRALIGHT_SDK_ROOT}/bin/ICU")
      add_custom_command(
        TARGET Ultralight-WebBrowser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources"
      )
      add_custom_command(
        TARGET Ultralight-WebBrowser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${ULTRALIGHT_SDK_ROOT}/bin/ICU" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources"
      )
    endif()
  endif()
endif()

# --- Copy inspector files if present ---
if(EXISTS "${ULTRALIGHT_SDK_ROOT}/inspector")
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${ULTRALIGHT_SDK_ROOT}/inspector"
    "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/inspector"
  )
endif()

# --- Install rules for packaging (install into a self-contained app dir) ---
set(INSTALL_DIR_NAME "UltralightWebBrowser")

# Binary and assets next to each other to preserve relative paths
install(TARGETS Ultralight-WebBrowser RUNTIME DESTINATION ${INSTALL_DIR_NAME})
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION ${INSTALL_DIR_NAME})

if(WIN32)
  # Runtime DLLs and resources (Windows)
  install(FILES
    "${ULTRALIGHT_SDK_ROOT}/bin/AppCore.dll"
    "${ULTRALIGHT_SDK_ROOT}/bin/Ultralight.dll"
    "${ULTRALIGHT_SDK_ROOT}/bin/UltralightCore.dll"
    "${ULTRALIGHT_SDK_ROOT}/bin/WebCore.dll"
    DESTINATION ${INSTALL_DIR_NAME}
  )
  install(FILES
    "${ULTRALIGHT_SDK_ROOT}/bin/ICU/icudt67l.dat"
    "${ULTRALIGHT_SDK_ROOT}/bin/ICU/cacert.pem"
    DESTINATION ${INSTALL_DIR_NAME}/assets/resources
  )
else()
  # Re-locate libraries at install time for macOS/Linux
  find_library(APP_CORE_LIB NAMES AppCore PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)
  find_library(ULTRALIGHT_LIB NAMES Ultralight PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)
  find_library(ULTRALIGHT_CORE_LIB NAMES UltralightCore PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)
  find_library(WEB_CORE_LIB NAMES WebCore PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)

  if(APP_CORE_LIB)
    install(FILES ${APP_CORE_LIB} DESTINATION ${INSTALL_DIR_NAME})
  endif()
  if(ULTRALIGHT_LIB)
    install(FILES ${ULTRALIGHT_LIB} DESTINATION ${INSTALL_DIR_NAME})
  endif()
  if(ULTRALIGHT_CORE_LIB)
    install(FILES ${ULTRALIGHT_CORE_LIB} DESTINATION ${INSTALL_DIR_NAME})
  endif()
  if(WEB_CORE_LIB)
    install(FILES ${WEB_CORE_LIB} DESTINATION ${INSTALL_DIR_NAME})
  endif()

  # Include additional dylibs/so that ship with the SDK (both lib/ and bin/)
  file(GLOB ULTRALIGHT_SHARED_LIBS
    "${ULTRALIGHT_SDK_ROOT}/lib/*.dylib" "${ULTRALIGHT_SDK_ROOT}/lib/*.so")
  file(GLOB ULTRALIGHT_BIN_LIBS
    "${ULTRALIGHT_SDK_ROOT}/bin/*.dylib" "${ULTRALIGHT_SDK_ROOT}/bin/*.so")
  if(ULTRALIGHT_SHARED_LIBS)
    install(FILES ${ULTRALIGHT_SHARED_LIBS} DESTINATION ${INSTALL_DIR_NAME})
  endif()
  if(ULTRALIGHT_BIN_LIBS)
    install(FILES ${ULTRALIGHT_BIN_LIBS} DESTINATION ${INSTALL_DIR_NAME})
  endif()

  # Resources (if present) or ICU fallback
  if(EXISTS "${ULTRALIGHT_SDK_ROOT}/resources")
    install(DIRECTORY "${ULTRALIGHT_SDK_ROOT}/resources" DESTINATION ${INSTALL_DIR_NAME}/assets)
  elseif(EXISTS "${ULTRALIGHT_SDK_ROOT}/bin/ICU")
    install(DIRECTORY "${ULTRALIGHT_SDK_ROOT}/bin/ICU" DESTINATION ${INSTALL_DIR_NAME}/assets/resources)
  endif()
endif()

# License and docs
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" DESTINATION ${INSTALL_DIR_NAME})
endif()

# --- Packaging via CPack ---
# Try to extract Ultralight SDK version from header for reference; fallback to 1.4.0
set(ULTRALIGHT_VERSION_STR "1.4.0")
if(EXISTS "${ULTRALIGHT_SDK_ROOT}/include/Ultralight/Defines.h")
  file(READ "${ULTRALIGHT_SDK_ROOT}/include/Ultralight/Defines.h" _UL_DEFINES)
  string(REGEX MATCH "#define[ \t]+ULTRALIGHT_VERSION[ \t]+\"([^\"]+)\"" _UL_VER_MATCH "${_UL_DEFINES}")
  if(_UL_VER_MATCH)
    string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" ULTRALIGHT_VERSION_STR "${_UL_VER_MATCH}")
  endif()
endif()

# Allow overriding the package version with a browser-specific version (defaults to "dev")
set(WEBBROWSER_VERSION "dev" CACHE STRING "WebBrowser version to use in package names (defaults to 'dev')")

set(CPACK_PACKAGE_NAME "Ultralight-WebBrowser")
set(CPACK_PACKAGE_VENDOR "Ultralight")
if(WEBBROWSER_VERSION)
  set(CPACK_PACKAGE_VERSION "${WEBBROWSER_VERSION}")
else()
  set(CPACK_PACKAGE_VERSION "${ULTRALIGHT_VERSION_STR}")
endif()
set(CPACK_PACKAGE_CONTACT "noreply@example.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}")
# Strip binaries in packages to minimize size and avoid shipping debug symbols
set(CPACK_STRIP_FILES ON)

# Icon detection (used by Windows installers when available)
set(APP_ICON_ICO "")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/browser.ico")
  set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/browser.ico")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
  set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
else()
  set(APP_ICON_ICO "")
endif()

# Pass icon path to resource compiler for browser.rc
if (APP_ICON_ICO)
  # We no longer pass APP_ICON_ICO via compile definitions for browser.rc;
  # instead we generate a configured copy of the resource file with
  # @APP_ICON_ICO@ replaced by the absolute icon path below.
  # (Keeping compile-time definitions for other sources is still possible.)
endif()

# Configure browser.rc into the build directory using @VAR@ style substitution
# so configure_file(... @ONLY) will replace @APP_ICON_ICO@ with the chosen path.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/browser.rc"
               "${CMAKE_CURRENT_BINARY_DIR}/browser.rc"
               @ONLY)

# Choose default generators per platform; users can always override CPACK_GENERATOR
if(WIN32)
  if(NOT DEFINED CPACK_GENERATOR)
    if(CREATE_INSTALLER)
      # Try NSIS for an .exe installer, also emit a .zip as fallback
      set(CPACK_GENERATOR "NSIS;ZIP")
    else()
      # No installer requested: just produce a portable .zip
      set(CPACK_GENERATOR "ZIP")
    endif()
  endif()

  # Windows installer settings (NSIS/WiX)
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "Ultralight Web Browser")
  set(CPACK_NSIS_DISPLAY_NAME "Ultralight Web Browser")
  # The executable and assets are installed under a subfolder (${INSTALL_DIR_NAME}).
  # Default NSIS shortcuts assume executables live in $INSTDIR (or bin), so we create
  # proper shortcuts pointing to our nested path and adjust the FinishPage runner.
  # Avoid default shortcut generation to prevent broken links.
  set(CPACK_PACKAGE_EXECUTABLES "")
  set(CPACK_CREATE_DESKTOP_LINKS "")
  # Use quadruple backslashes so they survive into generated CPackConfig.cmake (parsed again by CMake)
  set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${INSTALL_DIR_NAME}\\\\Ultralight-WebBrowser.exe")
  if(APP_ICON_ICO)
    # NSIS UI and product icons
    set(CPACK_NSIS_MUI_ICON "${APP_ICON_ICO}")
    set(CPACK_NSIS_MUI_UNIICON "${APP_ICON_ICO}")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "${INSTALL_DIR_NAME}\\\\Ultralight-WebBrowser.exe")
    # WiX product icon (if WIX generator is used explicitly by user)
    set(CPACK_WIX_PRODUCT_ICON "${APP_ICON_ICO}")
  endif()

  # Create Start Menu and Desktop shortcuts pointing to the correct nested path
  # IMPORTANT: We must double-escape quotes (\\\") so they survive into CPackConfig.cmake
  # and then into NSIS script without breaking CMake parsing.
  set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Ultralight Web Browser.lnk\\\" \\\"$INSTDIR\\\\${INSTALL_DIR_NAME}\\\\Ultralight-WebBrowser.exe\\\"\nCreateShortCut \\\"$DESKTOP\\\\Ultralight Web Browser.lnk\\\" \\\"$INSTDIR\\\\${INSTALL_DIR_NAME}\\\\Ultralight-WebBrowser.exe\\\""
  )
  set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Ultralight Web Browser.lnk\\\"\nDelete \\\"$DESKTOP\\\\Ultralight Web Browser.lnk\\\""
  )
else()
  # Default non-Windows generator
  if(NOT DEFINED CPACK_GENERATOR)
    set(CPACK_GENERATOR "TGZ")
  endif()
endif()

# Helpful names with better OS naming (macOS, Linux, Windows) and architecture
# Detect architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH_SUFFIX "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(ARCH_SUFFIX "x86")
else()
  # Fallback to processor name for ARM etc.
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set(ARCH_SUFFIX "arm64")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7")
    set(ARCH_SUFFIX "armv7")
  else()
    set(ARCH_SUFFIX ${CMAKE_SYSTEM_PROCESSOR})
  endif()
endif()

# Better OS naming
if(APPLE)
  set(OS_NAME "macOS")
elseif(WIN32)
  set(OS_NAME "Windows")
elseif(UNIX)
  set(OS_NAME "Linux")
else()
  set(OS_NAME ${CMAKE_SYSTEM_NAME})
endif()

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${OS_NAME}-${ARCH_SUFFIX}")

include(CPack)

# --- Linux desktop integration (DEB/RPM friendly) ---
if(UNIX AND NOT APPLE)
  # Install system packages under /usr for distro-standard paths
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  set(APP_ID "ultralight-webbrowser")
  # Wrapper in /usr/local/bin (or /usr/bin if CMAKE_INSTALL_PREFIX=/usr)
  install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/packaging/ultralight-webbrowser.sh"
          DESTINATION bin
          RENAME ${APP_ID})

  # .desktop entry in share/applications
  set(EXEC_WRAPPER "$<IF:$<BOOL:$<TARGET_EXISTS:Ultralight-WebBrowser>>,${CMAKE_INSTALL_PREFIX}/bin/${APP_ID},${CMAKE_INSTALL_PREFIX}/bin/${APP_ID}>")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/packaging/ultralight-webbrowser.desktop.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ultralight-webbrowser.desktop"
    @ONLY
  )
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ultralight-webbrowser.desktop"
          DESTINATION share/applications)

  # Icon (try to reuse existing project image)
  set(APP_ICON_SOURCE "")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png")
    set(APP_ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data/screenshots/preview.png")
    # Fallback to preview if a dedicated icon is not provided
    set(APP_ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/data/screenshots/preview.png")
  endif()
  if(APP_ICON_SOURCE)
    install(FILES "${APP_ICON_SOURCE}" DESTINATION share/pixmaps RENAME ${APP_ID}.png)
  endif()
endif()