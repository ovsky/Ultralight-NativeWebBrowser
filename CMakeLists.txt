cmake_minimum_required(VERSION 3.8)
project(UltralightWebBrowser LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# If the install prefix wasn't explicitly set by the caller, default to a
# local, user-writable directory inside the build folder to avoid attempts
# to install into system locations (eg. "C:/Program Files (x86)") which
# require elevated permissions during `cmake --install`.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install path" FORCE)
endif()

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
if(NOT ULTRALIGHT_SDK_ROOT)
  set(ULTRALIGHT_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/data")
endif()

if(NOT EXISTS "${ULTRALIGHT_SDK_ROOT}/include" OR NOT EXISTS "${ULTRALIGHT_SDK_ROOT}/lib")
  message(FATAL_ERROR "ULTRALIGHT_SDK_ROOT (\"${ULTRALIGHT_SDK_ROOT}\") does not look valid. Expected 'include' and 'lib' subfolders.")
endif()

# Sanity: warn if expected SDK headers are missing
if(NOT EXISTS "${ULTRALIGHT_SDK_ROOT}/include/AppCore/AppCore.h")
  message(WARNING "AppCore headers not found under ${ULTRALIGHT_SDK_ROOT}/include. Did you set ULTRALIGHT_SDK_ROOT correctly?")
endif()

# --- Include directories ---
include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${ULTRALIGHT_SDK_ROOT}/include"
)

# --- Custom add_app function ---
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
            # Dependency manager and sidecar compiled only when enabled & libcurl available
            "src/AdBlocker.h"
            "src/AdBlocker.cpp"
            "src/DownloadManager.h"
            "src/DownloadManager.cpp"
            "src/Tab.h"
            "src/Tab.cpp"
            "src/UI.h"
            "src/UI.cpp"
            "src/main.cpp"
            "${CMAKE_CURRENT_BINARY_DIR}/browser.rc"
)

option(ENABLE_DRM_SIDECAR "Enable DRM Sidecar support (downloads heavy engine components)" ON)
option(AUTO_INSTALL_CURL "Attempt to auto-install libcurl dev libraries during configure (CI/local helper)" OFF)

