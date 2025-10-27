set(SOURCES "src/Browser.h"
            "src/Browser.cpp"
            "src/Tab.h"
            "src/Tab.cpp"
            "src/UI.h"
            "src/UI.cpp"
            "src/main.cpp"
            "browser.rc"
)

set(NEEDS_INSPECTOR TRUE)

# Add current directory to include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_app(Ultralight-WebBrowser ${SOURCES})

INSTALL(DIRECTORY "${ULTRALIGHT_INSPECTOR_DIR}" DESTINATION "${INSTALL_PATH}/assets")
INSTALL(FILES "${CMAKE_CURRENT_SOURCE_DIR}/browser.ico" DESTINATION "${INSTALL_PATH}")