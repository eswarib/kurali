const fs = require('fs');
const path = require('path');
const os = require('os');
const { ipcRenderer } = require('electron');
const { ensureUserConfigWritable } = require('./config-helpers');

function getAppImageMountPath() {
  if (process.env.APPIMAGE) {
    return path.join(path.dirname(process.execPath), '..', '..', '..');
  }
  return null;
}

const LEGACY_USER_CONFIG_PATH = path.join(os.homedir(), '.coral', 'conf', 'config.json');
const LEGACY_USER_CONFIG_FLAT = path.join(os.homedir(), '.coral', 'config.json');

function seedUserConfigIfNeeded(userConfigPath, defaultConfigPath) {
  if (fs.existsSync(userConfigPath)) return;
  const dir = path.dirname(userConfigPath);
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
  if (fs.existsSync(LEGACY_USER_CONFIG_PATH)) {
    fs.copyFileSync(LEGACY_USER_CONFIG_PATH, userConfigPath);
    return;
  }
  if (fs.existsSync(LEGACY_USER_CONFIG_FLAT)) {
    fs.copyFileSync(LEGACY_USER_CONFIG_FLAT, userConfigPath);
    return;
  }
  if (defaultConfigPath && fs.existsSync(defaultConfigPath)) {
    fs.copyFileSync(defaultConfigPath, userConfigPath);
  }
}

// Resolve config.json path depending on environment (AppImage vs dev)
let configPath;
if (process.env.APPIMAGE) {
  const appImageMountPath = getAppImageMountPath();
  const defaultConfigPath = path.join(appImageMountPath, 'usr', 'share', 'coral', 'conf', 'config.json');
  const userConfigDir = path.join(os.homedir(), '.kurali');
  const userConfigPath = path.join(userConfigDir, 'conf', 'config.json');
  try {
    seedUserConfigIfNeeded(userConfigPath, defaultConfigPath);
  } catch (e) {
    console.error('Failed to prepare user config:', e.message);
  }
  configPath = userConfigPath;
  ensureUserConfigWritable(configPath);
} else {
  // Development / packaged Windows: use ~/.kurali/conf/config.json; copy from platform config on first run
  const userConfigDir = path.join(os.homedir(), '.kurali');
  configPath = path.join(userConfigDir, 'conf', 'config.json');
  const platformConfig = process.platform === 'win32' ? 'config.json' : 'config-linux.json';
  let devDefault;
  try {
    const { app } = require('electron');
    if (app.isPackaged && process.platform === 'win32') {
      devDefault = path.join(process.resourcesPath, 'conf', 'config.json');
    } else {
      devDefault = path.join(__dirname, '../..', 'coral', 'conf', platformConfig);
    }
  } catch (_) {
    devDefault = path.join(__dirname, '../..', 'coral', 'conf', platformConfig);
  }
  try {
    seedUserConfigIfNeeded(configPath, devDefault);
  } catch (e) { console.error('Failed to prepare user config:', e.message); }
  ensureUserConfigWritable(configPath);
}
const configForm = document.getElementById('configForm');
const saveBtn = document.getElementById('saveBtn');
const defaultBtn = document.getElementById('defaultBtn');
const statusDiv = document.getElementById('status');

loadConfig();

let config = {};
// List of installed models discovered via IPC (filenames + abs paths).
// Populated each time loadConfig runs; used to build the model dropdown and
// to validate the saved value at submit time.
let availableModels = [];

// Convert "ggml-small.en-q8_0.bin" -> "Whisper Small (en, q8_0)" for display.
function modelDisplayName(filename) {
  if (!filename) return '';
  let s = filename.replace(/^ggml-/i, '').replace(/\.bin$/i, '');
  let quant = '';
  const qm = s.match(/-(q[0-9_a-zA-Z]+)$/i);
  if (qm) { quant = qm[1]; s = s.slice(0, qm.index); }
  let lang = '';
  const lm = s.match(/\.([a-z]{2})$/i);
  if (lm) { lang = lm[1]; s = s.slice(0, lm.index); }
  const cap = s ? (s.charAt(0).toUpperCase() + s.slice(1)) : filename;
  const tags = [lang, quant].filter(Boolean);
  const named = tags.length ? `${cap} (${tags.join(', ')})` : cap;
  return `Whisper ${named}`;
}

