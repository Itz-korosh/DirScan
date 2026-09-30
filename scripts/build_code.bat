@echo off
REM =========================================================================
REM  build_code.bat — compile DIRSCAN app sources into build\dirscan.exe
REM  Requires: dlls\dsgui.dll and dlls\libdsgui.a (run build_dsgui.bat first)
REM =========================================================================

setlocal
cd /d "%~dp0\.."

set OUT=build\dirscan.exe

if not exist build mkdir build

REM ---- Check library exists ----------------------------------------------
if not exist dlls\dsgui.dll (
    echo.
    echo ERROR: dlls\dsgui.dll not found.
    echo Run scripts\build_dsgui.bat first to build the GUI library.
    exit /b 1
)
if not exist dlls\libdsgui.a (
    echo.
    echo ERROR: dlls\libdsgui.a not found.
    echo Run scripts\build_dsgui.bat first to build the GUI library.
    exit /b 1
)

REM ---- Compile -----------------------------------------------------------
echo [dirscan] compiling src\DirGui.cpp src\DirscanBase.cpp src\DirScan.cpp ...

g++ -std=c++11 -O2 -D_WIN32_WINNT=0x0601 ^
  -Isrc -Idsgui ^
  src\DirGui.cpp src\DirscanBase.cpp src\DirScan.cpp ^
  -Ldlls -ldsgui ^
  -o %OUT% ^
  -lcomctl32 -luser32 -lgdi32 -lshell32 -lole32 -ladvapi32 ^
  -static-libgcc -static-libstdc++ -static

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

REM ---- Copy DLL next to the exe ------------------------------------------
echo [dirscan] copying dlls\dsgui.dll -^> build\

copy /Y dlls\dsgui.dll build\dsgui.dll >nul
if errorlevel 1 (
    echo WARNING: could not copy dsgui.dll. Copy it manually.
)

echo.
echo ============================================================
echo   BUILD OK
echo ============================================================
echo   build\dirscan.exe
echo   build\dsgui.dll
echo.
echo Run:
echo     cd build ^&^& dirscan.exe
echo.

endlocal