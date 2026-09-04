@echo off
cd /d "C:\Users\Guarrazo\Desktop\pc-arcade-switch\build-host\tests"
pas_tests.exe > "C:\Users\Guarrazo\Desktop\pc-arcade-switch\test_simple.log" 2>&1
echo EXIT_CODE:%ERRORLEVEL% >> "C:\Users\Guarrazo\Desktop\pc-arcade-switch\test_simple.log"
