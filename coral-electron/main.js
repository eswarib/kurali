const { ipcMain } = require('electron');

const { app, Tray, Menu, BrowserWindow, dialog, screen, Notification } = require('electron');
app.commandLine.appendSwitch('no-sandbox');
// Linux: avoid gpu-process "InitializeSandbox() called with multiple threads" (stderr WARNING).
// GPU acceleration stays on; only the GPU helper sandbox is off (main sandbox already disabled above).
if (process.platform === 'linux') {
  app.commandLine.appendSwitch('disable-gpu-sandbox');
}
// Quieter Chromium stderr for distributed builds (DBus tray, GPU sandbox, CSP INFO on stderr).
// Local `electron .` stays verbose. Override: CORAL_VERBOSE=1 on .deb/AppImage/packaged too.
const installedDebLayout =
  process.platform === 'linux' &&
  (__dirname === '/opt/coral' ||
    __dirname.startsWith('/opt/coral/') ||
    (typeof process.execPath === 'string' && process.execPath.startsWith('/opt/coral/')));
if (!process.env.CORAL_VERBOSE && (app.isPackaged || process.env.APPIMAGE || installedDebLayout)) {
  app.commandLine.appendSwitch('log-level', '2');
}

process.on('uncaughtException', (err) => {
  console.error('Uncaught exception:', err);
});

ipcMain.handle('getAppVersion', () => app.getVersion())
const path = require('path');
const os = require('os');
const fs = require('fs');
const { spawn } = require('child_process');
const readline = require('readline');

/** Append one line to ~/.coral/logs/coral.log (same file as the C++ backend; no console noise). */
function appendElectronLog(...args) {
  try {
    const logDir = path.join(os.homedir(), '.coral', 'logs');
    if (!fs.existsSync(logDir)) fs.mkdirSync(logDir, { recursive: true });
    const logFile = path.join(logDir, 'coral.log');
    const msg = args.map((a) => (typeof a === 'object' ? JSON.stringify(a) : String(a))).join(' ');
    const line = `${new Date().toISOString()} [electron] ${msg}\n`;
    fs.appendFileSync(logFile, line, 'utf8');
  } catch (_) { /* ignore log failures */ }
}

let originalIconPath;
const ANIMATION_FRAME_COUNT = 8;
let animationFrames = [];
let transcribeAnimationFrames = [];
function resolveIconPaths() {
  if (process.env.APPIMAGE) {
    for (let i = 0; i < ANIMATION_FRAME_COUNT; i++) {
      animationFrames.push(path.join(process.resourcesPath, 'icons', 'wave_icons', `wave_${i}.png`));
      transcribeAnimationFrames.push(path.join(process.resourcesPath, 'icons', 'transcribe_icons', `transcribe_${i}.png`));
    }
    originalIconPath = path.join(process.resourcesPath, 'coral.png');
  } else if (app.isPackaged && process.platform === 'win32') {
    // Windows packaged: icons are in app.asar; getAppPath() returns app root (asar content)
    const appPath = app.getAppPath();
    for (let i = 0; i < ANIMATION_FRAME_COUNT; i++) {
      animationFrames.push(path.join(appPath, 'icons', 'wave_icons', `wave_${i}.png`));
      transcribeAnimationFrames.push(path.join(appPath, 'icons', 'transcribe_icons', `transcribe_${i}.png`));
    }
    originalIconPath = path.join(process.resourcesPath, 'coral.png');
    if (!fs.existsSync(originalIconPath)) {
      originalIconPath = path.join(appPath, 'coral.png');
    }
  } else {
    const baseDir = __dirname;
    for (let i = 0; i < ANIMATION_FRAME_COUNT; i++) {
      animationFrames.push(path.join(baseDir, 'icons', 'wave_icons', `wave_${i}.png`));
      transcribeAnimationFrames.push(path.join(baseDir, 'icons', 'transcribe_icons', `transcribe_${i}.png`));
    }
    const candidates = [
      path.join(baseDir, 'coral.png'),
      path.join(baseDir, '..', 'logo', 'coral.png'),
      path.join(process.resourcesPath || baseDir, 'coral.png'),
    ];
    originalIconPath = candidates.find(p => fs.existsSync(p)) || candidates[0];
  }
}
resolveIconPaths();

let tray = null;
let configWindow = null;
let devWindow = null;
let aboutWindow = null;
let backendProcess = null;
let backendRl = null;
let backendStopping = false;
let animationInterval = null;
let currentFrame = 0;
let stopAnimationTimer = null;
let lastTranscriptionTimer = null;
let currentTriggerMode = 'pushToTalk';
/** Timestamps (ms) for each INJECT_PENDING; paired in order with INJECTION_DONE */
const pendingInjectionStarts = [];
/** Whisper duration (ms) per queued inject; paired with INJECTION_DONE */
const pendingWhisperMs = [];
/** set on TRANSCRIBING_START, cleared on INJECT_PENDING or TRANSCRIBING_DONE (discard) */
let transcribingStartMs = null;

