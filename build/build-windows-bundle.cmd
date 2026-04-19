@echo off
setlocal EnableDelayedExpansion
REM Bundle coral.exe with DLLs - no PowerShell required
REM Called from build-windows.bat with: call build-windows-bundle.cmd

cd /d "%~dp0\.."
set "REPO_ROOT=%CD%"

set "APPVER=%~1"
if "%APPVER%"=="" set "APPVER=0.0.0"
set "VCPKG_ROOT=%~2"

set "OUT_DIR=coral-windows-x64-v%APPVER%"
mkdir "%OUT_DIR%" 2>nul

REM Find coral.exe
set "CORAL_EXE="
for /f "delims=" %%f in ('dir /s /b "build-win\coral.exe" 2^>nul') do (
    if "!CORAL_EXE!"=="" set "CORAL_EXE=%%f"
)
if "%CORAL_EXE%"=="" (
    echo ERROR: coral.exe not found under build-win
    exit /b 1
)

copy /Y "%CORAL_EXE%" "%OUT_DIR%\coral-%APPVER%.exe"

REM conf/ next to exe (backend looks for exeDir/conf).
REM Models are NOT bundled here. Coral's Electron frontend downloads the
REM default ggml-small.en-q8_0.bin into %USERPROFILE%\.coral\models on first
REM launch. This keeps the bundle small and avoids duplicating model files
REM between the install dir and the user's home dir.
mkdir "%OUT_DIR%\conf" 2>nul
if exist "%REPO_ROOT%\coral\conf\config-windows.json" (
    copy /Y "%REPO_ROOT%\coral\conf\config-windows.json" "%OUT_DIR%\conf\config.json"
) else if exist "%REPO_ROOT%\coral\conf\config.json" (
    copy /Y "%REPO_ROOT%\coral\conf\config.json" "%OUT_DIR%\conf\config.json"
)

REM VC++ CRT from vswhere
set "VSINSTALL="
for /f "delims=" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul') do set "VSINSTALL=%%i"

set "CRT_COPIED="
if defined VSINSTALL (
    set "VERFILE=%VSINSTALL%\VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt"
    if exist "%VERFILE%" (
        set "VCVER="
        for /f "delims=" %%v in ('type "%VERFILE%"') do set "VCVER=%%v"
        if defined VCVER (
            for /d %%d in ("%VSINSTALL%\VC\Tools\MSVC\!VCVER!\redist\x64\Microsoft.VC*.CRT") do (
                copy /Y "%%d\msvcp140.dll" "%OUT_DIR%\" 2>nul && copy /Y "%%d\vcruntime140.dll" "%OUT_DIR%\" 2>nul && copy /Y "%%d\vcruntime140_1.dll" "%OUT_DIR%\" 2>nul && set "CRT_COPIED=1"
            )
        )
    )
)

if not "%CRT_COPIED%"=="1" (
    if exist "%SystemRoot%\System32\msvcp140.dll" (
        copy /Y "%SystemRoot%\System32\msvcp140.dll" "%OUT_DIR%\"
        copy /Y "%SystemRoot%\System32\vcruntime140.dll" "%OUT_DIR%\"
        copy /Y "%SystemRoot%\System32\vcruntime140_1.dll" "%OUT_DIR%\"
        echo Bundled VC++ CRT from System32
    ) else (
        echo WARNING: VC++ DLLs not found - copy msvcp140.dll, vcruntime140.dll, vcruntime140_1.dll manually
    )
)

REM vcpkg DLLs
if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\installed\x64-windows\bin\*.dll" (
        copy /Y "%VCPKG_ROOT%\installed\x64-windows\bin\*.dll" "%OUT_DIR%\"
    )
)

REM Create zip (tar on Windows 10+, else minimal PowerShell one-liner)
where tar >nul 2>&1
if %errorlevel% equ 0 (
    pushd "%OUT_DIR%"
    tar -a -cf "..\coral-windows-x64-v%APPVER%.zip" *
    popd
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%REPO_ROOT%\%OUT_DIR%\*' -DestinationPath '%REPO_ROOT%\coral-windows-x64-v%APPVER%.zip' -Force"
)

echo.
echo === Build complete ===
echo Output: %OUT_DIR%
dir "%OUT_DIR%" /b