function formatKeyComboFromEvent(e) {
  const parts = [];
  if (e.ctrlKey) parts.push('ctrl');
  if (e.altKey) parts.push('alt');
  if (e.shiftKey) parts.push('shift');
  if (e.metaKey) parts.push('meta');

  let key = '';
  if (e.code && e.code.startsWith('Key')) {
    key = e.code.slice(3).toLowerCase();
  } else if (e.code && e.code.startsWith('Digit')) {
    key = e.code.slice(5);
  } else if (e.key && e.key.length === 1) {
    key = e.key.toLowerCase();
  } else if (e.key) {
    key = e.key.toLowerCase(); // e.g., 'enter', 'escape', 'space'
  }
  // Avoid pure modifier shortcuts
  const isOnlyModifier = !key || ['control','ctrl','alt','shift','meta','super'].includes(key);
  if (!isOnlyModifier) parts.push(key);
  return parts.join('+');
}

function createField(labelText, inputEl) {
  const row = document.createElement('div');
  row.className = 'field';
  const lbl = document.createElement('div');
  lbl.className = 'field-label';
  lbl.textContent = labelText;
  const wrap = document.createElement('div');
  wrap.className = 'field-input';
  wrap.appendChild(inputEl);
  row.appendChild(lbl);
  row.appendChild(wrap);
  return row;
}

function renderMainForm(cfg) {
  configForm.innerHTML = '';
  const primaryKeys = ['triggerKey', 'cmdTriggerKey', 'triggerMode'];
  const modelKey = 'whisperModelPath';

  // --- Trigger settings card ---
  const triggerLabel = document.createElement('div');
  triggerLabel.className = 'section-label';
  triggerLabel.textContent = 'Trigger';
  configForm.appendChild(triggerLabel);

  const triggerCard = document.createElement('div');
  triggerCard.className = 'card';

  // triggerKey
  const triggerInput = document.createElement('input');
  triggerInput.type = 'text';
  triggerInput.readOnly = true;
  triggerInput.placeholder = 'Click, then press shortcut';
  triggerInput.value = cfg.triggerKey || '';
  triggerInput.name = 'triggerKey';
  triggerInput.addEventListener('keydown', (e) => {
    e.preventDefault();
    e.stopPropagation();
    const combo = formatKeyComboFromEvent(e);
    if (combo) triggerInput.value = combo;
  });
  triggerInput.addEventListener('focus', () => { statusDiv.textContent = 'Press desired shortcut…'; statusDiv.className = ''; });
  triggerInput.addEventListener('blur', () => { statusDiv.textContent = ''; });
  triggerCard.appendChild(createField('Trigger key', triggerInput));

  // cmdTriggerKey
  const cmdInput = document.createElement('input');
  cmdInput.type = 'text';
  cmdInput.readOnly = true;
  cmdInput.placeholder = 'Click, then press shortcut';
  cmdInput.value = cfg.cmdTriggerKey || '';
  cmdInput.name = 'cmdTriggerKey';
  cmdInput.addEventListener('keydown', (e) => {
    e.preventDefault();
    e.stopPropagation();
    const combo = formatKeyComboFromEvent(e);
    if (combo) cmdInput.value = combo;
  });
  cmdInput.addEventListener('focus', () => { statusDiv.textContent = 'Press desired shortcut…'; statusDiv.className = ''; });
  cmdInput.addEventListener('blur', () => { statusDiv.textContent = ''; });
  triggerCard.appendChild(createField('Cmd key', cmdInput));

  // triggerMode
  const modeSelect = document.createElement('select');
  modeSelect.name = 'triggerMode';
  [['pushToTalk', 'Push-to-talk (hold)'], ['continuous', 'Continuous (double-tap)']].forEach(([val, txt]) => {
    const opt = document.createElement('option');
    opt.value = val;
    opt.textContent = txt;
    if ((cfg.triggerMode || 'pushToTalk') === val) opt.selected = true;
    modeSelect.appendChild(opt);
  });
  triggerCard.appendChild(createField('Mode', modeSelect));

  configForm.appendChild(triggerCard);

  // --- Model card ---
  const modelLabel = document.createElement('div');
  modelLabel.className = 'section-label';
  modelLabel.textContent = 'Model';
  configForm.appendChild(modelLabel);

  const modelCard = document.createElement('div');
  modelCard.className = 'card';

  const hidden = document.createElement('input');
  hidden.type = 'hidden';
  hidden.name = modelKey;
  hidden.value = cfg[modelKey] || '';

  const installedFilenames = (availableModels || []).map(m => m.filename);

  // Decide what label to show in the pill given the saved config value.
  const displayForPath = (p) => {
    if (!p) {
      // No selection saved: prefer first installed model, otherwise prompt.
      return installedFilenames.length ? modelDisplayName(installedFilenames[0]) : 'Select model…';
    }
    if (installedFilenames.includes(p)) return modelDisplayName(p);
    // Bare ggml-*.bin filename that isn't installed locally — still show, mark missing.
    if (/^ggml-.*\.bin$/i.test(p) && !p.includes('/') && !p.includes('\\')) {
      return modelDisplayName(p) + ' (missing)';
    }
    // Absolute / relative path supplied via Custom picker.
    try { return path.basename(p) || 'Custom'; } catch (_) { return 'Custom'; }
  };

  const modelWrap = document.createElement('div');
  modelWrap.className = 'model-selector';
  const pill = document.createElement('span');
  pill.className = 'pill-select';
  pill.textContent = displayForPath(cfg[modelKey]);
  const menu = document.createElement('div');
  menu.className = 'dropdown-menu';
  menu.style.display = 'none';

  // Build dropdown from installed models, then append "Custom…" file picker.
  const items = installedFilenames.map(fn => ({
    label: modelDisplayName(fn),
    value: fn,
    isCustom: false,
  }));
  items.push({ label: 'Custom…', value: null, isCustom: true });

  items.forEach(item => {
    const it = document.createElement('div');
    it.className = 'dropdown-item';
    it.textContent = item.label;
    it.onclick = async () => {
      menu.style.display = 'none';
      if (item.isCustom) {
        const fp = await ipcRenderer.invoke('select-model-file');
        if (fp) {
          hidden.value = fp;
          try { pill.textContent = path.basename(fp); } catch (_) { pill.textContent = 'Custom'; }
        }
      } else {
        hidden.value = item.value;
        pill.textContent = item.label;
      }
      requestResize();
    };
    menu.appendChild(it);
  });

  pill.onclick = () => {
    menu.style.display = (menu.style.display === 'none') ? 'block' : 'none';
    requestResize();
  };
  document.addEventListener('click', (e) => {
    if (!modelWrap.contains(e.target)) menu.style.display = 'none';
  });

  modelWrap.appendChild(pill);
  modelWrap.appendChild(menu);
  modelWrap.appendChild(hidden);

  const modelRow = document.createElement('div');
  modelRow.className = 'field';
  const modelLbl = document.createElement('div');
  modelLbl.className = 'field-label';
  modelLbl.textContent = 'Whisper model';
  const modelInputWrap = document.createElement('div');
  modelInputWrap.className = 'field-input';
  modelInputWrap.appendChild(modelWrap);
  modelRow.appendChild(modelLbl);
  modelRow.appendChild(modelInputWrap);
  modelCard.appendChild(modelRow);

  configForm.appendChild(modelCard);

  requestResize();
}