/** From config.json recordDevTimingStats — Developer Settings, default off */
let recordDevTimingStats = false;

function readRecordDevTimingStats() {
  try {
    const cfgPath = path.join(os.homedir(), '.coral', 'conf', 'config.json');
    const cfg = JSON.parse(fs.readFileSync(cfgPath, 'utf8'));
    recordDevTimingStats = cfg.recordDevTimingStats === true;
  } catch (_) {
    recordDevTimingStats = false;
  }
}

/**
 * Append one development timing row to ~/.coral/dev-timing.stats
 * Line format: whisper_ms,inject_ms (inject_ms is -1 if transcript was not injected)
 * whisper_ms: START → INJECT_PENDING, or START → DONE when junk-filtered
 * inject_ms: INJECT_PENDING → INJECTION_DONE when injected
 * Only when recordDevTimingStats is true (Developer Settings).
 */
function appendDevTimingStats(whisperMs, injectMs) {
  if (!recordDevTimingStats) return;
  try {
    const dir = path.join(os.homedir(), '.coral');
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
    const file = path.join(dir, 'dev-timing.stats');
    fs.appendFileSync(file, `${whisperMs},${injectMs}\n`, 'utf8');
  } catch (e) {
    console.warn('[Coral] dev-timing.stats append failed:', e.message);
  }
}

function clearContinuousModeTimers() {
  if (stopAnimationTimer) { clearTimeout(stopAnimationTimer); stopAnimationTimer = null; }
  if (lastTranscriptionTimer) { clearTimeout(lastTranscriptionTimer); lastTranscriptionTimer = null; }
}

/**
 * Shown when the C++ backend prints TRANSCRIBING_DONE (after Whisper runs and the
 * text is queued for the injector — not after X11/uinput/typing finishes).
 * Any delay after this toast is usually injection, not the model.
 */
function showTranscriptionDoneNotification() {
  if (!Notification.isSupported()) return;
  try {
    const cfgPath = path.join(os.homedir(), '.coral', 'conf', 'config.json');
    const cfg = JSON.parse(fs.readFileSync(cfgPath, 'utf8'));
    if (cfg.showTranscriptionNotification === false) return;
  } catch (_) {}
  try {
    new Notification({
      title: 'Coral',
      body: 'Transcription is being done — text will appear shortly.',
      icon: originalIconPath,
    }).show();
  } catch (_) {}
}

function readTriggerMode() {
  try {
    const cfgPath = path.join(os.homedir(), '.coral', 'conf', 'config.json');
    const cfg = JSON.parse(fs.readFileSync(cfgPath, 'utf8'));
    currentTriggerMode = cfg.triggerMode || 'pushToTalk';
  } catch (_) {}
}

function createConfigWindow() {
  if (configWindow) {
    configWindow.focus();
    return;
  }
  const display = screen.getPrimaryDisplay();
  const workH = display.workAreaSize.height;
  const initH = Math.min(720, workH - 60);
  configWindow = new BrowserWindow({
    width: 560,
    height: initH,
    show: false,
    useContentSize: true,
    resizable: true,
    autoHideMenuBar: true,
    title: 'Settings',
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false,
    },
  });
  // Ensure the menu bar is not visible for this window
  configWindow.setMenuBarVisibility(false);
  configWindow.loadFile(path.join(__dirname, 'renderer', 'index.html'));
  // Fallback: show after 1s even if resize IPC hasn't fired
  const showTimer = setTimeout(() => { if (configWindow && !configWindow.isVisible()) configWindow.show(); }, 1000);
  configWindow.on('closed', () => {
    clearTimeout(showTimer);
    configWindow = null;
  });
}

function createDevWindow() {
  if (devWindow) {
    devWindow.focus();
    return;
  }
  const display = screen.getPrimaryDisplay();
  const workH = display.workAreaSize.height;
  const initH = Math.min(640, workH - 60);
  devWindow = new BrowserWindow({
    width: 520,
    height: initH,
    show: false,
    useContentSize: true,
    resizable: true,
    autoHideMenuBar: true,
    title: 'Developer Settings',
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false,
    },
  });
  devWindow.setMenuBarVisibility(false);
  devWindow.loadFile(path.join(__dirname, 'renderer', 'dev.html'));
  const devShowTimer = setTimeout(() => { if (devWindow && !devWindow.isVisible()) devWindow.show(); }, 1000);
  devWindow.on('closed', () => {
    clearTimeout(devShowTimer);
    devWindow = null;
  });
}

