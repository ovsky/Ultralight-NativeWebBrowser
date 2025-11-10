cmake_minimum_required(VERSION 3.8)
project(UltralightWebBrowser LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
            "src/AdBlocker.h"
            "src/AdBlocker.cpp"
            "src/DownloadManager.h"
            "src/DownloadManager.cpp"
            "src/Tab.h"
            "src/Tab.cpp"
            "src/UI.h"
            "src/UI.cpp"
            "src/main.cpp"
            "browser.rc"
)

add_app(Ultralight-WebBrowser ${SOURCES})

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
endif()

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