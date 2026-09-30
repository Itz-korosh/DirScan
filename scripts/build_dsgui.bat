@echo off
REM Builds dsgui.dll → dlls/dsgui.dll

set SRC=dsgui\dsgui.cpp
set OUT=dlls\dsgui.dll
set IMP=dlls\libdsgui.a

if not exist dlls mkdir dlls

g++ -std=c++11 -O2 -shared -DDSGUI_EXPORTS -D_WIN32_WINNT=0x0601 ^
  -Idsgui %SRC% -o %OUT% ^
  -lcomctl32 -luser32 -lgdi32 -lshell32 -lole32 -ladvapi32 ^
  -static-libgcc -static-libstdc++ -static -Wl,--kill-at ^
  -Wl,--out-implib,%IMP%

if errorlevel 1 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

echo.
echo Built: %OUT%
echo Import: %IMP%