function createAboutWindow() {
  if (aboutWindow) {
    aboutWindow.focus();
    return;
  }
  let triggerKeyDisplay = 'Ctrl';
  try {
    const cfgPath = path.join(os.homedir(), '.coral', 'conf', 'config.json');
    const cfg = JSON.parse(fs.readFileSync(cfgPath, 'utf8'));
    if (cfg.triggerKey) triggerKeyDisplay = cfg.triggerKey;
  } catch (_) {}

  aboutWindow = new BrowserWindow({
    width: 420,
    height: 380,
    resizable: false,
    minimizable: true,
    maximizable: false,
    autoHideMenuBar: true,
    title: 'About Coral',
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
    },
  });
  aboutWindow.setMenuBarVisibility(false);
  aboutWindow.on('closed', () => { aboutWindow = null; });

  const aboutHtml = `<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; base-uri 'none'">
<title>About Coral</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
html,body{overflow:hidden;height:100%}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
  background:linear-gradient(135deg,#f0f4ff 0%,#e8f5e9 40%,#e0f2f1 100%);
  color:#1a1a2e;padding:28px 32px;-webkit-font-smoothing:antialiased}
.header{display:flex;align-items:center;gap:12px;margin-bottom:20px;padding-bottom:14px;border-bottom:1px solid rgba(0,150,136,0.15)}
.header-icon{width:36px;height:36px;background:linear-gradient(135deg,#0d47a1,#00897b);border-radius:10px;
  display:flex;align-items:center;justify-content:center;color:#fff;font-size:18px;font-weight:700}
.header h2{font-size:1.3em;font-weight:600;background:linear-gradient(135deg,#0d47a1,#00897b);
  -webkit-background-clip:text;-webkit-text-fill-color:transparent}
.version{font-size:13px;color:#5c6bc0;font-weight:600;margin-top:4px}
.desc{margin-top:16px;font-size:14px;color:#455a64;line-height:1.6}
.copy{margin-top:20px;font-size:12px;color:#90a4ae}
.btn{margin-top:20px;padding:9px 24px;border:none;border-radius:8px;font-size:0.9em;font-weight:500;
  cursor:pointer;background:linear-gradient(135deg,#0d47a1,#00897b);color:#fff;transition:opacity 0.15s}
.btn:hover{opacity:0.9}
</style></head><body>
<div class="header"><div class="header-icon">C</div><h2>Coral</h2></div>
<div class="version">Version ${app.getVersion()}</div>
<div class="desc">To transcribe your speech, hold down the trigger key (${triggerKeyDisplay}) and start talking.</div>
<div class="copy">&copy; 2025 Coral Contributors</div>
<button class="btn" onclick="window.close()">OK</button>
</body></html>`;
  aboutWindow.loadURL('data:text/html;charset=utf-8,' + encodeURIComponent(aboutHtml));
}

function getAppImageMountPath()
{
    if (process.env.APPIMAGE)
    {
        // process.execPath: /tmp/.mount_Coral-XXXXXX/usr/bin/electron
        // We want: /tmp/.mount_Coral-XXXXXX
        return path.join(path.dirname(process.execPath), "..", "..", "..");
    }
    return null;
}

function isChildAlive(child) {
  if (!child || !child.pid) return false;
  try {
    process.kill(child.pid, 0);
    return true;
  } catch {
    return false;
  }
}

function startTrayAnimation(frames) {
  if (animationInterval) stopTrayAnimation();
  const icons = frames || animationFrames;
  if (!icons.length || !icons.every(f => fs.existsSync(f))) {
    console.warn('Animation icons not found, skipping tray animation');
    return;
  }
  currentFrame = 0;
  animationInterval = setInterval(() => {
    try { tray.setImage(icons[currentFrame]); } catch (_) {}
    currentFrame = (currentFrame + 1) % ANIMATION_FRAME_COUNT;
  }, 100);
}
function stopTrayAnimation() {
  if (animationInterval) {
    clearInterval(animationInterval);
    animationInterval = null;
    try { tray.setImage(originalIconPath); } catch (_) {}
  }
}


