@echo off
cd build-host\tests
pas_tests.exe > ..\..\test_results.txt 2>&1
echo Exit code: %ERRORLEVEL% >> ..\..\test_results.txt
