const { app, BrowserWindow, dialog, ipcMain, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const https = require('https');

const APP_VERSION = '1.2.2';
const UPDATE_URL = 'https://raytracer-cpp.alexstath.com/version.json';

let mainWindow;

function checkForUpdates() {
  https.get(UPDATE_URL, (res) => {
    let data = '';
    res.on('data', (chunk) => data += chunk);
    res.on('end', () => {
      try {
        const info = JSON.parse(data);
        if (info.version && info.version !== APP_VERSION) {
          mainWindow.webContents.send('update-available', {
            current: APP_VERSION,
            latest: info.version,
            url: info.url,
          });
        }
      } catch (e) { /* ignore parse errors */ }
    });
  }).on('error', () => { /* ignore network errors */ });
}

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

  // Open external links in the user's browser
  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: 'deny' };
  });

  // Check for updates 3 seconds after launch
  mainWindow.webContents.on('did-finish-load', () => {
    setTimeout(checkForUpdates, 3000);
  });
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  app.quit();
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) createWindow();
});

// Open external URL
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

  // Write BMP
  const rowSize = Math.ceil((width * 3) / 4) * 4;
  const imageSize = rowSize * height;
  const fileSize = 54 + imageSize;

  const buffer = Buffer.alloc(fileSize);
  // Header
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
      buffer[dstIdx + 0] = pixelData[srcIdx + 2]; // B
      buffer[dstIdx + 1] = pixelData[srcIdx + 1]; // G
      buffer[dstIdx + 2] = pixelData[srcIdx + 0]; // R
    }
  }

  fs.writeFileSync(result.filePath, buffer);
  return { success: true, path: result.filePath };
});