// Prompt user to set up input permissions for hotkey detection and text injection.
// Two things are needed:
//   1. User in 'input' group  → access to /dev/input/event* (key detection via evdev)
//   2. udev rule for uinput   → access to /dev/uinput (text injection via virtual keyboard)
// Similar to how Wireshark asks for packet-capture permissions on first run.
let inputGroupPromptShown = false;
function promptInputGroupPermission() {
    if (inputGroupPromptShown) return;  // only ask once per session
    inputGroupPromptShown = true;

    const { execSync } = require('child_process');
    const currentUser = os.userInfo().username;

    // Check if both permissions are already in place
    let hasInputGroup = false;
    let hasUinputRule = false;
    try {
        const groups = execSync('groups', { encoding: 'utf-8' });
        hasInputGroup = groups.includes('input');
    } catch (_) {}
    try {
        hasUinputRule = fs.existsSync('/etc/udev/rules.d/99-coral-uinput.rules');
    } catch (_) {}

    if (hasInputGroup && hasUinputRule) return;  // all set

    dialog.showMessageBox({
        type: 'warning',
        title: 'Keyboard Access Required',
        message: 'Coral needs access to keyboard input devices for hotkey detection and text injection.\n\n' +
                 'This is a one-time setup that requires your password:\n' +
                 '  • Adds your user to the "input" group\n' +
                 '  • Enables the virtual keyboard device (/dev/uinput)\n\n' +
                 'You will need to log out and back in for the change to take effect.',
        buttons: ['Grant Access', 'Skip'],
        defaultId: 0,
        cancelId: 1,
    }).then((result) => {
        if (result.response === 0) {
            // Single pkexec call with a shell script that does both steps
            const { exec } = require('child_process');
            const setupCmd = `pkexec sh -c '` +
                `usermod -aG input ${currentUser} && ` +
                `echo "KERNEL==\\"uinput\\", GROUP=\\"input\\", MODE=\\"0660\\"" > /etc/udev/rules.d/99-coral-uinput.rules && ` +
                `udevadm control --reload-rules && ` +
                `udevadm trigger /dev/uinput` +
                `'`;
            exec(setupCmd, (error) => {
                if (error) {
                    dialog.showErrorBox('Permission Error',
                        'Failed to set up input permissions:\n' + error.message +
                        '\n\nYou can do it manually:\n' +
                        'sudo usermod -aG input ' + currentUser + '\n' +
                        'echo \'KERNEL=="uinput", GROUP="input", MODE="0660"\' | sudo tee /etc/udev/rules.d/99-coral-uinput.rules\n' +
                        'sudo udevadm control --reload-rules && sudo udevadm trigger /dev/uinput');
                    stopBackend();
                    app.quit();
                } else {
                    dialog.showMessageBox({
                        type: 'info',
                        title: 'Setup Complete',
                        message: 'Input permissions have been configured.\n\n' +
                                 'Please reboot your computer for the changes to take effect,\n' +
                                 'then start Coral again.',
                        buttons: ['Reboot Now', 'Later'],
                        defaultId: 0,
                        cancelId: 1,
                    }).then((res) => {
                        stopBackend();
                        if (res.response === 0) {
                            const { exec } = require('child_process');
                            exec('systemctl reboot');
                        }
                        app.quit();
                    });
                }
            });
        } else {
            // User clicked Skip — quit anyway since the app won't work without permissions
            stopBackend();
            app.quit();
        }
    });
}

