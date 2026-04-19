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

/** Append one line to ~/.coral/logs/corallectron.log (separate file for the Electron main process). */
function appendElectronLog(...args) {
  try {
    const logDir = path.join(os.homedir(), '.coral', 'logs');
    if (!fs.existsSync(logDir)) fs.mkdirSync(logDir, { recursive: true });
    const logFile = path.join(logDir, 'corallectron.log');
    const msg = args.map((a) => (typeof a === 'object' ? JSON.stringify(a) : String(a))).join(' ');
    const line = `${new Date().toISOString()} [electron] ${msg}\n`;
    fs.appendFileSync(logFile, line, 'utf8');
  } catch (_) { /* ignore log failures */ }
}

// Tee console.log/info/warn/error into corallectron.log while still echoing to stdout/stderr.
// This captures ad-hoc console calls throughout main.js (e.g. "[Coral] INJECT_PENDING without TRANSCRIBING_START")
// into the log file without having to rewrite each call site.
(function teeConsoleToElectronLog() {
  const levelTag = { log: 'INFO', info: 'INFO', warn: 'WARN', error: 'ERROR' };
  for (const method of Object.keys(levelTag)) {
    const original = console[method].bind(console);
    console[method] = (...args) => {
      original(...args);
      appendElectronLog(`[${levelTag[method]}]`, ...args);
    };
  }
})();

/**
 * Ensure ~/.coral/models exists and contains every bundled .bin model from the
 * system install dir, preferably as symlinks (zero disk overhead) and falling
 * back to file copies when symlinks are unavailable.
 *
 * Why both:
 *   - Linux: postinst already symlinks for $SUDO_USER. This is the safety net
 *     for other users on the box, AppImage runs (no postinst), and dev runs.
 *   - Windows: there's no postinst equivalent, AND fs.symlinkSync requires
 *     admin or Developer Mode. Plain end-users get EPERM, so we copy instead.
 *
 * Idempotent: pre-existing files/symlinks at the destination are left alone.
 */
function seedUserModelsDir() {
  try {
    const home = os.homedir();
    if (!home) return;
    const userDir = path.join(home, '.coral', 'models');

    // Candidate source dirs, in priority order. First one with .bin files wins.
    // Order matters: more-specific (packaged) paths before dev fallback.
    const candidates = [];
    try { candidates.push(path.join(app.getAppPath(), 'usr', 'share', 'coral', 'models')); } catch (_) {}
    if (process.platform === 'win32') {
      // Windows installers stage models under <exe>\model (singular) or
      // <exe>\models. See build-win-msi.bat / build-windows-bundle.cmd.
      try {
        const exeDir = path.dirname(process.execPath);
        candidates.push(path.join(exeDir, 'model'));
        candidates.push(path.join(exeDir, 'models'));
        candidates.push(path.join(exeDir, 'resources', 'model'));
        candidates.push(path.join(exeDir, 'resources', 'models'));
      } catch (_) {}
      try { candidates.push(path.join(app.getAppPath(), 'model')); } catch (_) {}
      try { candidates.push(path.join(app.getAppPath(), 'models')); } catch (_) {}
    } else {
      candidates.push('/opt/coral/usr/share/coral/models');
      candidates.push('/usr/share/coral/models');
      if (process.env.APPDIR) {
        candidates.push(path.join(process.env.APPDIR, 'usr', 'share', 'coral', 'models'));
      }
      try {
        const exeDir = path.dirname(process.execPath);
        candidates.push(path.join(exeDir, '..', 'share', 'coral', 'models'));
      } catch (_) {}
    }
    // Repo dev fallback: <repo>/models/
    candidates.push(path.resolve(__dirname, '..', 'models'));

    let srcDir = null;
    for (const c of candidates) {
      try {
        if (!fs.existsSync(c)) continue;
        if (!fs.statSync(c).isDirectory()) continue;
        const hasBin = fs.readdirSync(c).some((n) => /\.bin$/i.test(n));
        if (hasBin) { srcDir = c; break; }
      } catch (_) { /* skip */ }
    }
    if (!srcDir) {
      console.warn('[Coral] seedUserModelsDir: no bundled models dir found; skipping');
      return;
    }

    if (!fs.existsSync(userDir)) fs.mkdirSync(userDir, { recursive: true });

    // On Windows, jump straight to copy — symlinkSync almost always fails for
    // non-admin users. Elsewhere, try symlink first and only copy as fallback.
    const preferCopy = process.platform === 'win32';
    let symlinked = 0;
    let copied = 0;

    for (const name of fs.readdirSync(srcDir)) {
      if (!name.toLowerCase().endsWith('.bin')) continue;
      const src = path.join(srcDir, name);
      const dst = path.join(userDir, name);
      // lstatSync detects any pre-existing entry (regular file, symlink, even
      // a broken symlink); leave it alone in all those cases.
      let exists = false;
      try { fs.lstatSync(dst); exists = true; } catch (_) { /* not present */ }
      if (exists) continue;

      let placed = null;
      if (!preferCopy) {
        try { fs.symlinkSync(src, dst); placed = 'symlink'; } catch (_) { /* fall through to copy */ }
      }
      if (!placed) {
        try { fs.copyFileSync(src, dst); placed = 'copy'; }
        catch (e) {
          console.warn('[Coral] seedUserModelsDir: failed to place', src, '->', dst, ':', e.message);
        }
      }
      if (placed === 'symlink') symlinked++;
      else if (placed === 'copy') copied++;
    }
    if (symlinked + copied > 0) {
      console.log('[Coral] seedUserModelsDir:',
        symlinked, 'symlink(s),', copied, 'copy/copies in', userDir, 'from', srcDir);
    }
  } catch (e) {
    console.error('[Coral] seedUserModelsDir failed:', e && e.message);
  }
}
seedUserModelsDir();