function requestResize() {
  try {
    const measure = () => {
      const bodyH = document.body ? document.body.scrollHeight : 0;
      const docH = document.documentElement ? document.documentElement.scrollHeight : 0;
      const bodyW = document.body ? document.body.scrollWidth : 0;
      const docW = document.documentElement ? document.documentElement.scrollWidth : 0;
      const height = Math.ceil(Math.max(bodyH, docH, 400) + 24);
      const width = Math.ceil(Math.max(bodyW, docW, 480) + 24);
      return { height, width };
    };
    const m = measure();
    ipcRenderer.send('resize-window', m.height, m.width);
    setTimeout(() => { const m2 = measure(); ipcRenderer.send('resize-window', m2.height, m2.width); }, 80);
    setTimeout(() => { const m3 = measure(); ipcRenderer.send('resize-window', m3.height, m3.width); }, 250);
    setTimeout(() => { const m4 = measure(); ipcRenderer.send('resize-window', m4.height, m4.width); }, 600);
  } catch (_) {}
}

function loadConfig() {
  fs.readFile(configPath, 'utf8', async (err, data) => {
    if (err) {
      statusDiv.textContent = 'Failed to load config: ' + err.message;
      statusDiv.className = 'error';
      return;
    }
    try {
      config = JSON.parse(data);
      try {
        availableModels = await ipcRenderer.invoke('list-installed-models');
        if (!Array.isArray(availableModels)) availableModels = [];
      } catch (e) {
        console.warn('list-installed-models failed:', e && e.message);
        availableModels = [];
      }
      renderMainForm(config);
      statusDiv.textContent = '';
      statusDiv.className = '';
    } catch (e) {
      statusDiv.textContent = 'Invalid config.json: ' + e.message;
      statusDiv.className = 'error';
    }
  });
}

