cmake_minimum_required(VERSION 3.8)
project(UltralightWebBrowser LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# --- Copy inspector files ---
add_custom_command(
  TARGET Ultralight-WebBrowser POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${ULTRALIGHT_SDK_ROOT}/inspector"
  "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/inspector"
)