/**
 * On Windows, the MSI does NOT bundle any whisper .bin model (keeps installer
 * ~100MB instead of ~760MB). On first launch we fetch the configured default
 * model from Hugging Face into ~/.coral/models/. Subsequent launches see the
 * file and skip the download.
 *
 * Linux/macOS users get models via their package (.deb postinst symlinks) or
 * via seedUserModelsDir() above, so this is a Windows-only fast-path.
 *
 * Returns a Promise that resolves when a model is available (download
 * completed, or already present, or platform doesn't need it). Rejects only
 * on a hard download failure with no model present at all.
 */
function ensureDefaultModelDownloaded(progressCb) {
  return new Promise((resolve, reject) => {
    if (process.platform !== 'win32') return resolve({ skipped: true });

    const home = os.homedir();
    if (!home) return resolve({ skipped: true });

    const userDir = path.join(home, '.coral', 'models');
    try {
      if (!fs.existsSync(userDir)) fs.mkdirSync(userDir, { recursive: true });
    } catch (e) {
      return reject(new Error('Could not create ' + userDir + ': ' + e.message));
    }

    // Already have a usable transcription model? Skip download.
    let existing = [];
    try {
      existing = fs.readdirSync(userDir).filter(
        (n) => /^ggml-.*\.bin$/i.test(n) && !/silero/i.test(n)
      );
    } catch (_) { /* dir unreadable; treat as empty */ }
    if (existing.length > 0) {
      console.log('[Coral] ensureDefaultModelDownloaded: already have', existing.length, 'model(s); skipping');
      return resolve({ skipped: true, existing });
    }

    // Read the configured default from the user/system config so we download
    // whatever the C++ backend will actually try to load.
    let defaultName = 'ggml-small.en-q8_0.bin';
    try {
      const cfgPath = path.join(home, '.coral', 'conf', 'config.json');
      if (fs.existsSync(cfgPath)) {
        const cfg = JSON.parse(fs.readFileSync(cfgPath, 'utf8'));
        if (typeof cfg.whisperModelPath === 'string'
            && /^ggml-.*\.bin$/i.test(cfg.whisperModelPath)
            && !cfg.whisperModelPath.includes('/')
            && !cfg.whisperModelPath.includes('\\')) {
          defaultName = cfg.whisperModelPath;
        }
      }
    } catch (_) { /* keep fallback */ }

    const url = `https://huggingface.co/ggerganov/whisper.cpp/resolve/main/${defaultName}`;
    const dst = path.join(userDir, defaultName);
    const tmp = dst + '.partial';

    console.log('[Coral] First-run model download:', defaultName, 'from', url);

    let out;
    try { out = fs.createWriteStream(tmp); }
    catch (e) { return reject(new Error('Could not open ' + tmp + ': ' + e.message)); }

    const cleanupAndFail = (err) => {
      try { out.destroy(); } catch (_) {}
      try { if (fs.existsSync(tmp)) fs.unlinkSync(tmp); } catch (_) {}
      reject(err);
    };

    const https = require('https');
    const MAX_REDIRECTS = 6;
    const startedAt = Date.now();

    const doRequest = (currentUrl, redirectsLeft) => {
      const req = https.get(currentUrl, (res) => {
        // Hugging Face issues 302/307 to a signed S3/CDN URL — follow it.
        if ([301, 302, 303, 307, 308].includes(res.statusCode)) {
          if (redirectsLeft <= 0) {
            return cleanupAndFail(new Error('Too many redirects'));
          }
          const next = res.headers.location;
          res.resume();
          if (!next) return cleanupAndFail(new Error('Redirect with no Location header'));
          return doRequest(next, redirectsLeft - 1);
        }
        if (res.statusCode !== 200) {
          return cleanupAndFail(new Error('HTTP ' + res.statusCode + ' fetching ' + currentUrl));
        }
        const total = parseInt(res.headers['content-length'] || '0', 10);
        let received = 0;
        let lastReportedPct = -1;

        res.on('data', (chunk) => {
          received += chunk.length;
          if (typeof progressCb === 'function' && total > 0) {
            const pct = Math.floor((received * 100) / total);
            if (pct !== lastReportedPct) {
              lastReportedPct = pct;
              try { progressCb({ filename: defaultName, received, total, pct, done: false }); }
              catch (_) { /* progress reporter must never break the download */ }
            }
          }
        });

        res.pipe(out);
        out.on('finish', () => {
          out.close(() => {
            try { fs.renameSync(tmp, dst); }
            catch (e) { return cleanupAndFail(new Error('Could not move ' + tmp + ' -> ' + dst + ': ' + e.message)); }
            const tookMs = Date.now() - startedAt;
            console.log('[Coral] Model downloaded:', dst, 'in', tookMs, 'ms');
            if (typeof progressCb === 'function') {
              try { progressCb({ filename: defaultName, received: total || received, total: total || received, pct: 100, done: true }); }
              catch (_) {}
            }
            resolve({ downloaded: defaultName, path: dst, bytes: received, durationMs: tookMs });
          });
        });
        res.on('error', cleanupAndFail);
      });
      req.on('error', cleanupAndFail);
      req.setTimeout(30000, () => {
        req.destroy(new Error('Connection timed out fetching ' + currentUrl));
      });
    };

    doRequest(url, MAX_REDIRECTS);
  });
}

