@echo off
REM Script para compilar tests en host (Windows)

set CMAKE="C:\Program Files\CMake\bin\cmake.exe"
set CC=c:/devkitPro/msys2/usr/bin/gcc.exe
set CXX=c:/devkitPro/msys2/usr/bin/g++.exe

if exist build-host rmdir /s /q build-host
mkdir build-host
cd build-host

%CMAKE% .. -G "MSYS Makefiles" -DCMAKE_C_COMPILER=%CC% -DCMAKE_CXX_COMPILER=%CXX% -DPAS_BUILD_TESTS=ON
if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed
    exit /b 1
)

%CMAKE% --build .
if %ERRORLEVEL% NEQ 0 (
    echo Build failed
    exit /b 1
)

echo Build successful!
echo Run tests with: ctest
