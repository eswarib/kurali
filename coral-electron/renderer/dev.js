const fs = require('fs');
const path = require('path');
const os = require('os');
const { ipcRenderer } = require('electron');

function getAppImageMountPath() {
  if (process.env.APPIMAGE) {
    return path.join(path.dirname(process.execPath), '..', '..', '..');
  }
  return null;
}

let configPath;
if (process.env.APPIMAGE) {
  const appImageMountPath = getAppImageMountPath();
  const defaultConfigPath = path.join(appImageMountPath, 'usr', 'share', 'coral', 'conf', 'config.json');
  const userConfigDir = path.join(os.homedir(), '.coral');
  const userConfigPath = path.join(userConfigDir, 'conf', 'config.json');
  try {
    if (!fs.existsSync(userConfigDir)) {
      fs.mkdirSync(userConfigDir, { recursive: true });
    }
    if (!fs.existsSync(userConfigPath) && fs.existsSync(defaultConfigPath)) {
      fs.copyFileSync(defaultConfigPath, userConfigPath);
    }
  } catch (e) {
    console.error('Failed to prepare user config:', e.message);
  }
  configPath = userConfigPath;
} else {
  const userConfigDir = path.join(os.homedir(), '.coral');
  configPath = path.join(userConfigDir, 'conf', 'config.json');
  const platformConfig = process.platform === 'win32' ? 'config.json' : 'config-linux.json';
  const devDefault = path.join(__dirname, '../..', 'coral', 'conf', platformConfig);
  try {
    if (!fs.existsSync(path.dirname(configPath))) fs.mkdirSync(path.dirname(configPath), { recursive: true });
    if (!fs.existsSync(configPath) && fs.existsSync(devDefault)) {
      fs.copyFileSync(devDefault, configPath);
    }
  } catch (e) { console.error('Failed to prepare user config:', e.message); }
}

const devForm = document.getElementById('devForm');
const devSaveBtn = document.getElementById('devSaveBtn');
const devStatus = document.getElementById('devStatus');

/** Human-readable labels for Developer Settings (key otherwise shown as-is) */
const DEV_FIELD_LABELS = {
  recordDevTimingStats: 'Append timing CSV (~/.coral/dev-timing.stats)',
};

let config = {};

function renderDevForm(cfg) {
  devForm.innerHTML = '';
  const primaryKeys = new Set(['triggerKey', 'cmdTriggerKey', 'triggerMode', 'whisperModelPath']);
  const hiddenKeys = new Set(['silenceTimeoutSeconds', 'audioSampleRate', 'audioChannels', 'audioAmplification', 'noiseGateThreshold']);
  for (const [key, value] of Object.entries(cfg)) {
    if (primaryKeys.has(key) || hiddenKeys.has(key)) continue;
    const field = document.createElement('div');
    field.className = 'field';
    const label = document.createElement('label');
    label.textContent = DEV_FIELD_LABELS[key] || key;
    let input;
    if (typeof value === 'boolean') {
      input = document.createElement('select');
      ['true', 'false'].forEach(opt => {
        const option = document.createElement('option');
        option.value = opt;
        option.text = opt;
        if (String(value) === opt) option.selected = true;
        input.appendChild(option);
      });
      input.name = key;
    } else if (typeof value === 'number') {
      input = document.createElement('input');
      input.type = 'number';
      input.value = value;
      input.name = key;
    } else {
      input = document.createElement('input');
      input.type = 'text';
      input.value = value;
      input.name = key;
    }
    label.className = 'field-label';
    const wrap = document.createElement('div');
    wrap.className = 'field-input';
    wrap.appendChild(input);
    field.appendChild(label);
    field.appendChild(wrap);
    devForm.appendChild(field);
  }
  requestResize();
}

function requestResize() {
  try {
    const measure = () => {
      const bodyH = document.body ? document.body.scrollHeight : 0;
      const docH = document.documentElement ? document.documentElement.scrollHeight : 0;
      return Math.ceil(Math.max(bodyH, docH, 300) + 24);
    };
    ipcRenderer.send('resize-window', measure());
    setTimeout(() => ipcRenderer.send('resize-window', measure()), 80);
    setTimeout(() => ipcRenderer.send('resize-window', measure()), 250);
  } catch (_) {}
}

window.addEventListener('load', () => {
  requestResize();
});

function loadConfig() {
  fs.readFile(configPath, 'utf8', (err, data) => {
    if (err) {
      devStatus.textContent = 'Failed to load config: ' + err.message;
      devStatus.className = 'error';
      return;
    }
    try {
      config = JSON.parse(data);
      if (config.showTranscriptionNotification === undefined) {
        config.showTranscriptionNotification = true;
      }
      if (config.saveAudioToFolder === undefined) {
        config.saveAudioToFolder = '';
      }
      if (config.recordDevTimingStats === undefined) {
        config.recordDevTimingStats = false;
      }
      renderDevForm(config);
      devStatus.textContent = '';
      devStatus.className = '';
    } catch (e) {
      devStatus.textContent = 'Invalid config.json: ' + e.message;
      devStatus.className = 'error';
    }
  });
}

devSaveBtn.onclick = (e) => {
  e.preventDefault();
  const newConfig = { ...config };
  for (const el of devForm.elements) {
    if (!el.name) continue;
    if (el.tagName === 'SELECT') {
      newConfig[el.name] = el.value === 'true';
    } else if (el.type === 'number') {
      newConfig[el.name] = Number(el.value);
    } else {
      newConfig[el.name] = el.value;
    }
  }
  fs.writeFile(configPath, JSON.stringify(newConfig, null, 2), (err) => {
    if (err) {
      devStatus.textContent = 'Failed to save: ' + err.message;
      devStatus.className = 'error';
    } else {
      devStatus.textContent = 'Saved!';
      devStatus.className = 'success';
      try {
        window?.require && ipcRenderer.send('config-updated');
      } catch (_) {}
      setTimeout(() => { try { window.close(); } catch (_) {} }, 150);
    }
  });
};

loadConfig();

