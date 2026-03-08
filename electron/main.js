const { app, BrowserWindow, dialog, ipcMain, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const https = require('https');
const { execSync, spawn } = require('child_process');

const APP_VERSION = '1.3.0';
const UPDATE_URL = 'https://raytracer-cpp.alexstath.com/version.json';
const DOWNLOAD_URLS = {
  'darwin-arm64': 'https://raytracer-cpp.alexstath.com/download/RayTracer-mac-apple-silicon.zip',
  'darwin-x64': 'https://raytracer-cpp.alexstath.com/download/RayTracer-mac-intel.zip',
  'linux-x64': 'https://raytracer-cpp.alexstath.com/download/RayTracer-linux.AppImage',
};

let mainWindow;

// ─── Download with progress ──────────────────────────────
function downloadFile(url, destPath, onProgress) {
  return new Promise((resolve, reject) => {
    https.get(url, (res) => {
      // Follow redirects
      if (res.statusCode === 301 || res.statusCode === 302) {
        return downloadFile(res.headers.location, destPath, onProgress).then(resolve).catch(reject);
      }
      if (res.statusCode !== 200) {
        return reject(new Error('Download failed: HTTP ' + res.statusCode));
      }
      const totalSize = parseInt(res.headers['content-length'], 10);
      let downloaded = 0;
      const file = fs.createWriteStream(destPath);
      res.on('data', (chunk) => {
        downloaded += chunk.length;
        if (onProgress && totalSize) onProgress(Math.round(downloaded / totalSize * 100));
      });
      res.pipe(file);
      file.on('finish', () => { file.close(resolve); });
      file.on('error', reject);
    }).on('error', reject);
  });
}

// ─── Auto-update ──────────────────────────────────────────
async function performUpdate(sendStatus) {
  const platform = process.platform;
  const arch = process.arch;
  const key = platform + '-' + arch;
  const downloadUrl = DOWNLOAD_URLS[key];

  if (!downloadUrl) throw new Error('No update available for ' + key);

  const tempDir = path.join(app.getPath('temp'), 'raytracer-update-' + Date.now());
  fs.mkdirSync(tempDir, { recursive: true });

  if (platform === 'darwin') {
    // ─── macOS update ─────────────────────────────────
    const zipPath = path.join(tempDir, 'update.zip');

    sendStatus('downloading');
    await downloadFile(downloadUrl, zipPath, (pct) => {
      mainWindow.webContents.send('update-download-progress', pct);
    });

    sendStatus('installing');

    // Extract zip
    const extractDir = path.join(tempDir, 'extracted');
    fs.mkdirSync(extractDir);
    execSync(`ditto -xk "${zipPath}" "${extractDir}"`);

    // Find the .app bundle
    const entries = fs.readdirSync(extractDir);
    const newAppName = entries.find(f => f.endsWith('.app'));
    if (!newAppName) throw new Error('No .app found in update');
    const newAppPath = path.join(extractDir, newAppName);

    // Current app path (go up from .../Contents/MacOS/binary to .app)
    const exePath = process.execPath;
    const appIdx = exePath.indexOf('.app');
    if (appIdx === -1) throw new Error('Cannot determine app path');
    const currentAppPath = exePath.substring(0, appIdx + 4);

    // Remove quarantine from new app
    execSync(`xattr -cr "${newAppPath}"`);

    // Create a script that waits for us to quit, swaps, and relaunches
    const scriptPath = path.join(tempDir, 'update.sh');
    fs.writeFileSync(scriptPath, `#!/bin/bash
# Wait for the app to quit
sleep 2
# Replace old app with new
rm -rf "${currentAppPath}"
mv "${newAppPath}" "${currentAppPath}"
# Relaunch
open "${currentAppPath}"
# Clean up temp
rm -rf "${tempDir}"
`);
    fs.chmodSync(scriptPath, '0755');

    // Launch the updater script and quit
    spawn('bash', [scriptPath], { detached: true, stdio: 'ignore' }).unref();
    app.quit();

  } else if (platform === 'linux') {
    // ─── Linux AppImage update ────────────────────────
    const currentPath = process.execPath;
    const newPath = path.join(tempDir, 'RayTracer.AppImage');

    sendStatus('downloading');
    await downloadFile(downloadUrl, newPath, (pct) => {
      mainWindow.webContents.send('update-download-progress', pct);
    });

    sendStatus('installing');
    fs.chmodSync(newPath, '0755');

    // Script to replace and relaunch
    const scriptPath = path.join(tempDir, 'update.sh');
    fs.writeFileSync(scriptPath, `#!/bin/bash
sleep 2
cp "${newPath}" "${currentPath}"
chmod +x "${currentPath}"
"${currentPath}" &
rm -rf "${tempDir}"
`);
    fs.chmodSync(scriptPath, '0755');

    spawn('bash', [scriptPath], { detached: true, stdio: 'ignore' }).unref();
    app.quit();
  }
}

// ─── Window ───────────────────────────────────────────────
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1280,
    height: 820,
    minWidth: 900,
    minHeight: 600,
    backgroundColor: '#0a0a0f',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
    titleBarStyle: process.platform === 'darwin' ? 'hiddenInset' : 'default',
    title: 'Ray Tracer',
  });

  mainWindow.loadFile(path.join(__dirname, 'renderer', 'index.html'));
  mainWindow.setMenuBarVisibility(false);

  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: 'deny' };
  });
}