/**
 * Tiny first-run window that shows the model download progress. Inline HTML
 * (no preload) — main process pushes progress updates via executeJavaScript.
 *
 * Returns { window, update(progress), close() }.
 */
function showDownloadProgressWindow() {
  const win = new BrowserWindow({
    width: 460,
    height: 220,
    resizable: false,
    minimizable: false,
    maximizable: false,
    alwaysOnTop: true,
    frame: false,
    show: false,
    skipTaskbar: true,
    webPreferences: { nodeIntegration: false, contextIsolation: true },
  });
  const html = `<!doctype html><html><head><meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; img-src data:; base-uri 'none'">
<title>Coral - First run</title>
<style>
  html,body { margin:0; padding:0; background:#1e1f22; color:#e6e6e6; font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif; height:100%; }
  .wrap { padding:22px 26px; }
  h1 { font-size:16px; font-weight:600; margin:0 0 6px 0; }
  .sub { font-size:12px; color:#9aa0a6; margin-bottom:18px; }
  .bar { width:100%; height:10px; background:#2a2c31; border-radius:6px; overflow:hidden; }
  .fill { height:100%; width:0%; background:linear-gradient(90deg,#5db0ff,#7c5cff); transition:width .15s ease; }
  .meta { display:flex; justify-content:space-between; margin-top:10px; font-size:12px; color:#bdc1c6; }
  #pct { font-variant-numeric:tabular-nums; }
  #size { font-variant-numeric:tabular-nums; }
</style></head><body><div class="wrap">
  <h1>Setting up Coral</h1>
  <div class="sub" id="msg">Downloading the default Whisper model. This happens once.</div>
  <div class="bar"><div class="fill" id="fill"></div></div>
  <div class="meta"><span id="pct">0%</span><span id="size"></span></div>
</div>
<script>
  function fmtBytes(b){ if(!b||b<1024) return (b||0)+' B'; const u=['KB','MB','GB']; let i=-1; do{b/=1024;i++;}while(b>=1024&&i<u.length-1); return b.toFixed(1)+' '+u[i]; }
  window.coralUpdate = function(p){
    var fill=document.getElementById('fill'), pct=document.getElementById('pct'), sz=document.getElementById('size'), msg=document.getElementById('msg');
    var v = Math.max(0, Math.min(100, p.pct||0));
    fill.style.width = v + '%';
    pct.textContent = v + '%';
    if (p.total) sz.textContent = fmtBytes(p.received||0) + ' / ' + fmtBytes(p.total);
    if (p.done) { msg.textContent = 'Done. Starting Coral…'; }
    if (p.filename) { document.title = 'Coral - downloading ' + p.filename; }
  };
</script></body></html>`;
  win.loadURL('data:text/html;charset=utf-8,' + encodeURIComponent(html));
  win.once('ready-to-show', () => win.show());

  return {
    window: win,
    update: (p) => {
      if (win.isDestroyed()) return;
      const json = JSON.stringify(p);
      win.webContents.executeJavaScript('window.coralUpdate && window.coralUpdate(' + json + ')').catch(() => {});
    },
    close: () => { try { if (!win.isDestroyed()) win.close(); } catch (_) {} },
  };
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
                // Benign accounting drift — happens when an empty/skipped chunk
                // goes straight to inject, or when continuous mode restarts
                // mid-stream. Only relevant for dev-timing stats; route to the
                // log file to keep the console clean.
                if (recordDevTimingStats) {
                    appendElectronLog('[WARN] [Coral] INJECT_PENDING without TRANSCRIBING_START');
                }
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
            } else if (recordDevTimingStats) {
                // Same accounting drift — log file only, dev-stats only.
                appendElectronLog('[WARN] [Coral] INJECTION_DONE without matching INJECT_PENDING');
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

app.whenReady().then(async () => {
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

  // Windows-only first-run model download. On Linux/macOS this is a no-op
  // (returns immediately) — packaging already provides the model.
  if (process.platform === 'win32') {
    const home = os.homedir();
    const userModels = home ? path.join(home, '.coral', 'models') : null;
    let needsDownload = true;
    try {
      if (userModels && fs.existsSync(userModels)) {
        needsDownload = !fs.readdirSync(userModels).some(
          (n) => /^ggml-.*\.bin$/i.test(n) && !/silero/i.test(n)
        );
      }
    } catch (_) { /* assume needs download */ }

    if (needsDownload) {
      const dl = showDownloadProgressWindow();
      try {
        await ensureDefaultModelDownloaded((p) => dl.update(p));
      } catch (e) {
        console.error('[Coral] First-run model download failed:', e.message);
        dl.close();
        const userModelsDisplay = userModels || '%USERPROFILE%\\.coral\\models';
        dialog.showErrorBox(
          'Coral - download failed',
          'Could not download the default Whisper model.\n\n' + e.message +
          '\n\nYou can manually place a ggml-*.bin file into:\n' + userModelsDisplay +
          '\nthen restart Coral.'
        );
        // Keep the app running with no model — Settings still opens, user can
        // pick "Custom…" or drop a file in. Backend will log a load error and
        // restart cleanly once a model appears.
      }
      // Tiny delay so the user actually sees the "Done" state.
      setTimeout(() => dl.close(), 600);
    }
  }

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

// IPC handler: list ggml-*.bin model files in ~/.coral/models/.
// That directory is populated by the .deb postinst (symlinks into the bundled
// /opt/coral/usr/share/coral/models) and by seedUserModelsDir() at startup.
// Keeping this single-source-of-truth keeps the UI predictable: whatever the
// user sees in ~/.coral/models is exactly what the dropdown offers.
ipcMain.handle('list-installed-models', () => {
  const home = os.homedir();
  if (!home) return [];
  const dir = path.join(home, '.coral', 'models');
  let entries;
  try {
    if (!fs.existsSync(dir)) return [];
    entries = fs.readdirSync(dir);
  } catch (_) { return []; }

  const results = [];
  for (const name of entries) {
    if (!/^ggml-.*\.bin$/i.test(name)) continue;
    // Skip the Silero VAD model — it's not a transcription model.
    if (/silero/i.test(name)) continue;
    const full = path.join(dir, name);
    try {
      // statSync follows symlinks, so size reflects the real model file.
      const st = fs.statSync(full);
      if (!st.isFile() || st.size === 0) continue;
      results.push({ filename: name, dir, fullPath: full, size: st.size });
    } catch (_) { /* dangling symlink or unreadable — skip */ }
  }
  results.sort((a, b) => a.filename.localeCompare(b.filename));
  return results;
});