function startBackend() {
    if (isChildAlive(backendProcess)) {
        console.log('Backend already running in this Electron process; skipping spawn.');
        return;
    }
    let coralBinary, configPath, backendCwd, backendEnv;
    const isWin = process.platform === 'win32';
    const userConfigDir = path.join(os.homedir(), '.coral');
    const userConfigPath = path.join(userConfigDir, 'conf', 'config.json');

    if (process.env.APPIMAGE)
    {
        // AppImage: binary is in usr/bin/coral
        const appImageMountPath = getAppImageMountPath();
        coralBinary = path.join(appImageMountPath, 'usr', 'bin', 'coral');
        const defaultConfigPath = path.join(appImageMountPath, 'usr', 'share', 'coral', 'conf', 'config.json');
        try {
            appendElectronLog('Checking if user config directory exists:', userConfigDir);
            if (!fs.existsSync(path.dirname(userConfigPath))) {
                fs.mkdirSync(path.dirname(userConfigPath), { recursive: true });
                console.log('Created user config directory:', userConfigDir);
            }
            appendElectronLog('Checking if user config exists:', userConfigPath);
            appendElectronLog('Checking if default config exists:', defaultConfigPath);
            if (!fs.existsSync(userConfigPath) && fs.existsSync(defaultConfigPath)) {
                fs.copyFileSync(defaultConfigPath, userConfigPath);
                console.log('Copied default config to user config:', defaultConfigPath, '->', userConfigPath);
            } else {
                if (fs.existsSync(userConfigPath)) {
                    console.log('User config already exists:', userConfigPath);
                }
                if (!fs.existsSync(defaultConfigPath)) {
                    console.log('Default config does not exist:', defaultConfigPath);
                }
            }
        } catch (e) {
            console.error('Failed to prepare user config:', e.message);
        }
        configPath = userConfigPath;
        backendCwd = path.join(appImageMountPath, 'usr', 'bin');
        backendEnv = {
            ...process.env,
            LD_LIBRARY_PATH: path.join(appImageMountPath, 'usr', 'lib') + (process.env.LD_LIBRARY_PATH ? (':' + process.env.LD_LIBRARY_PATH) : '')
        };
    }
    else if (isWin)
    {
        // Windows: packaged (MSI) vs development
        if (app.isPackaged) {
            // Installed app: coral.exe and resources are in process.resourcesPath
            coralBinary = path.join(process.resourcesPath, 'coral.exe');
            if (!fs.existsSync(coralBinary)) {
                coralBinary = path.join(path.dirname(process.execPath), 'resources', 'coral.exe');
            }
        } else {
            // Development: coral.exe in build-win/Release
            const repoRoot = path.join(__dirname, '..');
            coralBinary = path.join(repoRoot, 'build-win', 'Release', 'coral.exe');
            if (!fs.existsSync(coralBinary)) {
                coralBinary = path.join(repoRoot, 'build-win', 'coral', 'Release', 'coral.exe');
            }
            if (!fs.existsSync(coralBinary)) {
                const { execSync } = require('child_process');
                try {
                    const found = execSync(`dir /s /b "${path.join(repoRoot, 'build-win')}\\coral.exe" 2>nul`, { encoding: 'utf-8' }).trim().split('\n')[0];
                    if (found && fs.existsSync(found)) coralBinary = found.trim();
                } catch (_) {}
            }
        }
        const defaultConfigSrc = app.isPackaged
            ? path.join(process.resourcesPath, 'conf', 'config.json')
            : path.join(__dirname, '..', 'coral', 'conf', process.platform === 'win32' ? 'config.json' : 'config-linux.json');
        try {
            if (!fs.existsSync(path.dirname(userConfigPath))) {
                fs.mkdirSync(path.dirname(userConfigPath), { recursive: true });
                console.log('Created user config directory:', userConfigDir);
            }
            if (!fs.existsSync(userConfigPath) && fs.existsSync(defaultConfigSrc)) {
                fs.copyFileSync(defaultConfigSrc, userConfigPath);
                console.log('Copied default config to user config (first run):', defaultConfigSrc, '->', userConfigPath);
            }
        } catch (e) {
            console.error('Failed to prepare user config:', e.message);
        }
        configPath = userConfigPath;
        backendCwd = path.dirname(coralBinary);
        backendEnv = { ...process.env };
    }
    else if (process.platform === 'linux' && !process.env.APPIMAGE &&
        fs.existsSync(path.join(__dirname, 'usr', 'bin', 'coral')))
    {
        // Linux .deb (or same layout as AppImage): bundled coral under ./usr/bin; whisper/ggml in ./usr/lib
        const appRoot = __dirname;
        coralBinary = path.join(appRoot, 'usr', 'bin', 'coral');
        const defaultConfigPath = path.join(appRoot, 'usr', 'share', 'coral', 'conf', 'config.json');
        try {
            if (!fs.existsSync(path.dirname(userConfigPath))) fs.mkdirSync(path.dirname(userConfigPath), { recursive: true });
            if (!fs.existsSync(userConfigPath) && fs.existsSync(defaultConfigPath)) {
                fs.copyFileSync(defaultConfigPath, userConfigPath);
                console.log('Copied default config (packaged Linux):', defaultConfigPath, '->', userConfigPath);
            }
        } catch (e) { console.error('Failed to prepare user config:', e.message); }
        configPath = userConfigPath;
        backendCwd = path.dirname(coralBinary);
        const bundledLib = path.join(appRoot, 'usr', 'lib');
        backendEnv = {
            ...process.env,
            LD_LIBRARY_PATH: bundledLib + (process.env.LD_LIBRARY_PATH ? (':' + process.env.LD_LIBRARY_PATH) : '')
        };
    }
    else
    {
        // Linux development: use ~/.coral/config.json, copy from config-linux.json on first run
        coralBinary = path.join(__dirname, '..', 'coral', 'bin', 'coral');
        const defaultConfigSrc = path.join(__dirname, '..', 'coral', 'conf', 'config-linux.json');
        try {
            if (!fs.existsSync(path.dirname(userConfigPath))) fs.mkdirSync(path.dirname(userConfigPath), { recursive: true });
            if (!fs.existsSync(userConfigPath) && fs.existsSync(defaultConfigSrc)) {
                fs.copyFileSync(defaultConfigSrc, userConfigPath);
            }
        } catch (e) { console.error('Failed to prepare user config:', e.message); }
        configPath = userConfigPath;
        backendCwd = path.dirname(coralBinary);
        backendEnv = process.env;
    }
    // Ensure DISPLAY is set for X11 injection (Linux only)
    if (!isWin) {
        backendEnv.DISPLAY = process.env.DISPLAY || ':0';
    }
    backendProcess = spawn(coralBinary, [configPath], {
        cwd: backendCwd,
        detached: false,
        stdio: ['ignore', 'pipe', 'ignore'], // Enable stdout pipe
        env: backendEnv
    });
    backendStopping = false;
    // Listen for trigger events from backend
    if (backendRl) { try { backendRl.close(); } catch (_) {} }
    backendRl = readline.createInterface({ input: backendProcess.stdout });
    backendRl.on('line', (line) => {
        if (backendStopping) return;
        const trimmed = line.trim();
        if (trimmed === 'TRIGGER_DOWN') {
            clearContinuousModeTimers();
            startTrayAnimation(animationFrames);
        } else if (trimmed === 'TRIGGER_UP') {
            if (currentTriggerMode === 'pushToTalk') {
                startTrayAnimation(transcribeAnimationFrames);
            } else {
                // Continuous mode: 300ms to distinguish chunk restart from user stop
                clearContinuousModeTimers();
                stopAnimationTimer = setTimeout(() => {
                    stopAnimationTimer = null;
                    // User stopped; keep animation on until last TRANSCRIBING_DONE
                    lastTranscriptionTimer = setTimeout(() => {
                        stopTrayAnimation();
                        lastTranscriptionTimer = null;
                        showTranscriptionDoneNotification();
                    }, 2000);
                }, 300);
            }
        } else if (trimmed === 'TRANSCRIBING_START') {
            transcribingStartMs = Date.now();
        } else if (trimmed === 'INJECT_PENDING') {
            if (transcribingStartMs != null) {
                const whisperMs = Date.now() - transcribingStartMs;
                pendingWhisperMs.push(whisperMs);
                transcribingStartMs = null;
            } else {
                console.warn('[Coral] INJECT_PENDING without TRANSCRIBING_START');
                pendingWhisperMs.push(-1);
            }
            pendingInjectionStarts.push(Date.now());
        } else if (trimmed === 'INJECTION_DONE') {
            const t0 = pendingInjectionStarts.shift();
            const whisperMs = pendingWhisperMs.shift();
            if (t0 !== undefined && whisperMs !== undefined && whisperMs >= 0) {
                const injectMs = Date.now() - t0;
                appendDevTimingStats(whisperMs, injectMs);
            } else if (t0 !== undefined) {
                const injectMs = Date.now() - t0;
                appendDevTimingStats(-1, injectMs);
            } else {
                console.warn('[Coral] INJECTION_DONE without matching INJECT_PENDING');
            }
        } else if (trimmed === 'TRANSCRIBING_DONE') {
            if (transcribingStartMs != null) {
                const whisperMs = Date.now() - transcribingStartMs;
                transcribingStartMs = null;
                appendDevTimingStats(whisperMs, -1);
            }
            if (currentTriggerMode === 'pushToTalk') {
                stopTrayAnimation();
                showTranscriptionDoneNotification();
            } else if (lastTranscriptionTimer) {
                // Continuous mode: reset timer; stop when no TRANSCRIBING_DONE for 2s
                clearTimeout(lastTranscriptionTimer);
                lastTranscriptionTimer = setTimeout(() => {
                    stopTrayAnimation();
                    lastTranscriptionTimer = null;
                }, 2000);
            }
        } else if (trimmed === 'NEED_INPUT_GROUP' || trimmed === 'NEED_UINPUT_RULE') {
            closeWelcomeWindow();
            promptInputGroupPermission();
        } else if (trimmed === 'BACKEND_READY') {
            readTriggerMode();
            updateWelcomeReady();
        } else if (trimmed === 'CONFIG_RELOADED') {
            readTriggerMode();
            console.log('Backend confirmed config reload');
        }
    });
    backendRl.on('error', (err) => {
      if (!backendStopping) console.error('Backend readline error:', err.message);
    });
    backendProcess.stdout.on('error', (err) => {
      if (!backendStopping) console.error('Backend stdout error:', err.message);
    });
    backendProcess.on('error', (err) => {
        console.error('Backend error:', err.message);
    });
    backendProcess.on('exit', (code, signal) => {
        console.log('Backend process exited with code:', code, 'signal:', signal);
    });
    if (!backendProcess.pid) {
      console.error('Backend process failed to start.');
    }
}

