🎙️ Coral — Linux Transcription App

Coral is a speech-to-text transcription app for Linux, powered by the Whisper model.
It runs entirely on your device.

🧠 Overview

🐧 Linux-first design (AppImage available)

⚙️ Electron frontend + C++ backend

🧩 Uses whisper.cpp
 for local model inference

🎤 Real-time microphone transcription

🪄 Inserts text automatically into the active window

🚀 Build Instructions

To build an AppImage manually:

cd coralapp/build
bash build.sh


Or simply download the prebuilt AppImage from the latest release:

👉 Download Coral AppImage
[Releases](https://github.com/eswarib/coralapp/releases/download/coral-0.2.0/CoralApp-0.2.0-x86_64.AppImage)

🧰 Tech Stack

Layer	Technology
Frontend	Electron
Backend	C++
Model Inference	whisper.cpp

🎧 How to Use

Launch Coral
Press and hold the hotkey (default: Alt + Z)
Speak — Coral will transcribe your speech once you release the hotkey
The transcribed text is automatically inserted into the currently active text field or editor

⚙️ Configuration
You can customize settings.

Model Path:
Download any Whisper model
 compatible with whisper.cpp (e.g., base.en, small, medium),
and set its path in the configuration. Coral will automatically detect and load it.

Hotkey:
The default hotkey is Alt + Z, but you can change it to any preferred combination.

⚠️ Limitations

A Windows .exe build exists but is not fully tested.
The app is primarily developed and optimized for Linux.

🙏 Acknowledgements

💡 ggerganov
 — for creating whisper.cpp, which powers Coral’s transcription engine

🎨 [voibe](getvoibe.com)
 — for inspiring the concept and workflow

📄 License

MIT License © 2025 [Your Name]

Coral — voice transcription for Linux and Windows

_______________________________________________________________________________________________________________________

Coral for Ubuntu

<img width="3330" height="1732" alt="CoralOnUbuntu" src="https://github.com/user-attachments/assets/4cf669c9-314a-4888-808e-463b4e392be5" />

_______________________________________________________________________________________________________________________

Coral for Windows

<img width="1601" height="977" alt="CoralOnWindows" src="https://github.com/user-attachments/assets/7fcea4bb-cae5-434c-8548-cc58414162a1" />


