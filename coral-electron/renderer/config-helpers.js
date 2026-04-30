'use strict';

const fs = require('fs');

/**
 * Windows: bundled config copy can inherit FILE_ATTRIBUTE_READONLY; clear it
 * so Apply / Defaults can write without EPERM.
 */
function ensureUserConfigWritable(p) {
  if (process.platform !== 'win32' || !p) return;
  try {
    if (fs.existsSync(p)) fs.chmodSync(p, 0o666);
  } catch (_) { /* ignore */ }
}

module.exports = { ensureUserConfigWritable };