function stopBackend() {
  backendStopping = true;
  pendingInjectionStarts.length = 0;
  pendingWhisperMs.length = 0;
  transcribingStartMs = null;
  clearContinuousModeTimers();
  try { stopTrayAnimation(); } catch (_) {}
  if (backendRl) {
    try { backendRl.close(); } catch (_) {}
    backendRl = null;
  }
  if (backendProcess) {
    try { backendProcess.stdout.removeAllListeners(); } catch (_) {}
    try { backendProcess.removeAllListeners(); } catch (_) {}
    try { backendProcess.kill(); } catch (_) {}
    backendProcess = null;
  }
}

async function restartBackend() {
  try {
    stopBackend();
  } catch (_) {}
  // slight delay to allow OS to release resources
  setTimeout(() => {
    try { startBackend(); } catch (e) { console.error('Failed to restart backend:', e.message); }
  }, 500);
}

let welcomeWindow = null;

function showWelcomeWindow() {
  if (welcomeWindow) return;
  welcomeWindow = new BrowserWindow({
    width: 460,
    height: 300,
    resizable: false,
    minimizable: false,
    maximizable: false,
    alwaysOnTop: true,
    frame: false,
    show: false,
    transparent: false,
    skipTaskbar: true,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
    },
  });
  welcomeWindow.once('ready-to-show', () => {
    welcomeWindow.show();
  });
  welcomeWindow.on('closed', () => { welcomeWindow = null; });

  let triggerKeyDisplay = 'Ctrl';
  try {
    const cfgPath = path.join(os.homedir(), '.coral', 'conf', 'config.json');
    const cfg = JSON.parse(fs.readFileSync(cfgPath, 'utf8'));
    if (cfg.triggerKey) triggerKeyDisplay = cfg.triggerKey;
  } catch (_) {}

  const welcomeHtml = `<html><head><meta charset="UTF-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline' https://fonts.googleapis.com; font-src https://fonts.gstatic.com; img-src data:; script-src 'unsafe-inline'; connect-src https://fonts.googleapis.com https://fonts.gstatic.com; base-uri 'none'">
<title>Coral</title><style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;800&display=swap');
    * { margin:0; padding:0; box-sizing:border-box; }
    body { font-family:'Inter',system-ui,sans-serif; height:100vh; overflow:hidden;
           background:linear-gradient(135deg,#f0f4ff 0%,#e8f5e9 40%,#e0f2f1 100%); }
    .scene { position:absolute; bottom:0; width:100%; height:55%; }
    .wave { position:absolute; width:200%; left:-50%; border-radius:45%; }
    .w1 { bottom:-60%; height:100%; background:rgba(13,71,161,0.07); animation:sway 8s ease-in-out infinite; }
    .w2 { bottom:-65%; height:100%; background:rgba(13,71,161,0.05); animation:sway 6s ease-in-out infinite reverse; }
    .w3 { bottom:-70%; height:100%; background:rgba(0,150,136,0.04); animation:sway 10s ease-in-out infinite; }
    @keyframes sway { 0%,100%{transform:translateX(-2%)} 50%{transform:translateX(2%)} }
    .coral-shape { position:absolute; border-radius:50%; }
    .c1 { width:80px; height:80px; background:rgba(0,150,136,0.12); bottom:20px; right:40px; }
    .c2 { width:50px; height:50px; background:rgba(13,71,161,0.1); bottom:50px; right:100px; }
    .c3 { width:35px; height:35px; background:rgba(76,175,80,0.1); bottom:10px; right:160px; }
    .c4 { width:60px; height:60px; background:rgba(0,150,136,0.1); bottom:30px; left:30px; }
    .c5 { width:25px; height:25px; background:rgba(76,175,80,0.08); bottom:60px; left:80px; }
    .content { position:relative; z-index:1; padding:36px 40px; height:100%; display:flex; flex-direction:column; }
    .brand { font-size:42px; font-weight:800; letter-spacing:-1px;
             background:linear-gradient(135deg,#0d47a1,#00897b); -webkit-background-clip:text; -webkit-text-fill-color:transparent; }
    .version { font-size:14px; color:#5c6bc0; font-weight:600; margin-top:4px; letter-spacing:0.5px; }
    .tagline { margin-top:24px; font-size:15px; color:#455a64; font-weight:300; line-height:1.6; max-width:280px; }
    .tagline strong { font-weight:600; color:#0d47a1; }
    .bottom { margin-top:auto; }
    .status { font-size:13px; color:#90a4ae; }
    .status.ready { color:#2e7d32; font-weight:600; }
    .ok-btn { margin-top:12px; padding:8px 28px; font-size:13px; font-weight:600;
              border-radius:8px; border:none; background:linear-gradient(135deg,#0d47a1,#1565c0);
              color:#fff; cursor:pointer; display:none; transition:opacity 0.2s; }
    .ok-btn:hover { opacity:0.85; }
    @keyframes dots { 0%{content:''} 33%{content:'.'} 66%{content:'..'} 100%{content:'...'} }
    .loading::after { content:''; animation:dots 1.2s steps(4,end) infinite; }
    .copy { position:absolute; bottom:12px; right:16px; font-size:11px; color:#b0bec5; z-index:1; }
  </style></head><body>
    <div class="scene">
      <div class="wave w1"></div><div class="wave w2"></div><div class="wave w3"></div>
      <div class="coral-shape c1"></div><div class="coral-shape c2"></div>
      <div class="coral-shape c3"></div><div class="coral-shape c4"></div><div class="coral-shape c5"></div>
    </div>
    <div class="content">
      <div class="brand">Coral</div>
      <div class="version">${app.getVersion()}</div>
      <div class="tagline"><strong>Double tap</strong> trigger key (${triggerKeyDisplay}), <strong>speak</strong>, get the <strong>text</strong></div>
      <div class="bottom">
        <div class="status loading" id="status">Starting up</div>
        <button class="ok-btn" id="okBtn" onclick="window.close()">Got it</button>
      </div>
    </div>
    <div class="copy">&copy; 2025 Coral Contributors</div>
  </body></html>`;
  welcomeWindow.loadURL('data:text/html;charset=utf-8,' + encodeURIComponent(welcomeHtml));
}

