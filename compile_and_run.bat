@echo off
setlocal enabledelayedexpansion

REM Clean previous CMake cache to avoid generator mismatch errors
if exist build\CMakeCache.txt (
	echo Removing previous CMake cache to avoid generator mismatch...
	rmdir /S /Q build 2>nul
)

REM Configure
cmake -S . -B build %*
if errorlevel 1 exit /b 1

REM Build
cmake --build build --config Release
if errorlevel 1 exit /b 1

REM Install (optional)
cmake --install build --config Release

cd .\build\Release\
 Ultralight-WebBrowser.exe %*
cd ..\..
endlocal
