@echo off
setlocal

set BUILD_LOG=%~dp0build_output.log
set BUILD_STATUS=%~dp0build_status.txt

echo Starting build... > "%BUILD_STATUS%"
call "%~dp0build_tests.bat" > "%BUILD_LOG%" 2>&1

if %ERRORLEVEL% EQU 0 (
    echo SUCCESS > "%BUILD_STATUS%"
) else (
    echo FAILED:%ERRORLEVEL% > "%BUILD_STATUS%"
)

exit /b %ERRORLEVEL%