function updateWelcomeReady() {
  if (!welcomeWindow) return;
  try {
    welcomeWindow.webContents.executeJavaScript(`
      (function () {
        var s = document.getElementById('status');
        var b = document.getElementById('okBtn');
        if (s) { s.className = 'status ready'; s.textContent = 'Ready!'; }
        if (b) { b.style.display = 'inline-block'; }
      })();
    `);
    // Auto-close after 5 seconds if the user doesn't click OK
    setTimeout(() => {
      try { if (welcomeWindow) welcomeWindow.close(); } catch (_) {}
    }, 3000);
  } catch (_) {}
}

function closeWelcomeWindow() {
  try { if (welcomeWindow) welcomeWindow.close(); } catch (_) {}
}

const gotLock = app.requestSingleInstanceLock();

if (!gotLock) {
  app.quit();
} else {
  app.on('second-instance', () => {
    if (configWindow) {
      if (configWindow.isMinimized()) configWindow.restore();
      configWindow.focus();
    }
  });
}

app.whenReady().then(() => {
  readRecordDevTimingStats();
  // Remove the default application menu (File/Edit/View/Window/Help)
  Menu.setApplicationMenu(null);
  // Tray icon - ensure path exists (Windows may fail silently with bad path)
  if (!fs.existsSync(originalIconPath)) {
    console.warn('Tray icon not found at:', originalIconPath, '- tray may not display correctly');
  }
  try {
    tray = new Tray(originalIconPath);
  } catch (e) {
    console.error('Failed to create tray:', e.message);
  }
  if (!tray) {
    dialog.showErrorBox('Coral', 'Could not create system tray icon. Ensure coral.png exists in coral-electron or logo folder.');
    app.quit();
    return;
  }
  const contextMenu = Menu.buildFromTemplate([
    { label: 'Settings', click: createConfigWindow },
    { label: 'Developer Settings', click: createDevWindow },
    { label: 'About', click: createAboutWindow },
    { type: 'separator' },
    { label: 'Quit', click: () => { stopBackend(); app.quit(); } },
  ]);
  tray.setToolTip('Coral App');
  tray.setContextMenu(contextMenu);
  showWelcomeWindow();
  startBackend();
});

