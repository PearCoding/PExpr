@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

if not "%~1"=="configure" goto :build

set "CONFIG=%~2"
if "!CONFIG!"=="" set "CONFIG=Release"

rem Strip first two args (configure + config), pass rest verbatim to cmake
set "ARGS=%*"
set "ARGS=!ARGS:*%~2=!"
cmake -S "%ROOT%" -B "%ROOT%\build\!CONFIG!" -G Ninja -DCMAKE_BUILD_TYPE=!CONFIG! !ARGS!
goto :eof

:build
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
cmake --build "%ROOT%\build\%CONFIG%" %2 %3 %4 %5 %6 %7 %8 %9