# Try to find libcurl only when the option is on.
if(ENABLE_DRM_SIDECAR)
  # If requested, attempt to auto-install curl dev headers via helper script (best-effort).
  if(AUTO_INSTALL_CURL)
    if(UNIX AND NOT APPLE)
      message(STATUS "AUTO_INSTALL_CURL enabled: attempting to install libcurl via scripts/install_curl.sh")
      execute_process(COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install_curl.sh" RESULT_VARIABLE _auto_curl_res)
      if(NOT _auto_curl_res EQUAL 0)
        message(WARNING "AUTO_INSTALL_CURL attempted but failed on this host. Proceeding; FindCURL will try to locate libcurl.")
      endif()
    elseif(APPLE)
      execute_process(COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install_curl.sh" RESULT_VARIABLE _auto_curl_res)
      if(NOT _auto_curl_res EQUAL 0)
        message(WARNING "AUTO_INSTALL_CURL attempted but failed on macOS; please install via brew if needed.")
      endif()
    elseif(WIN32)
      # Attempt to bootstrap vcpkg and install curl dependencies (best-effort).
  message(STATUS "AUTO_INSTALL_CURL enabled: attempting to install libcurl via scripts/install_curl.ps1")
  execute_process(COMMAND powershell -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install_curl.ps1" OUTPUT_VARIABLE _vcpkg_out RESULT_VARIABLE _auto_curl_res ERROR_VARIABLE _vcpkg_err OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(_vcpkg_out)
        # Extract the last non-empty line from the script output (script logs may precede the path)
        string(REPLACE "\r\n" "\n" _vcpkg_out_clean "${_vcpkg_out}")
        string(REPLACE "\r" "\n" _vcpkg_out_clean "${_vcpkg_out_clean}")
        string(REGEX MATCHALL "[^
]+" _vcpkg_lines "${_vcpkg_out_clean}")
        list(LENGTH _vcpkg_lines _vlen)
        if(_vlen GREATER 0)
          math(EXPR _last_index "${_vlen} - 1")
          list(GET _vcpkg_lines ${_last_index} _vcpkg_path)
          string(STRIP "${_vcpkg_path}" _vcpkg_path)
          message(STATUS "install_curl script reported vcpkg path: ${_vcpkg_path}")
          # Export to environment for later auto-detection
          set(ENV{VCPKG_ROOT} "${_vcpkg_path}")
        endif()
      endif()
      if(NOT _auto_curl_res EQUAL 0)
        message(WARNING "AUTO_INSTALL_CURL attempted but failed on Windows; vcpkg or choco may be unavailable.")
      endif()
    endif()
  endif()

  find_package(CURL)
  # On Windows, if find_package failed, try common vcpkg-installed locations or repo-local vcpkg
  if (NOT CURL_FOUND AND WIN32)
    message(STATUS "CURL not found by find_package(); attempting auto-detect from common vcpkg locations...")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(_vcpkg_triplet "x64-windows")
    else()
      set(_vcpkg_triplet "x86-windows")
    endif()

    set(_vcpkg_candidates
      $ENV{VCPKG_ROOT}
      "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg"
      "C:/vcpkg"
    )

    foreach(_c ${_vcpkg_candidates})
      if(_c STREQUAL "" )
        continue()
      endif()
      set(_inst_dir "${_c}/installed/${_vcpkg_triplet}")
      if(EXISTS "${_inst_dir}/include/curl/curl.h")
        # prefer static import lib then import lib
        if(EXISTS "${_inst_dir}/lib/libcurl.lib")
          set(_libpath "${_inst_dir}/lib/libcurl.lib")
        elseif(EXISTS "${_inst_dir}/lib/libcurl_imp.lib")
          set(_libpath "${_inst_dir}/lib/libcurl_imp.lib")
        elseif(EXISTS "${_inst_dir}/bin/libcurl.dll")
          # fallback: use dll path (linker may still require import lib)
          set(_libpath "${_inst_dir}/bin/libcurl.dll")
        endif()
        if(DEFINED _libpath)
          message(STATUS "Auto-detected libcurl at ${_inst_dir}; using include and lib from vcpkg installed tree")
          set(CURL_FOUND TRUE CACHE BOOL "Found libcurl (auto-detected)" FORCE)
          set(CURL_INCLUDE_DIRS "${_inst_dir}/include" CACHE PATH "Curl include dir" FORCE)
          set(CURL_LIBRARIES "${_libpath}" CACHE FILEPATH "Curl library" FORCE)
          set(CURL_VERSION_STRING "(vcpkg-installed)" CACHE STRING "Curl version string" FORCE)
          break()
        endif()
      endif()
    endforeach()
  endif()
  if(NOT CURL_FOUND AND DEFINED ENV{VCPKG_ROOT})
    message(WARNING "VCPKG_ROOT is present, but CMake wasn't invoked with vcpkg toolchain file. Re-run cmake with -DCMAKE_TOOLCHAIN_FILE=$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake to let find_package() locate vcpkg-installed libraries.")
  endif()
  if(CURL_FOUND)
    message(STATUS "Found CURL: ${CURL_VERSION_STRING}. Enabling DRM sidecar support.")
    add_definitions(-DENABLE_DRM_SIDECAR=1)
    list(APPEND SOURCES "src/DependencyManager.h" "src/DependencyManager.cpp" "src/SidecarProcess.h" "src/SidecarProcess.cpp")
  else()
    message(WARNING "libcurl not found. DRM sidecar support will be disabled. Install libcurl or set ENABLE_DRM_SIDECAR=OFF to ignore this warning.")
    set(ENABLE_DRM_SIDECAR OFF)
  endif()
endif()

add_app(Ultralight-WebBrowser ${SOURCES})

if(ENABLE_DRM_SIDECAR)
  if(CURL_FOUND)
    if(TARGET CURL::libcurl)
      target_link_libraries(Ultralight-WebBrowser PRIVATE CURL::libcurl)
    else()
      target_link_libraries(Ultralight-WebBrowser PRIVATE ${CURL_LIBRARIES})
      include_directories(${CURL_INCLUDE_DIRS})
    endif()
  endif()
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
    COMMAND ${CMAKE_COMMAND} -E echo "Copying AppCore.dll if present"
  )
  if(EXISTS "${ULTRALIGHT_SDK_ROOT}/bin/AppCore.dll")
    add_custom_command(
      TARGET Ultralight-WebBrowser POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${ULTRALIGHT_SDK_ROOT}/bin/AppCore.dll"
      "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/AppCore.dll"
    )
  else()
    message(WARNING "AppCore.dll not found at ${ULTRALIGHT_SDK_ROOT}/bin/AppCore.dll; skipping copy")
  endif()
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying Ultralight.dll if present"
  )
  if(EXISTS "${ULTRALIGHT_SDK_ROOT}/bin/Ultralight.dll")
    add_custom_command(
      TARGET Ultralight-WebBrowser POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${ULTRALIGHT_SDK_ROOT}/bin/Ultralight.dll"
      "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/Ultralight.dll"
    )
  else()
    message(WARNING "Ultralight.dll not found at ${ULTRALIGHT_SDK_ROOT}/bin/Ultralight.dll; skipping copy")
  endif()
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying UltralightCore.dll if present"
  )
  if(EXISTS "${ULTRALIGHT_SDK_ROOT}/bin/UltralightCore.dll")
    add_custom_command(
      TARGET Ultralight-WebBrowser POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${ULTRALIGHT_SDK_ROOT}/bin/UltralightCore.dll"
      "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/UltralightCore.dll"
    )
  else()
    message(WARNING "UltralightCore.dll not found at ${ULTRALIGHT_SDK_ROOT}/bin/UltralightCore.dll; skipping copy")
  endif()
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying WebCore.dll if present"
  )
  if(EXISTS "${ULTRALIGHT_SDK_ROOT}/bin/WebCore.dll")
    add_custom_command(
      TARGET Ultralight-WebBrowser POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${ULTRALIGHT_SDK_ROOT}/bin/WebCore.dll"
      "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/WebCore.dll"
    )
  else()
    message(WARNING "WebCore.dll not found at ${ULTRALIGHT_SDK_ROOT}/bin/WebCore.dll; skipping copy")
  endif()
  # If we auto-detected a vcpkg-installed libcurl (_inst_dir), copy any runtime DLLs
  # (libcurl and its OpenSSL/zlib dependencies) from the vcpkg installed bin folder
  # into the target output directory so the EXE can find them at runtime.
  if(DEFINED _inst_dir AND EXISTS "${_inst_dir}/bin")
    file(GLOB CURL_RUNTIME_DLLS "${_inst_dir}/bin/*.dll")
    foreach(_dll ${CURL_RUNTIME_DLLS})
      add_custom_command(
        TARGET Ultralight-WebBrowser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/"
      )
    endforeach()
  endif()
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
# Copy assets file-by-file (more robust than copy_directory which can fail
# under some MSBuild/environment setups). We create destination subdirs as
# needed and use copy_if_different to avoid touching unchanged files.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
  file(GLOB_RECURSE _ASSET_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
  foreach(_af ${_ASSET_FILES})
    # Create subdirectory in the target output if the asset lives in a nested folder
    get_filename_component(_af_dir "${_af}" DIRECTORY)
    if(NOT _af_dir STREQUAL "")
      add_custom_command(
        TARGET Ultralight-WebBrowser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/${_af_dir}"
      )
    endif()
    add_custom_command(
      TARGET Ultralight-WebBrowser POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets/${_af}" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/${_af}"
    )
  endforeach()
else()
  message(WARNING "Assets directory not found at ${CMAKE_CURRENT_SOURCE_DIR}/assets; skipping asset copy")
endif()

# --- Copy ICU data to the output directory (Windows SDK layout) ---
if(WIN32)
  add_custom_command(
    TARGET Ultralight-WebBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources"
  )
  if(EXISTS "${ULTRALIGHT_SDK_ROOT}/bin/ICU/icudt67l.dat")
    add_custom_command(
      TARGET Ultralight-WebBrowser POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${ULTRALIGHT_SDK_ROOT}/bin/ICU/icudt67l.dat"
      "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources/icudt67l.dat"
    )
  else()
    message(WARNING "ICU data file icudt67l.dat not found at ${ULTRALIGHT_SDK_ROOT}/bin/ICU/icudt67l.dat; skipping copy")
  endif()
  if(EXISTS "${ULTRALIGHT_SDK_ROOT}/bin/ICU/cacert.pem")
    add_custom_command(
      TARGET Ultralight-WebBrowser POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${ULTRALIGHT_SDK_ROOT}/bin/ICU/cacert.pem"
      "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources/cacert.pem"
    )
  else()
    message(WARNING "cacert.pem not found at ${ULTRALIGHT_SDK_ROOT}/bin/ICU/cacert.pem; skipping copy")
  endif()
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