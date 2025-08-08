@echo off
REM Clear all environment variables that might conflict
set INCLUDE=
set LIB=
set LIBPATH=
set C_INCLUDE_PATH=
set CPLUS_INCLUDE_PATH=
set LIBRARY_PATH=
set PKG_CONFIG_PATH=

REM Set up Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM Clean build directory
if exist build rmdir /s /q build

REM Configure with CMake
"C:\Program Files\CMake\bin\cmake.exe" -B build -S . -G "Visual Studio 17 2022" -A x64

REM Build
"C:\Program Files\CMake\bin\cmake.exe" --build build --config Debug
