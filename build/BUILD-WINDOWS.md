# Packaging Kurali for Windows

This guide covers building the **Kurali** C++ backend (`kurali.exe`) and packaging it with the Electron frontend into a Windows installer (MSI, NSIS, or portable exe).

## Prerequisites

- **Windows 10/11** (64-bit)
- **Visual Studio 2022** (or 2019) with C++ workload
- **Node.js** (for npm, Electron)
- **vcpkg** (C++ package manager)
- **whisper.cpp** (cloned as sibling of the kurali repo, or set `WHISPER_DIR`)

## Step 1: Build the C++ backend (kurali.exe)

From the `kurali` directory (repo root):

```cmd
build\build-windows.bat
```

This will:

1. Set up vcpkg (clones to `../vcpkg` if missing)
2. Install portaudio and libsndfile via vcpkg
3. Clone whisper.cpp to `../whispercpp` if missing
4. Build whisper.cpp (static libs)
5. Build kurali and produce `build-win\kurali\Release\kurali.exe` (or similar)

**Environment overrides:**

- `VCPKG_ROOT` — default: `%REPO_ROOT%\..\vcpkg`
- `WHISPER_DIR` — default: `%REPO_ROOT%\..\whispercpp`

**Visual Studio generator:** The script uses `Visual Studio 18 2026`. If you have VS 2022 or 2019, edit `build-windows.bat` and change to `Visual Studio 17 2022` or `Visual Studio 16 2019` respectively.

## Step 2: Bundle kurali.exe with DLLs and config

After Step 1, create the native bundle (exe + DLLs + config):

```cmd
build\build-windows-bundle.cmd 0.5.0 %VCPKG_ROOT%
```

Use the version from `coral-electron/package.json` in the command above (shown here as `0.5.0`). This creates `coral-windows-x64-v0.5.0\` with:

- `kurali-0.5.0.exe`
- VC++ runtime DLLs (msvcp140, vcruntime140, etc.)
- vcpkg DLLs (portaudio, libsndfile)
- `conf/config.json`

> **Note on Whisper models:** The Windows installer **does not bundle** any `ggml-*.bin` model file. On first launch, `coral-electron/main.js` downloads the configured default (`ggml-small.en-q8_0.bin`, ~182 MB) directly from Hugging Face into `%USERPROFILE%\.coral\models\`. This keeps the MSI small (~100 MB instead of ~760 MB) and avoids duplicating model files between the install dir and each user's home dir. A small progress window is shown during the one-time download.

## Step 3: Package Electron + backend together

### Option A: One-command (MSI installer)

```cmd
build\build-win-msi.bat
```

This runs Steps 1–2 (via `build-windows.bat`, which calls `build-windows-bundle.cmd`), stages everything to `dist/win-resources`, then runs electron-builder. Output: `build/Release/*.msi`.

### Option B: Manual staging + electron-builder

If you already have `coral-windows-x64-v0.5.0` from Step 2:

1. **Stage resources** into `dist/win-resources`:

   ```
   dist/win-resources/
   ├── kurali.exe
   ├── *.dll
   └── conf/config.json
   ```

   No `model/` directory — see the note in Step 2.

2. **Run electron-builder** from `coral-electron`:

   ```cmd
   cd coral-electron
   npm install
   npm run build:win:msi      REM MSI installer
   npm run build:win:portable REM Portable .exe
   npm run build:win:nsis     REM NSIS installer
   npm run build:win         REM Portable + NSIS
   ```

3. **Output** goes to `build/Release/` (per `package.json` `directories.output`).

## Summary

| Step | Command | Result |
|------|---------|--------|
| 1 | `build\build-windows.bat` | `build-win\...\kurali.exe` |
| 2 | `build\build-windows-bundle.cmd 0.5.0 %VCPKG_ROOT%` | `coral-windows-x64-v0.5.0\` |
| 3a | `build\build-win-msi.bat` | `build/Release/*.msi` |
| 3b | Stage to `dist/win-resources`, then `npm run build:win:*` | MSI / portable / NSIS |

The `package.json` `extraResources` section tells electron-builder to include `dist/win-resources` contents in the packaged app’s `resources` folder, where `main.js` looks for `kurali.exe`.
