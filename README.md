🎙️ Coral — Desktop Transcription App (Linux & Windows)

Coral is a speech-to-text transcription app for **Linux** and **Windows**, powered by the Whisper model.
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
cd coralapp/build
bash build.sh
```

**Debian package** (install with `apt` / `dpkg`, no FUSE; output at repo root):

```bash
cd coralapp/build
bash build-deb.sh amd64    # Intel/AMD 64-bit (default arch if omitted)
# bash build-deb.sh arm64  # ARM64 — run on aarch64, or CI; use CORAL_DEB_FRESH_NPM=1 if switching arch after a prior npm install
sudo apt install ./Coral-*_amd64.deb   # or *_arm64.deb
```

Filenames: `Coral-<version>_amd64.deb` / `Coral-<version>_arm64.deb` (version from `coral-electron/package.json`).

**Prebuilt installers** from the latest release ([all assets](https://github.com/eswarib/coralapp/releases)):

| Platform | Download |
|----------|-----------|
| Linux (AppImage) | [CoralApp-0.5.0-x86_64.AppImage](https://github.com/eswarib/coralapp/releases/download/coral-0.5.0/CoralApp-0.5.0-x86_64.AppImage) |
| Windows (MSI, x64) | [Coral-0.5.0.msi](https://github.com/eswarib/coralapp/releases/download/coral-0.5.0/Coral-0.5.0.msi) |

Check the [releases](https://github.com/eswarib/coralapp/releases) page for newer builds. Attach a `.deb` to a release if you publish one, or build it locally with `build-deb.sh`.

🧰 Tech Stack

| Layer | Technology |
|-------|------------|
| Frontend | Electron |
| Backend | C++ |
| Model inference | whisper.cpp |

🎧 How to Use

1. Launch Coral.
2. Press and hold the hotkey (default: Alt + Z) unless you changed it in Settings.
3. Speak — Coral transcribes when you release the hotkey (behavior depends on trigger mode in config).
4. Text is inserted into the focused window.

⚙️ Configuration

You can customize settings in the app.

**Model path:** Download any Whisper model compatible with whisper.cpp (e.g. base.en, small, medium), set its path in configuration — Coral resolves common locations.

**Hotkey:** Defaults vary by shipped config; open Settings to see or change your trigger key and mode.

🙏 Acknowledgements

💡 **ggerganov** — for creating [whisper.cpp](https://github.com/ggerganov/whisper.cpp), which powers Coral’s transcription engine.

🎨 **[Voibe](https://getvoibe.com)** — for inspiring the concept and workflow.

📄 License

MIT License © 2026 Eswari Mathialagan

Coral — voice transcription for Linux and Windows.

_______________________________________________________________________________________________________________________

Coral for Ubuntu

<img width="3330" height="1732" alt="CoralOnUbuntu" src="https://github.com/user-attachments/assets/4cf669c9-314a-4888-808e-463b4e392be5" />

_______________________________________________________________________________________________________________________

Coral for Windows

<img width="1601" height="977" alt="CoralOnWindows" src="https://github.com/user-attachments/assets/7fcea4bb-cae5-434c-8548-cc58414162a1" />