app.on('window-all-closed', (e) => {
  // Prevent app from quitting when config window is closed
  e.preventDefault();
});

app.on('before-quit', () => {
  stopBackend();
});

// Add IPC handler for Spectron and other integration tests
ipcMain.on('show-config', () => {
  createConfigWindow();
});

// Signal backend to reload config: SIGUSR1 on Linux/macOS, restart on Windows
ipcMain.on('config-updated', () => {
  readRecordDevTimingStats();
  if (process.platform !== 'win32' && backendProcess && backendProcess.pid && !backendProcess.killed) {
    try {
      process.kill(backendProcess.pid, 'SIGUSR1');
      console.log('Sent SIGUSR1 to backend PID:', backendProcess.pid);
    } catch (e) {
      console.error('Failed to send SIGUSR1, falling back to restart:', e.message);
      restartBackend();
    }
  } else {
    restartBackend();
  }
});

// Allow renderers to request window resize to fit content
ipcMain.on('resize-window', (event, contentHeight, contentWidth) => {
  try {
    const win = BrowserWindow.fromWebContents(event.sender);
    if (!win) return;
    const [currentWidth, currentHeight] = win.getContentSize();
    const minH = 200;
    const minW = 540;
    const display = screen.getDisplayMatching(win.getBounds()) || screen.getPrimaryDisplay();
    const workH = (display && display.workAreaSize && display.workAreaSize.height) ? display.workAreaSize.height : 900;
    const workW = (display && display.workAreaSize && display.workAreaSize.width) ? display.workAreaSize.width : 1600;
    const maxH = Math.max(320, workH - 60);
    const maxW = Math.min(workW - 40, 800);
    const paddedH = Math.max(minH, Math.min(maxH, Math.ceil(Number(contentHeight) || 0) + 12));
    const paddedW = contentWidth != null ? Math.max(minW, Math.min(maxW, Math.ceil(Number(contentWidth) || 0) + 24)) : currentWidth;
    // Never shrink: only expand to fit content (avoids shrink when dropdown closes)
    const newW = Math.max(currentWidth, paddedW, minW);
    const newH = Math.max(currentHeight, paddedH);
    win.setContentSize(newW, newH);
    if (!win.isVisible()) win.show();
  } catch (_) {}
});

// IPC handler for folder selection
ipcMain.handle('select-folder', async () => {
  const result = await dialog.showOpenDialog({
    properties: ['openDirectory']
  });
  if (result.canceled || !result.filePaths.length) return null;
  return result.filePaths[0];
});

// IPC handler for selecting a model file instead of a folder
ipcMain.handle('select-model-file', async () => {
  const result = await dialog.showOpenDialog({
    properties: ['openFile'],
    filters: [
      { name: 'Whisper models', extensions: ['bin', 'gguf'] },
      { name: 'All Files', extensions: ['*'] }
    ]
  });
  if (result.canceled || !result.filePaths.length) return null;
  return result.filePaths[0];
});