saveBtn.onclick = (e) => {
  e.preventDefault();
  const newConfig = { ...config };
  const booleanSelects = new Set();
  for (const el of configForm.elements) {
    if (!el.name) continue;
    if (el.tagName === 'SELECT') {
      if (el.querySelector('option[value="true"]') && el.querySelector('option[value="false"]')) {
        newConfig[el.name] = el.value === 'true';
      } else {
        newConfig[el.name] = el.value;
      }
    } else if (el.type === 'number') {
      newConfig[el.name] = Number(el.value);
    } else {
      newConfig[el.name] = el.value;
    }
  }
  // Fallback: ensure triggerMode is captured (Windows form.elements can omit nested controls)
  const triggerModeEl = configForm.querySelector('[name="triggerMode"]');
  if (triggerModeEl && (triggerModeEl.value === 'pushToTalk' || triggerModeEl.value === 'continuous')) {
    newConfig.triggerMode = triggerModeEl.value;
  }
  // Validate model selection: empty, or a filename present in availableModels,
  // or an absolute/relative path that exists on disk.
  try {
    const modelPath = newConfig['whisperModelPath'];
    const installedNames = (availableModels || []).map(m => m.filename);
    // Fallback set, in case list-installed-models failed earlier.
    const fallback = ['ggml-base.en.bin', 'ggml-small.en.bin', 'ggml-small.en-q8_0.bin'];
    const allowedNames = installedNames.length ? installedNames : fallback;
    const isAllowedToken = !modelPath || allowedNames.includes(modelPath);
    if (!isAllowedToken) {
      if (!fs.existsSync(modelPath) || !fs.statSync(modelPath).isFile()) {
        statusDiv.textContent = 'Please select a valid model file.';
        statusDiv.className = 'error';
        return;
      }
    }
  } catch (_) {}
  try {
    const dir = path.dirname(configPath);
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
  } catch (e) {
    statusDiv.textContent = 'Failed to create config dir: ' + e.message;
    statusDiv.className = 'error';
    return;
  }
  ensureUserConfigWritable(configPath);
  fs.writeFile(configPath, JSON.stringify(newConfig, null, 2), (err) => {
    if (err) {
      statusDiv.textContent = 'Failed to save: ' + err.message;
      statusDiv.className = 'error';
    } else {
      statusDiv.textContent = 'Settings saved!';
      statusDiv.className = 'success';
      try {
        window?.require && ipcRenderer.send('config-updated');
      } catch (_) {}
      setTimeout(() => { try { window.close(); } catch (_) {} }, 400);
    }
  });
};

defaultBtn.onclick = (e) => {
  e.preventDefault();
  try {
    let defaultConfigPath;
    if (process.env.APPIMAGE) {
      const appImageMountPath = getAppImageMountPath();
      defaultConfigPath = path.join(appImageMountPath, 'usr', 'share', 'coral', 'conf', 'config.json');
    } else {
      try {
        const { app } = require('electron');
        if (app.isPackaged && process.platform === 'win32') {
          defaultConfigPath = path.join(process.resourcesPath, 'conf', 'config.json');
        } else {
          const platformConfig = process.platform === 'win32' ? 'config.json' : 'config-linux.json';
          defaultConfigPath = path.join(__dirname, '../..', 'coral', 'conf', platformConfig);
        }
      } catch (_) {
        const platformConfig = process.platform === 'win32' ? 'config.json' : 'config-linux.json';
        defaultConfigPath = path.join(__dirname, '../..', 'coral', 'conf', platformConfig);
      }
    }
    const data = fs.readFileSync(defaultConfigPath, 'utf8');
    config = JSON.parse(data);
    // Write defaults into user config and re-render form
    ensureUserConfigWritable(configPath);
    fs.writeFile(configPath, JSON.stringify(config, null, 2), (err) => {
      if (err) {
        statusDiv.textContent = 'Failed to set defaults: ' + err.message;
      } else {
        renderMainForm(config);
        statusDiv.textContent = 'Defaults applied';
        try { ipcRenderer.send('config-updated'); } catch (_) {}
      }
    });
  } catch (ex) {
    statusDiv.textContent = 'Failed to load defaults: ' + (ex && ex.message ? ex.message : ex);
  }
};
