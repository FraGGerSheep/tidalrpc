@echo off
REM Baut tidalrpc.exe (GUI / Tray, ohne Konsole) mit MSVC. Kein cmake noetig.
REM Aufruf: build.bat [x64|x86]   (Standard: x64)
setlocal

set "ARCH=%~1"
if /i "%ARCH%"=="x86" (
    set "VCARCH=x86"
) else (
    set "ARCH=x64"
    set "VCARCH=x64"
)
echo Ziel-Architektur: %ARCH%

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [Fehler] Visual Studio Installer nicht gefunden.
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSPATH=%%i"
if "%VSPATH%"=="" (
    echo [Fehler] Keine Visual-Studio-Installation gefunden.
    exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" %VCARCH% >nul
if errorlevel 1 (
    echo [Fehler] vcvarsall.bat %VCARCH% fehlgeschlagen.
    exit /b 1
)

REM Ressourcen (Icon) kompilieren.
rc /nologo /fo src\tidalrpc.res src\tidalrpc.rc
if errorlevel 1 (
    echo [Fehler] Ressourcen-Kompilierung fehlgeschlagen.
    exit /b 1
)

cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MT ^
   /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DUNICODE /D_UNICODE ^
   /Isrc src\main.cpp src\tidalrpc.res ^
   /Fe:tidalrpc.exe ^
   /link /SUBSYSTEM:WINDOWS winhttp.lib windowsapp.lib ole32.lib shell32.lib user32.lib

if errorlevel 1 (
    echo [Fehler] Kompilierung fehlgeschlagen.
    exit /b 1
)

del *.obj src\tidalrpc.res >nul 2>nul
echo.
echo Fertig: tidalrpc.exe
