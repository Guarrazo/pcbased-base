@echo off
cd build-host
"C:\Program Files\CMake\bin\cmake.exe" --build . --target pas_tests > ..\rebuild_tests.log 2>&1
echo Exit code: %ERRORLEVEL% >> ..\rebuild_tests.log
