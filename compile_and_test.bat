@echo off
setlocal enabledelayedexpansion

set ROOT=C:\Users\Guarrazo\Desktop\pc-arcade-switch

echo Step 1: Cleaning old test executable...
del /Q "%ROOT%\build-host\tests\pas_tests.exe" 2>nul
del /Q "%ROOT%\build-host\tests\CMakeFiles\pas_tests.dir\main.cpp.obj" 2>nul

echo Step 2: Rebuilding tests...
cd /d "%ROOT%\build-host"
"C:\Program Files\CMake\bin\cmake.exe" --build . --target pas_tests
set BUILD_RESULT=!ERRORLEVEL!

if !BUILD_RESULT! NEQ 0 (
    echo BUILD_FAILED:!BUILD_RESULT! > "%ROOT%\compile_status.txt"
    exit /b !BUILD_RESULT!
)

echo BUILD_SUCCESS > "%ROOT%\compile_status.txt"

echo Step 3: Running tests...
cd /d "%ROOT%\build-host\tests"
pas_tests.exe
set TEST_RESULT=!ERRORLEVEL!

echo TEST_EXIT:!TEST_RESULT! >> "%ROOT%\compile_status.txt"

exit /b !TEST_RESULT!
