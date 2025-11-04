# verify_assets.cmake
# Validates the repository assets required by the sample are present.

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR not defined for verify_assets.cmake")
endif()

set(ASSETS_DIR "${SOURCE_DIR}/assets")
message(STATUS "Verifying assets directory: ${ASSETS_DIR}")

if(NOT EXISTS "${ASSETS_DIR}")
  message(FATAL_ERROR "assets/ folder not found at ${ASSETS_DIR}")
endif()

foreach(req IN ITEMS ui.html ui.css ui.js new_tab_page.html about.html release_notes.html)
  if(NOT EXISTS "${ASSETS_DIR}/${req}")
    message(FATAL_ERROR "Required asset missing: ${req}")
  endif()
endforeach()

message(STATUS "Assets look OK")
