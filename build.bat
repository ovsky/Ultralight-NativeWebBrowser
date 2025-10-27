@echo off
cmake -B build
cmake --build build --config Release
cmake --install build --config Release
