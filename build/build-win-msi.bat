@echo off
setlocal EnableDelayedExpansion
REM Build coral for Windows and produce MSI installer via electron-builder.
REM Run from coralapp directory: build\build-win-msi.bat
REM Prerequisites: build-windows.bat deps + Node.js (npm)

cd /d "%~dp0\.."
set REPO_ROOT=%CD%

if not exist "%REPO_ROOT%\coral-electron\package.json" (
    echo Error: Run from coralapp directory. coral-electron\package.json not found.
    exit /b 1
)

REM ---- Step 1: Build native bundle (zip) ----
echo === Step 1: Building native bundle ===
call "%~dp0build-windows.bat"
if errorlevel 1 exit /b 1

REM ---- Get version ----
for /f "delims=" %%a in ('powershell -NoProfile -Command "((Get-Content coral-electron\package.json | ConvertFrom-Json).version).ToString().Trim()"') do set APPVER=%%a
if "%APPVER%"=="" set APPVER=0.0.0

set BUNDLE_DIR=coral-windows-x64-v%APPVER%
if not exist "%BUNDLE_DIR%" (
    echo Error: Bundle directory %BUNDLE_DIR% not found after build.
    exit /b 1
)

REM ---- Step 2: Stage dist/win-resources for electron-builder ----
REM Structure: exe + conf/ next to exe (backend looks for exeDir/conf).
REM Models are intentionally NOT bundled in the MSI to keep installer size
REM small (~100MB vs ~760MB). The Electron frontend downloads the default
REM ggml-small.en-q8_0.bin into %USERPROFILE%\.coral\models on first launch.
echo.
echo === Step 2: Staging dist/win-resources ===
set STAGE_DIR=%REPO_ROOT%\dist\win-resources
if exist "%STAGE_DIR%" rmdir /s /q "%STAGE_DIR%"
mkdir "%STAGE_DIR%"
mkdir "%STAGE_DIR%\conf"

REM coral.exe
copy /Y "%BUNDLE_DIR%\coral-%APPVER%.exe" "%STAGE_DIR%\coral.exe"

REM DLLs
copy /Y "%BUNDLE_DIR%\*.dll" "%STAGE_DIR%\" 2>nul

REM conf/ from bundle (already created by build-windows-bundle.cmd)
if exist "%BUNDLE_DIR%\conf\config.json" (
    copy /Y "%BUNDLE_DIR%\conf\config.json" "%STAGE_DIR%\conf\"
) else (
    copy /Y "%REPO_ROOT%\coral\conf\config-windows.json" "%STAGE_DIR%\conf\config.json"
)

echo Staged contents:
dir "%STAGE_DIR%" /b
dir "%STAGE_DIR%\conf" /b

REM ---- Step 3: Build MSI with electron-builder ----
echo.
echo === Step 3: Building MSI ===
cd coral-electron
call npm install
if errorlevel 1 (
    cd "%REPO_ROOT%"
    exit /b 1
)
call npm run build:win:msi
if errorlevel 1 (
    cd "%REPO_ROOT%"
    exit /b 1
)
cd "%REPO_ROOT%"

echo.
echo === MSI build complete ===
echo Output: build\Release\*.msi