app.whenReady().then(createWindow);
app.on('window-all-closed', () => app.quit());
app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) createWindow();
});

// ─── IPC Handlers ─────────────────────────────────────────
ipcMain.handle('check-for-update', async () => {
  return new Promise((resolve) => {
    https.get(UPDATE_URL + '?t=' + Date.now(), (res) => {
      let data = '';
      res.on('data', (chunk) => data += chunk);
      res.on('end', () => {
        try {
          const info = JSON.parse(data);
          resolve({
            currentVersion: APP_VERSION,
            latestVersion: info.version,
            hasUpdate: info.version !== APP_VERSION,
          });
        } catch (e) { resolve({ error: 'Failed to parse update info' }); }
      });
    }).on('error', (e) => resolve({ error: 'Network error: ' + e.message }));
  });
});

ipcMain.handle('install-update', async () => {
  try {
    await performUpdate((status) => {
      mainWindow.webContents.send('update-status', status);
    });
  } catch (e) {
    return { error: e.message };
  }
});

ipcMain.handle('open-external', async (event, url) => {
  await shell.openExternal(url);
});

// Save file dialog
ipcMain.handle('save-file', async (event, { defaultName, pixels, width, height }) => {
  const result = await dialog.showSaveDialog(mainWindow, {
    title: 'Save Render',
    defaultPath: defaultName,
    filters: [
      { name: 'BMP Image', extensions: ['bmp'] },
      { name: 'All Files', extensions: ['*'] },
    ],
  });

  if (result.canceled) return { success: false };

  const rowSize = Math.ceil((width * 3) / 4) * 4;
  const imageSize = rowSize * height;
  const fileSize = 54 + imageSize;

  const buffer = Buffer.alloc(fileSize);
  buffer.write('BM', 0);
  buffer.writeInt32LE(fileSize, 2);
  buffer.writeInt32LE(54, 10);
  buffer.writeInt32LE(40, 14);
  buffer.writeInt32LE(width, 18);
  buffer.writeInt32LE(height, 22);
  buffer.writeInt16LE(1, 26);
  buffer.writeInt16LE(24, 28);
  buffer.writeInt32LE(imageSize, 34);

  const pixelData = Buffer.from(pixels);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const srcIdx = (y * width + x) * 4;
      const dstIdx = 54 + y * rowSize + x * 3;
      buffer[dstIdx + 0] = pixelData[srcIdx + 2];
      buffer[dstIdx + 1] = pixelData[srcIdx + 1];
      buffer[dstIdx + 2] = pixelData[srcIdx + 0];
    }
  }

  fs.writeFileSync(result.filePath, buffer);
  return { success: true, path: result.filePath };
});
