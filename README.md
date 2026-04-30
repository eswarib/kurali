🎙️ Kurali — Desktop Transcription App (Linux & Windows)

Kurali is a speech-to-text transcription app for **Linux** and **Windows**, powered by the Whisper model.
It runs entirely on your device.

🧠 Overview

🐧 **Linux:** **AppImage** and **.deb** builds

🪟 **Windows:** **MSI** installer

⚙️ Electron frontend + C++ backend

🧩 Uses whisper.cpp for local model inference

🎤 Real-time microphone transcription

🪄 Inserts text automatically into the active window

🚀 Build Instructions

**AppImage** (same layout as release artifacts):

```bash
cd kurali/build
bash build.sh
```

**Debian package** (install with `apt` / `dpkg`, no FUSE; output at repo root):

```bash
cd kurali/build
bash build-deb.sh amd64    # Intel/AMD 64-bit (default arch if omitted)
# bash build-deb.sh arm64  # ARM64 — run on aarch64, or CI; use `KURALI_DEB_FRESH_NPM=1` (or legacy `CORAL_DEB_FRESH_NPM=1`) if switching arch after a prior npm install
sudo apt install ./Coral-*_amd64.deb   # or *_arm64.deb
```

Filenames: `Coral-<version>_amd64.deb` / `Coral-<version>_arm64.deb` (version from `coral-electron/package.json`).

**Prebuilt installers** from [GitHub Releases](https://github.com/eswarib/kurali/releases):

| Platform | Download |
|----------|-----------|
| Linux (AppImage) | [Releases — AppImage](https://github.com/eswarib/kurali/releases) |
| Windows (MSI, x64) | [Releases — Windows](https://github.com/eswarib/kurali/releases) |

Check the [releases](https://github.com/eswarib/kurali/releases) page for versioned assets. Attach a `.deb` to a release if you publish one, or build it locally with `build-deb.sh`.

🧰 Tech Stack

| Layer | Technology |
|-------|------------|
| Frontend | Electron |
| Backend | C++ |
| Model inference | whisper.cpp |

🎧 How to Use

1. Launch Kurali.
2. Press and hold the hotkey (default: Alt + Z) unless you changed it in Settings.
3. Speak — Kurali transcribes when you release the hotkey (behavior depends on trigger mode in config).
4. Text is inserted into the focused window.

⚙️ Configuration

You can customize settings in the app.

**Model path:** Download any Whisper model compatible with whisper.cpp (e.g. base.en, small, medium), set its path in configuration — Kurali resolves common locations.

**Hotkey:** Defaults vary by shipped config; open Settings to see or change your trigger key and mode.

🌐 Website (GitHub Pages)

Static site files are in **`website/`** (`index.html`, `coffee.html`, etc.). Deployments use **GitHub Actions** (`.github/workflows/github-pages.yml`). In the repository **Settings → Pages**, set the **source** to **GitHub Actions** if it is not already (the classic “Deploy from branch” option only publishes **`/docs`** or repo root, not `/website`).

🙏 Acknowledgements

💡 **ggerganov** — for creating [whisper.cpp](https://github.com/ggerganov/whisper.cpp), which powers Kurali's transcription engine.

🎨 **[Voibe](https://getvoibe.com)** — for inspiring the concept and workflow.

📄 License

MIT License © 2026 Eswari Mathialagan

Kurali — voice transcription for Linux and Windows.

_______________________________________________________________________________________________________________________

Kurali for Ubuntu

<img width="3330" height="1732" alt="Kurali on Ubuntu" src="https://github.com/user-attachments/assets/4cf669c9-314a-4888-808e-463b4e392be5" />

_______________________________________________________________________________________________________________________

Kurali for Windows

<img width="1601" height="977" alt="Kurali on Windows" src="https://github.com/user-attachments/assets/7fcea4bb-cae5-434c-8548-cc58414162a1" />

