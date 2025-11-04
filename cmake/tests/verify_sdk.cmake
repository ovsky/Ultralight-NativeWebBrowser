# verify_sdk.cmake
# Validates the Ultralight SDK root contains expected structure and headers.

if(NOT DEFINED ULTRALIGHT_SDK_ROOT)
  message(FATAL_ERROR "ULTRALIGHT_SDK_ROOT not defined for verify_sdk.cmake")
endif()

get_filename_component(SDK "${ULTRALIGHT_SDK_ROOT}" ABSOLUTE)
message(STATUS "Verifying SDK root: ${SDK}")

if(NOT EXISTS "${SDK}")
  message(FATAL_ERROR "SDK root does not exist: ${SDK}")
endif()

if(NOT EXISTS "${SDK}/include")
  message(FATAL_ERROR "SDK include/ not found in ${SDK}")
endif()

if(NOT EXISTS "${SDK}/lib" AND NOT EXISTS "${SDK}/bin")
  message(FATAL_ERROR "SDK lib/ or bin/ not found in ${SDK}")
endif()

# Check a couple of key headers
if(NOT EXISTS "${SDK}/include/AppCore/AppCore.h")
  message(FATAL_ERROR "Missing AppCore/AppCore.h under ${SDK}/include")
endif()
if(NOT EXISTS "${SDK}/include/Ultralight/Ultralight.h")
  message(FATAL_ERROR "Missing Ultralight/Ultralight.h under ${SDK}/include")
endif()

message(STATUS "SDK structure looks OK")
