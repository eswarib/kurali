@echo off
setlocal EnableDelayedExpansion
REM Build kurali (C++ backend) for Windows - steps from windows-build.yaml
REM Run from kurali repo root: build\build-windows.bat
REM Or from build: .\build-windows.bat

cd /d "%~dp0\.."
set REPO_ROOT=%CD%

REM Paths: default to sibling of kurali clone (override with env: VCPKG_ROOT, WHISPER_DIR)
if "%VCPKG_ROOT%"=="" set VCPKG_ROOT=%REPO_ROOT%\..\vcpkg
if "%WHISPER_DIR%"=="" set WHISPER_DIR=%REPO_ROOT%\..\whispercpp
if not exist "%REPO_ROOT%\coral-electron\package.json" (
    echo Error: Run from repo root. coral-electron\package.json not found.
    exit /b 1
)
echo === Kurali Windows Build ===
echo Repo root: %REPO_ROOT%
echo vcpkg:     %VCPKG_ROOT%
echo whisper:   %WHISPER_DIR%
echo.

REM ---- Set up vcpkg ----
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo Setting up vcpkg...
    if not exist "%VCPKG_ROOT%" git clone https://github.com/microsoft/vcpkg "%VCPKG_ROOT%"
    call "%VCPKG_ROOT%\bootstrap-vcpkg.bat"
)

echo Installing portaudio and libsndfile...
"%VCPKG_ROOT%\vcpkg.exe" install portaudio:x64-windows libsndfile:x64-windows

REM ---- Compute app version ----
for /f "delims=" %%a in ('powershell -NoProfile -Command "((Get-Content coral-electron\package.json | ConvertFrom-Json).version).ToString().Trim()"') do set APPVER=%%a
if "%APPVER%"=="" set APPVER=0.0.0
echo APPVER=%APPVER%

REM ---- Set up whisper.cpp ----
if not exist "%WHISPER_DIR%" (
    echo Cloning whisper.cpp...
    git clone --depth 1 https://github.com/ggerganov/whisper.cpp "%WHISPER_DIR%"
)

REM ---- Build whisper.cpp ----
echo Building whisper.cpp...
cmake -S "%WHISPER_DIR%" -B "%WHISPER_DIR%\build" -G "Visual Studio 18 2026" -A x64 -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DGGML_OPENMP=OFF
cmake --build "%WHISPER_DIR%\build" --config Release --target ggml whisper

REM ---- Detect whisper and ggml paths ----
echo Listing .lib files...
dir /s /b "%WHISPER_DIR%\build\*.lib" 2>nul

set WHISPERLIB=
for /f "delims=" %%f in ('dir /s /b "%WHISPER_DIR%\build\whisper*.lib" 2^>nul') do (
    if "!WHISPERLIB!"=="" set WHISPERLIB=%%f
)
if "%WHISPERLIB%"=="" (
    for /f "delims=" %%f in ('dir /s /b "%WHISPER_DIR%\build\whispercpp.lib" 2^>nul') do set WHISPERLIB=%%f
)
if "%WHISPERLIB%"=="" (
    echo ERROR: whisper.lib not found under %WHISPER_DIR%\build
    exit /b 1
)
echo WHISPERLIB=%WHISPERLIB%

set GGMLLIBS=
for /f "delims=" %%f in ('dir /s /b "%WHISPER_DIR%\build\ggml*.lib" 2^>nul') do (
    if "!GGMLLIBS!"=="" (set GGMLLIBS=%%f) else (set GGMLLIBS=!GGMLLIBS!;%%f)
)
if "%GGMLLIBS%"=="" (
    echo ERROR: ggml*.lib not found
    exit /b 1
)
echo GGMLLIBS=%GGMLLIBS%

REM ---- Build kurali (CMake) ----
if exist build-win rmdir /s /q build-win
echo Building kurali...
cmake -S coral -B build-win -G "Visual Studio 18 2026" -A x64 -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release -DWHISPER_LIBRARY="%WHISPERLIB%" -DGGML_LIBRARIES="%GGMLLIBS%" -DAPP_VERSION="%APPVER%" -DBUILD_DATE="%DATE%" -DGIT_COMMIT="local"
cmake --build build-win --config Release
if errorlevel 1 exit /b 1

REM ---- Bundle kurali.exe with DLLs, config, model ----
echo.
echo === Bundling kurali.exe ===
call "%~dp0build-windows-bundle.cmd" %APPVER% %VCPKG_ROOT%
if errorlevel 1 exit /b 1
