cmake_minimum_required(VERSION 3.8)

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

# --- Library directories ---
link_directories(
  "${ULTRALIGHT_SDK_ROOT}/lib"
)

# --- Custom add_app function ---
function(add_app NAME)
  add_executable(${NAME} WIN32 ${ARGN})

  # Disable RTTI
  if(MSVC)
    target_compile_options(${NAME} PRIVATE /GR-)
  else()
    target_compile_options(${NAME} PRIVATE -fno-rtti)
  endif()

  # Link libraries
  target_link_libraries(${NAME} PRIVATE
    AppCore
    Ultralight
    UltralightCore
    WebCore
  )
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

# --- Copy DLLs to the output directory ---
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

# --- Copy assets to the output directory ---
add_custom_command(
  TARGET Ultralight-WebBrowser POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${CMAKE_CURRENT_SOURCE_DIR}/assets"
  "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets"
)

# --- Copy ICU data to the output directory ---
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

# --- Copy inspector files ---
add_custom_command(
  TARGET Ultralight-WebBrowser POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${ULTRALIGHT_SDK_ROOT}/inspector"
  "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/inspector"
)