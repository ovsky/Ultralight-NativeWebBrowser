cmake_minimum_required(VERSION 3.8)
project(UltralightWebBrowser LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
            "src/Tab.h"
            "src/Tab.cpp"
            "src/UI.h"
            "src/UI.cpp"
            "src/main.cpp"
            "browser.rc"
)

add_app(Ultralight-WebBrowser ${SOURCES})

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
# Try to extract Ultralight SDK version from header for package version; fallback to 1.4.0
set(ULTRALIGHT_VERSION_STR "1.4.0")
if(EXISTS "${ULTRALIGHT_SDK_ROOT}/include/Ultralight/Defines.h")
  file(READ "${ULTRALIGHT_SDK_ROOT}/include/Ultralight/Defines.h" _UL_DEFINES)
  string(REGEX MATCH "#define[ \t]+ULTRALIGHT_VERSION[ \t]+\"([^\"]+)\"" _UL_VER_MATCH "${_UL_DEFINES}")
  if(_UL_VER_MATCH)
    string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" ULTRALIGHT_VERSION_STR "${_UL_VER_MATCH}")
  endif()
endif()

set(CPACK_PACKAGE_NAME "Ultralight-WebBrowser")
set(CPACK_PACKAGE_VENDOR "Ultralight")
set(CPACK_PACKAGE_VERSION "${ULTRALIGHT_VERSION_STR}")
set(CPACK_PACKAGE_CONTACT "noreply@example.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# Default generator: TGZ (tar.gz). You can override: -DCPACK_GENERATOR="DEB;RPM;TGZ"
if(NOT DEFINED CPACK_GENERATOR)
  set(CPACK_GENERATOR "TGZ")
endif()

# Helpful names
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}")

include(CPack)