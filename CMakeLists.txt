cmake_minimum_required(VERSION 3.8)
project(UltralightWebBrowser LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CREATE_INSTALLER "Create a Windows installer via CPack (NSIS)" OFF)

include(CTest)

# Ultralight SDK: default to repository `data/` folder when not set
set(ULTRALIGHT_SDK_ROOT "" CACHE PATH "Path to Ultralight SDK root (contains include, lib, bin)")
if(NOT ULTRALIGHT_SDK_ROOT)
	set(ULTRALIGHT_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/data")
endif()

if(NOT EXISTS "${ULTRALIGHT_SDK_ROOT}/include" OR NOT EXISTS "${ULTRALIGHT_SDK_ROOT}/lib")
	message(FATAL_ERROR "ULTRALIGHT_SDK_ROOT (\"${ULTRALIGHT_SDK_ROOT}\") does not look valid. Expect 'include' and 'lib' subfolders.")
endif()

include_directories(
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${ULTRALIGHT_SDK_ROOT}/include"
)

function(add_app NAME)
	if (WIN32)
		add_executable(${NAME} WIN32 ${ARGN})
	else()
		add_executable(${NAME} ${ARGN})
	endif()

	if(MSVC)
		target_compile_options(${NAME} PRIVATE /GR-)
	else()
		target_compile_options(${NAME} PRIVATE -fno-rtti)
	endif()

	find_library(APP_CORE_LIB NAMES AppCore PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)
	find_library(ULTRALIGHT_LIB NAMES Ultralight PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)
	find_library(ULTRALIGHT_CORE_LIB NAMES UltralightCore PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)
	find_library(WEB_CORE_LIB NAMES WebCore PATHS "${ULTRALIGHT_SDK_ROOT}/lib" "${ULTRALIGHT_SDK_ROOT}/bin" NO_DEFAULT_PATH)

	if(NOT APP_CORE_LIB OR NOT ULTRALIGHT_LIB OR NOT ULTRALIGHT_CORE_LIB OR NOT WEB_CORE_LIB)
		message(FATAL_ERROR "Failed to locate Ultralight libraries under ${ULTRALIGHT_SDK_ROOT}.")
	endif()

	target_link_libraries(${NAME} PRIVATE
		${APP_CORE_LIB}
		${ULTRALIGHT_LIB}
		${ULTRALIGHT_CORE_LIB}
		${WEB_CORE_LIB}
	)

	if(APPLE OR UNIX)
		set_target_properties(${NAME} PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
	endif()
endfunction()

# Sources
set(SOURCES
	"src/Browser.h"
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
	"${CMAKE_CURRENT_BINARY_DIR}/browser.rc"
)

add_app(Ultralight-WebBrowser ${SOURCES})

# Install/package settings
set(INSTALL_DIR_NAME "UltralightWebBrowser")
install(TARGETS Ultralight-WebBrowser RUNTIME DESTINATION ${INSTALL_DIR_NAME})
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION ${INSTALL_DIR_NAME})

# Windows: include SDK DLLs and ICU files if present
if(WIN32)
	file(GLOB ULTRALIGHT_DLLS RELATIVE "${ULTRALIGHT_SDK_ROOT}/bin" "${ULTRALIGHT_SDK_ROOT}/bin/*.dll")
	list(LENGTH ULTRALIGHT_DLLS _dll_count)
	if(_dll_count GREATER 0)
		# install expects full paths; prepend the bin folder
		set(_dll_fullpaths)
		foreach(_d ${ULTRALIGHT_DLLS})
			list(APPEND _dll_fullpaths "${ULTRALIGHT_SDK_ROOT}/bin/${_d}")
		endforeach()
		install(FILES ${_dll_fullpaths} DESTINATION ${INSTALL_DIR_NAME})
	endif()

	file(GLOB ICU_FILES "${ULTRALIGHT_SDK_ROOT}/bin/ICU/icudt*.dat" "${ULTRALIGHT_SDK_ROOT}/bin/ICU/cacert.pem")
	list(LENGTH ICU_FILES _icu_count)
	if(_icu_count GREATER 0)
		install(FILES ${ICU_FILES} DESTINATION ${INSTALL_DIR_NAME}/assets/resources)
	endif()
endif()

# Post-build: copy assets and runtime files next to the exe for running from build dir
if(WIN32)
	add_custom_command(TARGET Ultralight-WebBrowser POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets"
	)
	foreach(_dll IN ITEMS AppCore.dll Ultralight.dll UltralightCore.dll WebCore.dll)
		add_custom_command(TARGET Ultralight-WebBrowser POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ULTRALIGHT_SDK_ROOT}/bin/${_dll}" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/${_dll}"
		)
	endforeach()
	add_custom_command(TARGET Ultralight-WebBrowser POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources"
	)
	add_custom_command(TARGET Ultralight-WebBrowser POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ULTRALIGHT_SDK_ROOT}/bin/ICU/icudt67l.dat" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources/icudt67l.dat"
	)
	add_custom_command(TARGET Ultralight-WebBrowser POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ULTRALIGHT_SDK_ROOT}/bin/ICU/cacert.pem" "$<TARGET_FILE_DIR:Ultralight-WebBrowser>/assets/resources/cacert.pem"
	)
endif()

# CPack (global scope)
set(WEBBROWSER_VERSION "dev" CACHE STRING "WebBrowser version to use in package names (defaults to 'dev')")
set(CPACK_PACKAGE_NAME "Ultralight-WebBrowser")
set(CPACK_PACKAGE_VENDOR "Ultralight")
set(CPACK_PACKAGE_VERSION "${WEBBROWSER_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}")
set(CPACK_STRIP_FILES ON)

set(APP_ICON_ICO "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/browser.ico")
	set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/browser.ico")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
	set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/browser.rc" "${CMAKE_CURRENT_BINARY_DIR}/browser.rc" @ONLY)

if(WIN32)
	if(NOT DEFINED CPACK_GENERATOR)
		if(CREATE_INSTALLER)
			set(CPACK_GENERATOR "NSIS;ZIP")
		else()
			set(CPACK_GENERATOR "ZIP")
		endif()
	endif()
	set(CPACK_PACKAGE_INSTALL_DIRECTORY "Ultralight Web Browser")
	set(CPACK_NSIS_DISPLAY_NAME "Ultralight Web Browser")
	set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${INSTALL_DIR_NAME}\\Ultralight-WebBrowser.exe")
	if(APP_ICON_ICO)
		set(CPACK_NSIS_MUI_ICON "${APP_ICON_ICO}")
		set(CPACK_NSIS_MUI_UNIICON "${APP_ICON_ICO}")
	endif()
else()
	if(NOT DEFINED CPACK_GENERATOR)
		set(CPACK_GENERATOR "TGZ")
	endif()
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(ARCH_SUFFIX "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(ARCH_SUFFIX "x86")
else()
	set(ARCH_SUFFIX ${CMAKE_SYSTEM_PROCESSOR})
endif()

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
